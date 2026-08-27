// build/bin/chaos -- measures what is predictable about a granular interaction.
//
// Runs an ensemble of E realizations that share one parameter vector theta and
// differ only by a random displacement of magnitude
// initialization.perturbation (a fraction of the particle radius) applied to
// every grain's initial position. Member 0 is the unperturbed reference. At each
// checkpoint it reports, for every other member, how far it has diverged from
// the reference in two very different senses:
//
//   PER-GRAIN   root-mean-square displacement between corresponding grains,
//               sqrt( mean_i |x_i^(e) - x_i^(ref)|^2 ). For a chaotic system
//               this grows exponentially, ~exp(lambda t), until it saturates at
//               the scale of the assembly.
//
//   FIELD       relative l2 difference of the interpolated mass field,
//               ||rho^(e) - rho^(ref)|| / ||rho^(ref)||, plus the displacement
//               of the field's center of mass. These are coarse-grained
//               functionals, and the claim under test is that they stay bounded
//               and small while the per-grain measure saturates.
//
// The gap between those two curves is the entire argument for predicting a
// coarse field rather than a trajectory: it separates "the estimator is
// hopeless" from "the quantity is meaningless". It also supplies the number
// without which no surrogate error can be interpreted -- run with a
// perturbation comparable to initialization.jitter and the resulting field
// scatter is the irreducible floor a surrogate conditioned only on theta cannot
// beat.
//
// Members are run sequentially. The reference trajectory's checkpoint states are
// held in host memory (n_p * d floats per checkpoint) and each perturbed member
// is compared against them as it runs, so only one simulation exists at a time.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "particle_dynamics.h"
#include "sim_config.h"

