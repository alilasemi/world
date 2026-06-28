#include "sim_config.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Trim leading/trailing ASCII whitespace.
std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// Strip an optional pair of surrounding quotes.
std::string unquote(const std::string& s) {
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

// Parse an inline list "[a, b, c]" into a vector of floats.
std::vector<float> parse_float_list(const std::string& value) {
    std::vector<float> out;
    std::string inner = trim(value);
    if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']') {
        inner = inner.substr(1, inner.size() - 2);
    }
    std::stringstream ss(inner);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const std::string t = trim(item);
        if (!t.empty()) {
            out.push_back(std::stof(t));
        }
    }
    return out;
}

void warn_unknown(const std::string& section, const std::string& key) {
    std::fprintf(stderr, "load_config: ignoring unknown key '%s.%s'\n",
            section.c_str(), key.c_str());
}

// Assign a single "section.key = value" into the config. Unknown keys warn and
// are skipped, so a config with extra/typo'd keys still loads the rest.
void apply_kv(SimConfig& cfg, const std::string& section, const std::string& key,
        const std::string& value) {
    const auto f = [&value]() { return std::stof(value); };
    const auto i = [&value]() { return std::stoi(value); };
    const auto b = [&value]() { return value == "true"; };

    if (section == "simulation") {
        if (key == "dt") cfg.dt = f();
        else if (key == "grid_size") cfg.grid_size = i();
        else if (key == "particles_per_cell") cfg.particles_per_cell = i();
        else if (key == "force_grid_size") cfg.force_grid_size = i();
        else if (key == "threads_per_block") cfg.threads_per_block = i();
        else warn_unknown(section, key);
    } else if (section == "domain") {
        if (key == "x_min") cfg.domain.x_min = f();
        else if (key == "x_max") cfg.domain.x_max = f();
        else if (key == "y_min") cfg.domain.y_min = f();
        else if (key == "y_max") cfg.domain.y_max = f();
        else warn_unknown(section, key);
    } else if (section == "physics") {
        if (key == "gravity") cfg.physics.gravity = f();
        else if (key == "particle_radius") cfg.physics.particle_radius = f();
        else if (key == "max_force") cfg.physics.max_force = f();
        else if (key == "floor_y") cfg.physics.floor_y = f();
        else if (key == "ceiling_y") cfg.physics.ceiling_y = f();
        else if (key == "left_wall_x") cfg.physics.left_wall_x = f();
        else if (key == "right_wall_x") cfg.physics.right_wall_x = f();
        else warn_unknown(section, key);
    } else if (section == "materials") {
        if (key == "masses") cfg.masses = parse_float_list(value);
        else warn_unknown(section, key);
    } else if (section == "initialization") {
        if (key == "type") cfg.init_type = unquote(value);
        else if (key == "x0") cfg.init_x0 = f();
        else if (key == "y0") cfg.init_y0 = f();
        else if (key == "cube_length") cfg.cube_length = f();
        else if (key == "sled_x") cfg.sled_x = f();
        else if (key == "sled_y") cfg.sled_y = f();
        else if (key == "sled_vx") cfg.sled_vx = f();
        else if (key == "sled_vy") cfg.sled_vy = f();
        else if (key == "sled_material") cfg.sled_material = i();
        else if (key == "two_particle_separation") cfg.two_particle_separation = f();
        else if (key == "particle_material") cfg.particle_material = i();
        else warn_unknown(section, key);
    } else if (section == "rendering") {
        if (key == "num_triangles") cfg.num_triangles = i();
        else warn_unknown(section, key);
    } else if (section == "drivers") {
        if (key == "port") cfg.port = i();
        else if (key == "steps_per_frame") cfg.steps_per_frame = i();
        else if (key == "profiling_outer_iters") cfg.profiling_outer_iters = i();
        else if (key == "profiling_steps_per_iter") cfg.profiling_steps_per_iter = i();
        else if (key == "stability_sim_time") cfg.stability_sim_time = f();
        else if (key == "stability_dt_sweep") cfg.stability_dt_sweep = parse_float_list(value);
        else if (key == "kernel_timing") cfg.kernel_timing = b();
        else warn_unknown(section, key);
    } else if (section == "time_integration") {
        if (key == "solver") cfg.time_integrator = value;
        else if (key == "picard_iterations") cfg.picard_iterations = i();
        else warn_unknown(section, key);
    } else if (section == "rl") {
        if (key == "max_force") cfg.rl_max_force = f();
        else warn_unknown(section, key);
    } else {
        warn_unknown(section, key);
    }
}

}  // namespace

SimConfig load_config(const std::string& path) {
    SimConfig cfg;  // starts from defaults; only present keys override

    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr,
                "load_config: could not open '%s'; using built-in defaults.\n",
                path.c_str());
        return cfg;
    }

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        // Strip comments (no '#' appears inside the values this config uses).
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            std::fprintf(stderr, "load_config: skipping malformed line '%s'\n", line.c_str());
            continue;
        }

        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));

        if (value.empty()) {
            // A bare "name:" introduces a new section.
            section = key;
        } else {
            apply_kv(cfg, section, key, value);
        }
    }

    return cfg;
}
