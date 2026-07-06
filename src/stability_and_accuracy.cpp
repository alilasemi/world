#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "particle_dynamics.h"
#include "sim_config.h"

namespace {

// All sweep outputs (data file, gnuplot script, PNG) always land here,
// regardless of which config file was used to drive the run.
const char* kOutputDir = "stability";

// Outcome of a single (dt, max_force) run. In "fixed_time" mode only
// UNSTABLE/STEADY are possible (STEADY meaning "finished the fixed sim_time
// without going unstable"). In "steady_state" mode, TIMED_OUT additionally
// covers runs that stayed in-bounds but never dropped below the acceleration
// cutoff within the sim_time cap.
enum class Status : int { UNSTABLE = 0, STEADY = 1, TIMED_OUT = 2 };

const char* status_letter(Status s) {
    switch (s) {
        case Status::UNSTABLE:  return "U";
        case Status::STEADY:    return "S";
        case Status::TIMED_OUT: return "T";
    }
    return "?";
}

// Prints one clean message and exits if the spatial grid has ever overflowed
// particles_per_cell during this run -- instead of letting the old device
// assert print once per dropped particle (potentially thousands of lines)
// before CUDA_CHECK aborts. Still exits (rather than trying to recover the
// combo), so the parent's existing crashed-child detection still classifies
// it UNSTABLE, same as before -- just quietly.
void check_grid_overflow(const ParticleDynamics& sim, float dt, float max_force) {
    const int overflowed = sim.grid_overflow_count();
    if (overflowed > 0) {
        std::fprintf(stderr,
                "stability_and_accuracy: dt=%.6g max_force=%.6g -- spatial grid overflowed "
                "particles_per_cell capacity in %d cell-step(s); simulation has diverged. Exiting.\n",
                static_cast<double>(dt), static_cast<double>(max_force), overflowed);
        std::exit(1);
    }
}

// Runs a single (dt, max_force) combo to config.stability_sim_time (or until
// it settles, in steady_state mode) and writes "<energy> <status>" to
// out_path. Runs in a child process -- if it exits non-zero (e.g. via
// check_grid_overflow() above), only this child dies, not the whole sweep.
//
// Whether the run is "unstable" is decided once, from the final state after
// the loop below -- not from checking every intermediate step. A transient
// excursion out of the domain mid-run (which the sim's penalty forces can
// often recover from) shouldn't fail a combo that ends up fine; only where
// it actually lands at t_max matters. Grid overflow is checked at the same
// (infrequent) cadence as the steady_state accel check, plus once at the
// end -- not every step, since a host sync every step would serialize what
// would otherwise be async-queued kernel launches.
void run_child(float dt, float max_force, const char* out_path, SimConfig config) {
    config.physics.max_force = max_force;
    ParticleDynamics sim(config);
    sim.dt = dt;

    const int num_steps = static_cast<int>(std::lround(
            static_cast<double>(config.stability_sim_time) / static_cast<double>(dt)));
    const bool steady_state_mode = (config.stability_mode == "steady_state");
    const float accel_cutoff = config.physics.gravity * config.stability_acceleration_cutoff;

    bool settled_early = false;
    for (int step = 0; step < num_steps; ++step) {
        sim.take_step();
        if (steady_state_mode && (step + 1) % config.stability_steps_per_steadiness_check == 0) {
            check_grid_overflow(sim, dt, max_force);
            if (sim.compute_max_acceleration() < accel_cutoff) {
                settled_early = true;
                break;
            }
        }
    }
    check_grid_overflow(sim, dt, max_force);

    // Final-state check takes priority: if it ends up out of the domain or
    // non-finite, that's UNSTABLE regardless of what settled_early said.
    Status status;
    if (!sim.is_stable()) {
        status = Status::UNSTABLE;
    } else if (steady_state_mode) {
        status = settled_early ? Status::STEADY : Status::TIMED_OUT;
    } else {
        status = Status::STEADY;
    }

    // Still report energy even when unstable -- it's informative (shows how
    // badly it diverged) and naturally comes out NaN/Inf if the state blew up.
    const float energy = sim.compute_total_energy();

    FILE* out = fopen(out_path, "w");
    if (!out) {
        std::exit(1);
    }
    fprintf(out, "%.9g %d\n", static_cast<double>(energy), static_cast<int>(status));
    fclose(out);
}

struct Result {
    float dt = 0.0f;
    float max_force = 0.0f;
    float energy = 0.0f;
    Status status = Status::UNSTABLE;
    bool crashed = false;
};

std::string resolve_self_path(const char* argv0) {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return std::string(argv0);
}

// Forks and re-execs self in child mode for a single (dt, max_force) pair, so
// that a CUDA-fatal abort() in the child cannot bring down the rest of the
// sweep. Must be called before this process ever touches CUDA (fork-after-
// CUDA-init is unsupported) -- the parent never constructs a ParticleDynamics.
Result run_combo_in_subprocess(const std::string& self_path, float dt, float max_force,
        const std::string& config_path) {
    Result result;
    result.dt = dt;
    result.max_force = max_force;

    char out_template[] = "/tmp/stability_and_accuracy_XXXXXX";
    int fd = mkstemp(out_template);
    if (fd < 0) {
        perror("mkstemp");
        std::exit(1);
    }
    close(fd);
    const std::string out_path = out_template;

    char dt_buf[64];
    snprintf(dt_buf, sizeof(dt_buf), "%.9g", static_cast<double>(dt));
    char max_force_buf[64];
    snprintf(max_force_buf, sizeof(max_force_buf), "%.9g", static_cast<double>(max_force));

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        std::exit(1);
    }

