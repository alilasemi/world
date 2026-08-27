// Runs one small, hand-picked simulation (a "model problem"), optionally
// swept over dt x max_force x restitution -- the same "sweep:" config
// section stability_and_accuracy uses for the first two axes, plus a third
// (restitution) that's model_problem-only -- and records every single
// step's full state for every combo. Unlike stability_and_accuracy, which
// only reports a final pass/fail verdict, this keeps the whole trajectory
// so it can be plotted and inspected at a granular level. No WebSocket, no
// subprocess isolation: one process, one history file (all combos, as
// gnuplot-"index"-separated blocks), one plot (one line per combo). See
// model_problems/ for the config files this is meant to be pointed at.

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "particle_dynamics.h"
#include "sim_config.h"

namespace {

// All outputs (history data, gnuplot script, PNG) always land here,
// regardless of which config file was used to drive the run.
const char* kOutputDir = "model_problems";

void ensure_output_dir_exists() {
    if (mkdir(kOutputDir, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        std::exit(1);
    }
}

// One step's full state: simulated time plus every particle's
// (x, y, z, vx, vy, vz), in the same flat order as ParticleDynamics::host_state.
struct HistoryRow {
    float time;
    std::vector<float> state;  // kStateStride*n floats
};

// One (dt, max_force, restitution) combo's full run.
struct ComboHistory {
    float dt;
    float max_force;
    float restitution;
    std::vector<HistoryRow> rows;
};

// Runs one (dt, max_force, restitution) combo to config.model_problem_sim_time,
// recording every single step's full state. A full device->host sync every
// single step -- deliberately not batched or made infrequent (unlike
// stability_and_accuracy / broadcast.cpp's overflow checks), since "save
// every iteration's state" is exactly the point of this driver. Fine
// because model problems are small and short by design.
ComboHistory run_combo(const SimConfig& config, float dt, float max_force, float restitution) {
    SimConfig combo_config = config;
    combo_config.physics.max_force = max_force;
    combo_config.physics.restitution = restitution;
    ParticleDynamics sim(combo_config);
    sim.dt = dt;

    const int num_steps = static_cast<int>(std::lround(
            static_cast<double>(config.model_problem_sim_time) / static_cast<double>(dt)));

    ComboHistory combo;
    combo.dt = dt;
    combo.max_force = max_force;
    combo.restitution = restitution;
    combo.rows.reserve(static_cast<std::size_t>(num_steps) + 1);

    // t=0 row: the constructor already ran unpack_state() once, so
    // host_state already reflects the initial condition.
    combo.rows.push_back({sim.time,
            std::vector<float>(sim.host_state.data(),
                    sim.host_state.data() + kStateStride * sim.n)});

    for (int step = 0; step < num_steps; ++step) {
        sim.take_step();
        sim.unpack_state();
        combo.rows.push_back({sim.time,
                std::vector<float>(sim.host_state.data(),
                    sim.host_state.data() + kStateStride * sim.n)});
    }

    // Unlike stability_and_accuracy/broadcast.cpp, an overflowing combo just
    // warns and keeps whatever (diverged) data it recorded rather than
    // exit(1)-ing -- there's no subprocess isolation here, so aborting would
    // also kill every other combo in the sweep.
    const int overflowed = sim.grid_overflow_count();
    if (overflowed > 0) {
        std::fprintf(stderr,
                "model_problem: dt=%.6g max_force=%.6g restitution=%.6g -- spatial grid "
                "overflowed particles_per_cell capacity in %d cell-step(s); simulation has "
                "diverged.\n",
                static_cast<double>(dt), static_cast<double>(max_force),
                static_cast<double>(restitution), overflowed);
    }

    return combo;
}

// Writes every combo's history into one file as blank-line-separated blocks
// so write_and_run_gnuplot_script() can plot each combo as its own line via
// gnuplot's "index" mechanism.
void write_history(const std::string& path, int n, const std::vector<ComboHistory>& combos) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "Could not open '%s' for writing.\n", path.c_str());
        return;
    }
    fprintf(f, "# step time");
    for (int i = 0; i < n; ++i) {
        fprintf(f, " x%d y%d z%d vx%d vy%d vz%d", i, i, i, i, i, i);
    }
    fprintf(f, "\n");
    for (std::size_t c = 0; c < combos.size(); ++c) {
        const ComboHistory& combo = combos[c];
        fprintf(f, "# combo %zu: dt=%.9g max_force=%.9g restitution=%.9g\n", c,
                static_cast<double>(combo.dt), static_cast<double>(combo.max_force),
                static_cast<double>(combo.restitution));
        for (std::size_t step = 0; step < combo.rows.size(); ++step) {
            const HistoryRow& row = combo.rows[step];
            fprintf(f, "%zu %.9g", step, static_cast<double>(row.time));
            for (float v : row.state) {
                fprintf(f, " %.9g", static_cast<double>(v));
            }
            fprintf(f, "\n");
        }
        if (c + 1 < combos.size()) {
            // Two blank lines: gnuplot's actual "index" dataset separator --
            // a single blank line only marks a discontinuity within the same
            // index (verified directly against gnuplot 6.0; one blank line
            // between blocks silently produces "no valid points" for every
            // index past the first).
            fprintf(f, "\n\n");
        }
    }
    fclose(f);
}

