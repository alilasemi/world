// build/bin/dataset -- generates the training set for the outcome surrogate.
//
// One rollout per row of a design CSV (see surrogate/make_design.py): apply the
// row's throw/material parameters on top of a base config, run the blob, and
// record the coarse density/momentum latent at several checkpoints along the
// way. Deliberately has NO WebSocket in the loop: the browser protocol is a
// great demo path and a terrible data-generation path.
//
// Two design choices worth stating:
//
// * COMMON RANDOM NUMBERS. Every rollout uses the same init_seed, so the base
//   packing and jitter pattern are identical across the whole design and the
//   only thing varying is the parameters. This is the cheapest variance
//   reduction available for the downstream sensitivity estimates -- differences
//   between rollouts are attributable to theta rather than to a different
//   random packing.
//
// * A DIVERGED ROLLOUT DOES NOT KILL THE RUN. Unlike broadcast.cpp and
//   stability_and_accuracy.cpp, which exit(1) on grid overflow, this driver
//   records the failure in the manifest, zero-fills that rollout's grids, and
//   continues. Losing a whole design to one bad parameter combination hours in
//   would be far worse than carrying a few flagged rows -- and the analysis side
//   needs to know which rows to drop anyway, so the status column is load-bearing.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "particle_dynamics.h"
#include "sim_config.h"

namespace {

constexpr const char* kMagic = "RTPGRD01";

struct Design {
    std::vector<std::string> names;
    std::vector<std::vector<float>> rows;
};

// Strip surrounding whitespace and any trailing carriage return. Needed because
// Python's csv.writer emits CRLF line endings by default, and std::getline only
// consumes the '\n' -- leaving a '\r' glued to the last field of every line,
// which silently turns "friction" into "friction\r" and fails name matching.
std::string clean_cell(const std::string& cell) {
    const std::size_t first = cell.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = cell.find_last_not_of(" \t\r\n");
    return cell.substr(first, last - first + 1);
}

Design read_design(const std::string& path) {
    Design design;
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr,
                "dataset: could not open design '%s'.\n"
                "Generate one first:  surrogate/.venv/bin/python surrogate/make_design.py\n",
                path.c_str());
        std::exit(1);
    }
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cell;
        if (header) {
            while (std::getline(ss, cell, ',')) design.names.push_back(clean_cell(cell));
            header = false;
            continue;
        }
        std::vector<float> row;
        while (std::getline(ss, cell, ',')) row.push_back(std::stof(clean_cell(cell)));
        if (row.size() != design.names.size()) {
            std::fprintf(stderr, "dataset: design row has %zu values, expected %zu\n",
                    row.size(), design.names.size());
            std::exit(1);
        }
        design.rows.push_back(row);
    }
    return design;
}