    if (pid == 0) {
        // Redirect stderr to /dev/null so a crashing child's error output
        // (e.g. check_grid_overflow()'s message, or a rarer CUDA_CHECK
        // abort) doesn't interleave with the live-updating status table on
        // stdout. dup2 happens before execl so it carries over into the
        // re-exec'd child-mode process. The table's 'X' for this cell and
        // stability_results.dat already carry the machine-readable outcome.
        FILE* devnull = fopen("/dev/null", "w");
        if (devnull) {
            dup2(fileno(devnull), STDERR_FILENO);
        }
        execl(self_path.c_str(), self_path.c_str(), "--dt", dt_buf, "--max_force", max_force_buf,
                "--out", out_path.c_str(), "--config", config_path.c_str(), nullptr);
        perror("execl");
        std::exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        result.crashed = true;
        std::remove(out_path.c_str());
        return result;
    }

    FILE* in = fopen(out_path.c_str(), "r");
    int status_flag = 0;
    if (!in || std::fscanf(in, "%f %d", &result.energy, &status_flag) != 2) {
        result.crashed = true;
    } else {
        result.status = static_cast<Status>(status_flag);
    }
    if (in) {
        fclose(in);
    }
    std::remove(out_path.c_str());
    return result;
}

// Ensures kOutputDir exists so sweep outputs always have somewhere to land,
// even if the config passed in lives elsewhere (e.g. the repo-root config.yaml).
void ensure_output_dir_exists() {
    if (mkdir(kOutputDir, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        std::exit(1);
    }
}

// Effective (possibly crashed-overridden) status, used everywhere a Result
// is reported (live table, gnuplot, stability_results.dat): a crashed child
// -- most commonly a grid overflow (see check_grid_overflow()) -- is folded
// into UNSTABLE rather than shown as a distinct outcome. particles_per_cell
// is set generously for this driver specifically, so an overflow means the
// sim diverged badly enough to matter, not that the config is merely
// under-provisioned.
Status effective_status(const Result& r) {
    return r.crashed ? Status::UNSTABLE : r.status;
}

// Prints the legend and column header up front; cells are filled in live as
// the sweep runs (see main()) so progress is visible on a slow sweep rather
// than only appearing once everything has finished.
void print_table_header(const std::vector<float>& max_forces) {
    printf("Legend: U = unstable, S = steady, T = timed out (bounded, never settled)\n\n");
    printf("%-12s", "dt \\ max_f");
    for (float mf : max_forces) {
        printf("%-10g", static_cast<double>(mf));
    }
    printf("\n");
    std::fflush(stdout);
}

// Writes a small gnuplot script that renders the sweep as a grid of filled
// rectangles (one per (dt, max_force) cell), colored red/green/yellow by
// status. Cells are placed on an integer index grid rather than at their
// actual dt/max_force values, since the swept values are not evenly spaced;
// tic labels map each index back to its real value.
void write_gnuplot_script(const std::string& script_path, const std::string& png_path,
        const std::vector<float>& dts, const std::vector<float>& max_forces,
        const std::vector<std::vector<Result>>& results) {
    FILE* f = fopen(script_path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "Could not open '%s' for writing; skipping plot.\n", script_path.c_str());
        return;
    }

    fprintf(f, "set terminal pngcairo size 900,700\n");
    fprintf(f, "set output \"%s\"\n", png_path.c_str());
    fprintf(f, "set title \"Stability sweep: dt vs max_force\" noenhanced\n");
    fprintf(f, "set xlabel \"dt\" noenhanced\n");
    fprintf(f, "set ylabel \"max_force\" noenhanced\n");
    fprintf(f, "unset key\n");
    fprintf(f, "set xrange [%g:%g]\n", -0.5, static_cast<double>(dts.size()) - 0.5);
    fprintf(f, "set yrange [%g:%g]\n", -0.5, static_cast<double>(max_forces.size()) - 0.5);

    fprintf(f, "set xtics (");
    for (std::size_t i = 0; i < dts.size(); ++i) {
        fprintf(f, "%s\"%g\" %zu", i ? ", " : "", static_cast<double>(dts[i]), i);
    }
    fprintf(f, ") rotate by -45\n");

    fprintf(f, "set ytics (");
    for (std::size_t j = 0; j < max_forces.size(); ++j) {
        fprintf(f, "%s\"%g\" %zu", j ? ", " : "", static_cast<double>(max_forces[j]), j);
    }
    fprintf(f, ")\n");

    int obj_id = 1;
    for (std::size_t i = 0; i < dts.size(); ++i) {
        for (std::size_t j = 0; j < max_forces.size(); ++j) {
            const char* color = "red";
            switch (effective_status(results[i][j])) {
                case Status::STEADY:    color = "green";  break;
                case Status::TIMED_OUT: color = "yellow"; break;
                case Status::UNSTABLE:  color = "red";    break;
            }
            fprintf(f, "set object %d rectangle from %g,%g to %g,%g fc rgb \"%s\" fs solid 1.0 noborder\n",
                    obj_id++, static_cast<double>(i) - 0.5, static_cast<double>(j) - 0.5,
                    static_cast<double>(i) + 0.5, static_cast<double>(j) + 0.5, color);
        }
    }

    // Objects only render alongside an actual plot command; NaN draws nothing
    // but still sets up the plot area.
    fprintf(f, "plot NaN notitle\n");
    fclose(f);
}