// A handful of visually distinct colors, cycled if there are more combos
// than colors. First entry is "purple" so a single-combo (unswept) run looks
// exactly like it did before this driver supported sweeping; the rest are
// matplotlib's "tab10" palette minus red, which is reserved for the
// floor-contact reference line below.
const char* kPalette[] = {
    "purple", "#1f77b4", "#ff7f0e", "#2ca02c", "#8c564b",
    "#e377c2", "#7f7f7f", "#bcbd22", "#17becf", "#000000",
};
constexpr std::size_t kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);

// Plots time (column 2) vs. particle 0's z-position (column 5 -- z is the
// gravity axis, and each row is step time x0 y0 z0 vx0 vy0 vz0) for every
// combo as its own colored, legended line -- the sensible default for a
// model problem, which by design has only a particle or two -- with a
// purple/colored dot at every recorded row (i.e. every single timestep) so
// the timestep size is visible directly on the trajectory, plus a red
// dashed reference line at z = floor_z + particle_radius, the height at
// which particle 0's surface just touches the floor. Same
// system("gnuplot ...") idiom as stability_and_accuracy.cpp: a nonzero exit
// just warns, it doesn't fail the run (the history data is what matters
// most and is already written).
void write_and_run_gnuplot_script(const std::string& script_path, const std::string& png_path,
        const std::string& data_path, const std::vector<ComboHistory>& combos, float contact_z) {
    FILE* f = fopen(script_path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "Could not open '%s' for writing; skipping plot.\n", script_path.c_str());
        return;
    }
    // noenhanced on the terminal (rather than per-command) so legend titles
    // containing "max_force" don't get their underscore read as a subscript.
    fprintf(f, "set terminal pngcairo size 1000,750 noenhanced\n");
    fprintf(f, "set output \"%s\"\n", png_path.c_str());
    fprintf(f, "set title \"Particle 0 trajectory\"\n");
    fprintf(f, "set xlabel \"time (s)\"\n");
    fprintf(f, "set ylabel \"z position\"\n");
    fprintf(f, "set key outside right\n");

    fprintf(f, "plot ");
    for (std::size_t c = 0; c < combos.size(); ++c) {
        const ComboHistory& combo = combos[c];
        fprintf(f, "%s\"%s\" index %zu using 2:5 with linespoints lw 2 lc rgb \"%s\" pt 7 ps 0.5 "
                "title \"dt=%.4g, max_force=%.4g, e=%.3g\", \\\n",
                c == 0 ? "" : "     ", data_path.c_str(), c, kPalette[c % kPaletteSize],
                static_cast<double>(combo.dt), static_cast<double>(combo.max_force),
                static_cast<double>(combo.restitution));
    }
    fprintf(f, "     %.9g with lines lc rgb \"red\" dashtype 2 lw 2 title \"floor contact\"\n",
            static_cast<double>(contact_z));
    fclose(f);

    const std::string cmd = "gnuplot \"" + script_path + "\"";
    if (std::system(cmd.c_str()) != 0) {
        std::fprintf(stderr, "Warning: gnuplot invocation failed; see '%s' to run it manually.\n",
                script_path.c_str());
    } else {
        printf("Wrote plot to '%s'.\n", png_path.c_str());
    }
}

}  // namespace

int main(int argc, char** argv) {
    // Defaults to model_problems/single_particle.yaml (rather than the
    // repo-root config.yaml) since that's the config meant for this driver.
    const std::string config_path = (argc > 1) ? argv[1] : "model_problems/single_particle.yaml";
    const SimConfig config = load_config(config_path);

    // An empty sweep_dt/sweep_max_force/sweep_restitution (absent from the
    // config's "sweep:" section) degenerates that axis to a single value at
    // the config's own simulation.dt / physics.max_force / physics.restitution
    // -- i.e. omitting "sweep:" entirely is exactly the old single-run
    // behavior.
    const std::vector<float> dts = config.sweep_dt.empty()
            ? std::vector<float>{config.dt} : config.sweep_dt;
    const std::vector<float> max_forces = config.sweep_max_force.empty()
            ? std::vector<float>{config.physics.max_force} : config.sweep_max_force;
    const std::vector<float> restitutions = config.sweep_restitution.empty()
            ? std::vector<float>{config.physics.restitution} : config.sweep_restitution;

    std::vector<ComboHistory> combos;
    combos.reserve(dts.size() * max_forces.size() * restitutions.size());
    for (float dt : dts) {
        for (float max_force : max_forces) {
            for (float restitution : restitutions) {
                combos.push_back(run_combo(config, dt, max_force, restitution));
            }
        }
    }

    ensure_output_dir_exists();
    const std::string data_path = std::string(kOutputDir) + "/history.dat";
    const std::string script_path = std::string(kOutputDir) + "/trajectory_plot.gp";
    const std::string png_path = std::string(kOutputDir) + "/trajectory_plot.png";

    const int n = static_cast<int>(combos[0].rows[0].state.size() / kStateStride);
    write_history(data_path, n, combos);
    const float contact_z = config.physics.floor_z + config.physics.particle_radius;
    write_and_run_gnuplot_script(script_path, png_path, data_path, combos, contact_z);

    std::size_t total_rows = 0;
    for (const ComboHistory& combo : combos) {
        total_rows += combo.rows.size();
    }
    printf("Ran %zu combo(s), %zu total step(s), to '%s'.\n", combos.size(), total_rows, data_path.c_str());

    return 0;
}
