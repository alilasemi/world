#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "particle_dynamics.h"
#include "sim_config.h"

namespace {

// Runs a single dt to t = config.stability_sim_time and writes
// "<energy> <stable 0/1>" to out_path. Runs in a child process -- if a CUDA
// error aborts the process (e.g. FindNeighborsKernel's grid-overflow assert
// under a too-large dt), only this child dies, not the whole sweep.
void run_child(float dt, const char* out_path, const SimConfig& config) {
    ParticleDynamics sim(config);
    sim.dt = dt;

    const int num_steps = static_cast<int>(std::lround(
            static_cast<double>(config.stability_sim_time) / static_cast<double>(dt)));

    bool stable = true;
    for (int step = 0; step < num_steps; ++step) {
        sim.take_step();
        stable = sim.is_stable();
        if (!stable) {
            break;
        }
    }

    // Still report energy even when unstable -- it's informative (shows how
    // badly it diverged) and naturally comes out NaN/Inf if the state blew up.
    const float energy = sim.compute_total_energy();

    FILE* out = fopen(out_path, "w");
    if (!out) {
        std::exit(1);
    }
    fprintf(out, "%.9g %d\n", static_cast<double>(energy), stable ? 1 : 0);
    fclose(out);
}

struct Result {
    float dt;
    float energy = 0.0f;
    bool stable = false;
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

// Forks and re-execs self in child mode for a single dt value, so that a
// CUDA-fatal abort() in the child cannot bring down the rest of the sweep.
// Must be called before this process ever touches CUDA (fork-after-CUDA-init
// is unsupported) -- the parent never constructs a ParticleDynamics.
Result run_dt_in_subprocess(const std::string& self_path, float dt, const std::string& config_path) {
    Result result;
    result.dt = dt;

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

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        std::exit(1);
    }

    if (pid == 0) {
        execl(self_path.c_str(), self_path.c_str(), "--dt", dt_buf, "--out", out_path.c_str(),
                "--config", config_path.c_str(), nullptr);
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
    int stable_flag = 0;
    if (!in || std::fscanf(in, "%f %d", &result.energy, &stable_flag) != 2) {
        result.crashed = true;
    } else {
        result.stable = (stable_flag != 0);
    }
    if (in) {
        fclose(in);
    }
    std::remove(out_path.c_str());
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    // Child mode: run a single dt and report its result to a file.
    if (argc == 7 && std::strcmp(argv[1], "--dt") == 0 && std::strcmp(argv[3], "--out") == 0 &&
            std::strcmp(argv[5], "--config") == 0) {
        const SimConfig config = load_config(argv[6]);
        run_child(std::strtof(argv[2], nullptr), argv[4], config);
        return 0;
    }

    // Parent mode: sweep every dt, each isolated in its own subprocess. The
    // parent never touches CUDA (fork-after-CUDA-init is unsupported); it only
    // reads the config to learn the sweep list, then re-execs itself per dt.
    const std::string config_path = (argc > 1) ? argv[1] : "config.yaml";
    const SimConfig config = load_config(config_path);
    const std::string self_path = resolve_self_path(argv[0]);
    const std::vector<float> dts = config.stability_dt_sweep;

    std::vector<Result> results;
    for (float dt : dts) {
        results.push_back(run_dt_in_subprocess(self_path, dt, config_path));
    }

    printf("%-10s %-20s %-10s\n", "dt", "energy", "stable");
    for (const Result& r : results) {
        if (r.crashed) {
            printf("%-10g %-20s %-10s\n", static_cast<double>(r.dt), "N/A (crashed)", "false");
        } else {
            printf("%-10g %-20.9g %-10s\n", static_cast<double>(r.dt), static_cast<double>(r.energy),
                    r.stable ? "true" : "false");
        }
    }

    return 0;
}