// Map one design row onto a copy of the base config. Unknown column names are a
// hard error rather than a warning: silently ignoring a parameter would produce
// a dataset whose manifest claims a variable was swept when it was not, which is
// the kind of error that survives all the way into a fitted model.
SimConfig config_for_row(const SimConfig& base, const Design& design,
        const std::vector<float>& row) {
    SimConfig config = base;
    for (size_t c = 0; c < design.names.size(); ++c) {
        const std::string& name = design.names[c];
        const float value = row[c];
        if (name == "throw_vx") config.init_vx0 = value;
        else if (name == "throw_vy") config.init_vy0 = value;
        else if (name == "throw_vz") config.init_vz0 = value;
        else if (name == "release_z") config.init_z0 = value;
        else if (name == "grain_mass") {
            // Blob mass is varied through the per-grain mass so the particle
            // COUNT stays fixed across the design -- a varying n would change
            // the state size and the latent's normalization from row to row.
            for (size_t m = 1; m < config.masses.size(); ++m) config.masses[m] = value;
        }
        else if (name == "restitution") config.physics.restitution = value;
        else if (name == "friction") config.physics.friction = value;
        else {
            std::fprintf(stderr, "dataset: design column '%s' is not a known parameter\n",
                    name.c_str());
            std::exit(1);
        }
    }
    return config;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "dataset/blob_throw.yaml";
    const std::string design_path = (argc > 2) ? argv[2] : "dataset/design.csv";
    // Output directory is an argument so that separate experiments -- a
    // terminal-state set and a finer-in-time dynamics set -- can coexist
    // without overwriting each other.
    const std::string output_dir = (argc > 3) ? argv[3] : "dataset";
    const char* kOutputDir = output_dir.c_str();
    const SimConfig base = load_config(config_path);
    const Design design = read_design(design_path);

    const int num_rollouts = static_cast<int>(design.rows.size());
    const int num_checkpoints = base.dataset_checkpoints;
    if (num_rollouts == 0 || num_checkpoints <= 0) {
        std::fprintf(stderr, "dataset: nothing to do (%d rollouts, %d checkpoints)\n",
                num_rollouts, num_checkpoints);
        return 1;
    }

    // Checkpoint times are evenly spaced and INCLUDE the end of the rollout, so
    // checkpoint k is at (k+1)/K * sim_time. t=0 is excluded because the initial
    // condition is fully determined by theta and carries no information.
    std::vector<float> checkpoint_times(num_checkpoints);
    for (int k = 0; k < num_checkpoints; ++k) {
        checkpoint_times[k] = base.dataset_sim_time * static_cast<float>(k + 1)
                / static_cast<float>(num_checkpoints);
    }

    std::string mkdir_cmd = std::string("mkdir -p ") + kOutputDir;
    if (std::system(mkdir_cmd.c_str()) != 0) {
        std::fprintf(stderr, "dataset: could not create '%s'\n", kOutputDir);
        return 1;
    }

    // Probe the grid shape once from a throwaway sim, so the header can be
    // written before any rollout runs.
    int nx = base.density_grid_size_x, ny = base.density_grid_size_y, nz = base.density_grid_size_z;
    const int channels = DensityGridKernel::kChannels;
    const size_t floats_per_snapshot =
            static_cast<size_t>(channels) * nx * ny * nz;

    const std::string grid_path = std::string(kOutputDir) + "/grids.bin";
    std::FILE* grids = std::fopen(grid_path.c_str(), "wb");
    if (!grids) {
        std::fprintf(stderr, "dataset: could not open '%s' for writing\n", grid_path.c_str());
        return 1;
    }
    // Self-describing header so the Python side needs no out-of-band shape info.
    std::fwrite(kMagic, 1, 8, grids);
    const int32_t header[7] = {num_rollouts, num_checkpoints, channels, nx, ny, nz,
                               static_cast<int32_t>(design.names.size())};
    std::fwrite(header, sizeof(int32_t), 7, grids);
    std::fwrite(checkpoint_times.data(), sizeof(float), checkpoint_times.size(), grids);

    const std::string manifest_path = std::string(kOutputDir) + "/manifest.csv";
    std::FILE* manifest = std::fopen(manifest_path.c_str(), "w");
    if (!manifest) {
        std::fprintf(stderr, "dataset: could not open '%s' for writing\n", manifest_path.c_str());
        return 1;
    }
    std::fprintf(manifest, "rollout");
    for (const std::string& name : design.names) std::fprintf(manifest, ",%s", name.c_str());
    std::fprintf(manifest, ",status,final_time,overflow_cells,deposited_mass\n");

    std::vector<float> zeros(floats_per_snapshot, 0.0f);
    int num_ok = 0, num_diverged = 0;

    for (int r = 0; r < num_rollouts; ++r) {
        const SimConfig config = config_for_row(base, design, design.rows[r]);
        ParticleDynamics sim(config);
        const int steps_per_checkpoint = std::max(1,
                static_cast<int>(base.dataset_sim_time
                        / (static_cast<float>(num_checkpoints) * config.dt) + 0.5f));

        bool diverged = false;
        int overflow = 0;
        double deposited = 0.0;
        for (int k = 0; k < num_checkpoints; ++k) {
            if (!diverged) {
                for (int step = 0; step < steps_per_checkpoint; ++step) sim.take_step();
                // Polled once per checkpoint, not per step: a host sync every
                // step would serialize otherwise-async kernel launches (same
                // reasoning as broadcast.cpp's "run" handler).
                overflow = sim.grid_overflow_count();
                if (overflow > 0 || !sim.is_stable()) diverged = true;
            }
            if (diverged) {
                std::fwrite(zeros.data(), sizeof(float), zeros.size(), grids);
                continue;
            }
            sim.compute_density_grid();
            std::fwrite(sim.host_density_grid.data(), sizeof(float),
                    sim.host_density_grid.size(), grids);
            if (k == num_checkpoints - 1) {
                for (size_t node = 0; node < sim.density_grid_nodes(); ++node) {
                    deposited += sim.host_density_grid[node];
                }
            }
        }

        std::fprintf(manifest, "%d", r);
        for (float value : design.rows[r]) std::fprintf(manifest, ",%.9g", value);
        std::fprintf(manifest, ",%s,%.9g,%d,%.9g\n", diverged ? "diverged" : "ok",
                static_cast<double>(sim.time), overflow, deposited);
        std::fflush(manifest);

        if (diverged) ++num_diverged; else ++num_ok;
        // Progress on one line as it goes, so a long run is observable.
        std::printf("\rrollout %d/%d  ok=%d diverged=%d", r + 1, num_rollouts, num_ok, num_diverged);
        std::fflush(stdout);
    }
    std::printf("\n");

    std::fclose(grids);
    std::fclose(manifest);
    std::printf("Wrote %s (%d rollouts x %d checkpoints x %d channels x %dx%dx%d)\n",
            grid_path.c_str(), num_rollouts, num_checkpoints, channels, nx, ny, nz);
    std::printf("Wrote %s (%d ok, %d diverged)\n", manifest_path.c_str(), num_ok, num_diverged);
    return 0;
}