namespace {

constexpr const char* kOutputDir = "chaos";

// Mass channel only; the momentum field is vacuous once the assembly settles.
double field_relative_difference(const std::vector<float>& reference,
        const HostVector<float>& current, size_t nodes) {
    double numerator = 0.0, denominator = 0.0;
    for (size_t node = 0; node < nodes; ++node) {
        const double difference = static_cast<double>(current[node]) - reference[node];
        numerator += difference * difference;
        denominator += reference[node] * reference[node];
    }
    return (denominator > 0.0) ? std::sqrt(numerator / denominator) : 0.0;
}

void center_of_mass(const HostVector<float>& grid, size_t nodes, int nx, int ny, int nz,
        double* out) {
    double total = 0.0;
    out[0] = out[1] = out[2] = 0.0;
    for (int ix = 0; ix < nx; ++ix) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                const size_t node = static_cast<size_t>((ix * ny + iy) * nz + iz);
                const double mass = grid[node];
                total += mass;
                out[0] += mass * ix;
                out[1] += mass * iy;
                out[2] += mass * iz;
            }
        }
    }
    if (total > 0.0) {
        for (int a = 0; a < 3; ++a) out[a] /= total;
    }
    (void)nodes;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "chaos/ensemble.yaml";
    const int num_members = (argc > 2) ? std::atoi(argv[2]) : 16;
    const SimConfig base = load_config(config_path);

    if (base.init_perturbation <= 0.0f) {
        std::fprintf(stderr,
                "chaos: initialization.perturbation is 0, so every member would be "
                "identical.\nSet it to the perturbation magnitude you want to study.\n");
        return 1;
    }

    const int num_checkpoints = base.dataset_checkpoints;
    const float total_time = base.dataset_sim_time;
    std::vector<float> checkpoint_times(num_checkpoints);
    for (int k = 0; k < num_checkpoints; ++k) {
        checkpoint_times[k] = total_time * static_cast<float>(k + 1)
                / static_cast<float>(num_checkpoints);
    }

    if (std::system((std::string("mkdir -p ") + kOutputDir).c_str()) != 0) {
        std::fprintf(stderr, "chaos: could not create '%s'\n", kOutputDir);
        return 1;
    }

    // ---- reference member: record positions and mass field at each checkpoint ----
    std::vector<std::vector<float>> reference_positions(num_checkpoints);
    std::vector<std::vector<float>> reference_fields(num_checkpoints);
    size_t nodes = 0;
    int nx = 0, ny = 0, nz = 0, num_particles = 0;
    int steps_per_checkpoint = 0;
    {
        SimConfig config = base;
        config.init_perturbation = 0.0f;   // member 0 is unperturbed
        ParticleDynamics sim(config);
        num_particles = sim.n;
        nodes = sim.density_grid_nodes();
        nx = sim.density_grid_size_x; ny = sim.density_grid_size_y; nz = sim.density_grid_size_z;
        steps_per_checkpoint = std::max(1, static_cast<int>(
                total_time / (static_cast<float>(num_checkpoints) * config.dt) + 0.5f));
        for (int k = 0; k < num_checkpoints; ++k) {
            for (int step = 0; step < steps_per_checkpoint; ++step) sim.take_step();
            sim.unpack_state();
            reference_positions[k] = sim.positions;
            sim.compute_density_grid();
            reference_fields[k].assign(sim.host_density_grid.data(),
                    sim.host_density_grid.data() + nodes);
        }
        std::printf("reference: %d grains, %d checkpoints, %d steps each, grid %dx%dx%d\n",
                num_particles, num_checkpoints, steps_per_checkpoint, nx, ny, nz);
    }

    const std::string path = std::string(kOutputDir) + "/divergence.csv";
    std::FILE* out = std::fopen(path.c_str(), "w");
    if (!out) {
        std::fprintf(stderr, "chaos: could not open '%s'\n", path.c_str());
        return 1;
    }
    std::fprintf(out, "member,checkpoint,time,grain_rms_displacement,"
                      "field_relative_difference,com_displacement\n");

    // Also dump every member's terminal mass field. The pairwise difference
    // between two realizations is NOT the error of predicting their mean: if
    // s = mu + n with independent zero-mean n, then ||s_a - s_b|| ~ sqrt(2)||n||
    // while a model predicting mu incurs only ||n||. Writing the fields lets the
    // scatter about the ensemble mean be measured directly instead of inferred
    // through that factor, which is the quantity a surrogate error must be
    // compared against.
    const std::string fields_path = std::string(kOutputDir) + "/final_fields.bin";
    std::FILE* fields = std::fopen(fields_path.c_str(), "wb");
    if (!fields) {
        std::fprintf(stderr, "chaos: could not open '%s'\n", fields_path.c_str());
        return 1;
    }
    {
        const int32_t header[2] = {num_members, static_cast<int32_t>(nodes)};
        std::fwrite(header, sizeof(int32_t), 2, fields);
        std::fwrite(reference_fields[num_checkpoints - 1].data(), sizeof(float), nodes, fields);
    }

    // ---- perturbed members ----
    for (int member = 1; member < num_members; ++member) {
        SimConfig config = base;
        config.init_perturbation_seed = static_cast<unsigned int>(1000 + member);
        ParticleDynamics sim(config);
        if (sim.n != num_particles) {
            std::fprintf(stderr, "chaos: member %d has %d grains, reference had %d\n",
                    member, sim.n, num_particles);
            return 1;
        }
        for (int k = 0; k < num_checkpoints; ++k) {
            for (int step = 0; step < steps_per_checkpoint; ++step) sim.take_step();
            sim.unpack_state();

            // Per-grain separation. Grain ordering is fixed across members
            // (same lattice construction), so index i refers to the same grain.
            double sum_squared = 0.0;
            for (int i = 0; i < num_particles; ++i) {
                double squared = 0.0;
                for (int a = 0; a < kDim; ++a) {
                    const double difference = static_cast<double>(sim.positions[kDim * i + a])
                            - reference_positions[k][kDim * i + a];
                    squared += difference * difference;
                }
                sum_squared += squared;
            }
            const double grain_rms = std::sqrt(sum_squared / num_particles);

            sim.compute_density_grid();
            const double field_difference =
                    field_relative_difference(reference_fields[k], sim.host_density_grid, nodes);

            double com_member[3], com_reference[3];
            center_of_mass(sim.host_density_grid, nodes, nx, ny, nz, com_member);
            HostVector<float> reference_wrapper(nodes);
            for (size_t node = 0; node < nodes; ++node) {
                reference_wrapper[node] = reference_fields[k][node];
            }
            center_of_mass(reference_wrapper, nodes, nx, ny, nz, com_reference);
            double com_displacement = 0.0;
            for (int a = 0; a < 3; ++a) {
                const double difference = com_member[a] - com_reference[a];
                com_displacement += difference * difference;
            }
            com_displacement = std::sqrt(com_displacement);

            std::fprintf(out, "%d,%d,%.9g,%.9g,%.9g,%.9g\n", member, k,
                    static_cast<double>(checkpoint_times[k]), grain_rms,
                    field_difference, com_displacement);
            if (k == num_checkpoints - 1) {
                std::fwrite(sim.host_density_grid.data(), sizeof(float), nodes, fields);
            }
        }
        std::fflush(out);
        std::printf("\rmember %d/%d", member, num_members - 1);
        std::fflush(stdout);
    }
    std::printf("\n");
    std::fclose(out);
    std::fclose(fields);
    std::printf("Wrote %s (%d perturbed members, perturbation = %.3g x radius)\n",
            path.c_str(), num_members - 1, static_cast<double>(base.init_perturbation));
    return 0;
}