void write_results_dat(const std::string& path, const std::vector<float>& dts,
        const std::vector<float>& max_forces, const std::vector<std::vector<Result>>& results) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "Could not open '%s' for writing.\n", path.c_str());
        return;
    }
    fprintf(f, "# dt max_force status(0=unstable,1=steady,2=timed_out) energy\n");
    for (std::size_t i = 0; i < dts.size(); ++i) {
        for (std::size_t j = 0; j < max_forces.size(); ++j) {
            const Result& r = results[i][j];
            fprintf(f, "%.9g %.9g %d %.9g\n", static_cast<double>(dts[i]), static_cast<double>(max_forces[j]),
                    static_cast<int>(effective_status(r)), static_cast<double>(r.energy));
        }
    }
    fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    // Child mode: run a single (dt, max_force) combo and report its result to a file.
    if (argc == 9 && std::strcmp(argv[1], "--dt") == 0 && std::strcmp(argv[3], "--max_force") == 0 &&
            std::strcmp(argv[5], "--out") == 0 && std::strcmp(argv[7], "--config") == 0) {
        const SimConfig config = load_config(argv[8]);
        run_child(std::strtof(argv[2], nullptr), std::strtof(argv[4], nullptr), argv[6], config);
        return 0;
    }

    // Parent mode: sweep every (dt, max_force) pair, each isolated in its own
    // subprocess. The parent never touches CUDA (fork-after-CUDA-init is
    // unsupported); it only reads the config to learn the sweep lists, then
    // re-execs itself per combo. Defaults to stability/config.yaml (rather
    // than the repo-root config.yaml) since that's the config meant for this
    // sweep -- the root configs no longer carry a stability: section.
    const std::string config_path = (argc > 1) ? argv[1] : "stability/config.yaml";
    const SimConfig config = load_config(config_path);
    const std::string self_path = resolve_self_path(argv[0]);
    // An empty sweep_dt/sweep_max_force (no "sweep:" section in the config)
    // degenerates to a 1x1 sweep at the config's own simulation.dt /
    // physics.max_force.
    const std::vector<float> dts = config.sweep_dt.empty()
            ? std::vector<float>{config.dt} : config.sweep_dt;
    const std::vector<float> max_forces = config.sweep_max_force.empty()
            ? std::vector<float>{config.physics.max_force} : config.sweep_max_force;

    print_table_header(max_forces);

    // Filled in and printed one cell at a time as each combo finishes, so a
    // slow sweep shows progress instead of going silent until it's all done.
    std::vector<std::vector<Result>> results(dts.size(), std::vector<Result>(max_forces.size()));
    for (std::size_t i = 0; i < dts.size(); ++i) {
        printf("%-12g", static_cast<double>(dts[i]));
        std::fflush(stdout);
        for (std::size_t j = 0; j < max_forces.size(); ++j) {
            results[i][j] = run_combo_in_subprocess(self_path, dts[i], max_forces[j], config_path);
            printf("%-10s", status_letter(effective_status(results[i][j])));
            std::fflush(stdout);
        }
        printf("\n");
    }
    printf("\n");

    ensure_output_dir_exists();
    const std::string data_path = std::string(kOutputDir) + "/stability_results.dat";
    const std::string script_path = std::string(kOutputDir) + "/stability_plot.gp";
    const std::string png_path = std::string(kOutputDir) + "/stability_plot.png";

    write_results_dat(data_path, dts, max_forces, results);
    write_gnuplot_script(script_path, png_path, dts, max_forces, results);

    const std::string plot_cmd = "gnuplot \"" + script_path + "\"";
    if (std::system(plot_cmd.c_str()) != 0) {
        std::fprintf(stderr, "Warning: gnuplot invocation failed; see '%s' to run it manually.\n",
                script_path.c_str());
    } else {
        printf("Wrote sweep data to '%s' and plot to '%s'.\n", data_path.c_str(), png_path.c_str());
    }

    return 0;
}
