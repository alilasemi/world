#include <stdio.h>
#include <assert.h>
#include <cmath>
#include <chrono>
#include "particle_dynamics.h"
#include "cuda_check.h"


ParticleDynamics::ParticleDynamics(const SimConfig& config_) : config(config_) {
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);

    // Pull the scalar grid/timestep settings out of the config.
    collision_grid_size_x = config.collision_grid_size_x;
    collision_grid_size_y = config.collision_grid_size_y;
    particles_per_cell = config.particles_per_cell;
    force_grid_size_x = config.force_grid_size_x;
    force_grid_size_y = config.force_grid_size_y;
    occupancy_grid_size_x = config.occupancy_grid_size_x;
    occupancy_grid_size_y = config.occupancy_grid_size_y;
    dt = config.dt;

    // Initialize on host according to the configured initialization type.
    if (config.init_type == "two_particles") {
        initialize_to_two_particles(config.init_x0, config.init_y0);
    } else if (config.init_type == "single_particle") {
        initialize_to_single_particle(config.init_x0, config.init_y0);
    } else {
        initialize_to_cube(config.init_x0, config.init_y0);
    }
    // Copy to device
    device_state.copy_from_host(host_state);
    device_material.copy_from_host(host_material);
    unpack_state();

    // 9 = 3x3 stencil cell count; FindNeighborsKernel owns the dense
    // collision grid privately and writes results as a flat n*9*k neighbor array.
    device_neighbors = DeviceVector<int>(static_cast<size_t>(n) * 9 * static_cast<size_t>(particles_per_cell));

    time = 0.0f;
    last_time = 0.0f;
    last_wall_clock_time = std::chrono::steady_clock::now();
    real_time_ratio = 0.0f;
    // Per-material masses come from the config (index = material id; e.g.
    // 0 = walls, 1 = snow, 2 = sled).
    const int num_materials = static_cast<int>(config.masses.size());
    host_mass = HostVector<float>(num_materials);
    device_mass = DeviceVector<float>(num_materials);
    for (int i = 0; i < num_materials; ++i) {
        host_mass[i] = config.masses[i];
    }
    device_mass.copy_from_host(host_mass);

    // force_grid_size_x*force_grid_size_y grid of externally-supplied (e.g.
    // AI) body forces, plus the occupancy snapshot fed back to that external
    // model. Independent of the collision grid above.
    device_state_n = DeviceVector<float>(4 * n);
    device_body_force_x = DeviceVector<float>(n);
    device_body_force_y = DeviceVector<float>(n);
    device_grid_force_x = DeviceVector<float>(force_grid_size_x * force_grid_size_y);
    device_grid_force_y = DeviceVector<float>(force_grid_size_x * force_grid_size_y);
    // Nothing else initializes these, and InterpolateForceKernel reads them
    // every step starting from the very first take_step() call -- zero is
    // also the correct "no AI model configured yet" default.
    CUDA_CHECK(cudaMemset(device_grid_force_x.data(), 0,
            static_cast<size_t>(force_grid_size_x) * static_cast<size_t>(force_grid_size_y) * sizeof(float)));
    CUDA_CHECK(cudaMemset(device_grid_force_y.data(), 0,
            static_cast<size_t>(force_grid_size_x) * static_cast<size_t>(force_grid_size_y) * sizeof(float)));
    device_occupancy_grid = DeviceVector<int>(occupancy_grid_size_x * occupancy_grid_size_y);

    // Create CUDA kernels
    const bool kt = config.kernel_timing;
    find_neighbors_kernel = std::make_unique<FindNeighborsKernel>(
            device_state.data(), n, collision_grid_size_x, collision_grid_size_y, particles_per_cell,
            config.domain, config.threads_per_block, device_neighbors.data(), kt);
    compute_rhs_kernel = std::make_unique<ComputeRHSKernel>(device_state.data(), device_material.data(),
            device_mass.data(), device_neighbors.data(),
            device_body_force_x.data(), device_body_force_y.data(),
            n, particles_per_cell, config.physics, config.threads_per_block, device_rhs.data(), kt);
    semi_implicit_euler_kernel = std::make_unique<SemiImplicitEulerKernel>(device_state.data(),
            device_rhs.data(), n, dt, config.threads_per_block, kt);
    backward_euler_picard_kernel = std::make_unique<BackwardEulerPicardKernel>(device_state.data(),
            device_state_n.data(), device_rhs.data(), n, dt, config.threads_per_block, kt);

    device_energy = DeviceVector<float>(1);
    host_energy = HostVector<float>(1);
    energy_kernel = std::make_unique<EnergyKernel>(device_state.data(), device_material.data(),
            device_mass.data(), n, config.physics, config.threads_per_block, device_energy.data(), kt);

    interpolate_force_kernel = std::make_unique<InterpolateForceKernel>(device_state.data(),
            device_grid_force_x.data(), device_grid_force_y.data(), n, force_grid_size_x, force_grid_size_y,
            config.domain, config.threads_per_block, device_body_force_x.data(), device_body_force_y.data(), kt);
    occupancy_grid_kernel = std::make_unique<OccupancyGridKernel>(device_occupancy_grid.data(),
            device_state.data(), n, occupancy_grid_size_x, occupancy_grid_size_y, config.domain,
            config.threads_per_block, kt);
}


void ParticleDynamics::resize(const int new_n) {
    n = new_n;
    xy = std::vector<float>(2*n);

    host_state = HostVector<float>(4 * n);
    device_state = DeviceVector<float>(4 * n);
    device_rhs = DeviceVector<float>(4 * n);
    host_material = HostVector<int>(n);
    device_material = DeviceVector<int>(n);
}


void ParticleDynamics::unpack_state() {
    time_unpack_state = 0.f;
    start_timer();

    CUDA_CHECK(cudaDeviceSynchronize());

    host_state.copy_from_device(device_state);

    for (size_t i = 0; i < n; ++i) {
//        printf("Particle %lu: host_state = (%.2f, %.2f, %.2f, %.2f)\n", i, host_state[4 * i + 0], host_state[4 * i + 1], host_state[4 * i + 2], host_state[4 * i + 3]);
        xy[2*i + 0] = host_state[4 * i + 0];
        xy[2*i + 1] = host_state[4 * i + 1];
    }
    stop_timer(time_unpack_state);
    real_time_ratio = get_real_time_ratio();
}



void ParticleDynamics::initialize_to_two_particles(const float x0, const float y0) {
    const float separation = config.two_particle_separation;
    printf("Initializing to two particles at (%.2f, %.2f) and (%.2f, %.2f)...\n", x0, y0, x0, y0 + separation);
    resize(2);

    printf("Resized to %d particles.\n", n);
    host_state[0] = x0;
    host_state[1] = y0;
    host_state[2] = 0.0f;
    host_state[3] = 0.0f;

    printf("Initialized first particle at (%.2f, %.2f).\n", host_state[0], host_state[1]);
    host_state[4] = x0;
    host_state[5] = y0 + separation;
    host_state[6] = 0.0f;
    host_state[7] = 0.0f;

    host_material[0] = config.particle_material;
    host_material[1] = config.particle_material;

    printf("Initialized second particle at (%.2f, %.2f).\n", host_state[4],
            host_state[5]);
}


void ParticleDynamics::initialize_to_single_particle(const float x0, const float y0) {
    const float vx0 = config.init_vx0;
    const float vy0 = config.init_vy0;
    printf("Initializing to a single particle at (%.2f, %.2f) with velocity (%.2f, %.2f)...\n",
            x0, y0, vx0, vy0);
    resize(1);

    host_state[0] = x0;
    host_state[1] = y0;
    host_state[2] = vx0;
    host_state[3] = vy0;

    host_material[0] = config.particle_material;
}


void ParticleDynamics::initialize_to_cube(const float x0, const float y0) {
    const float length_x = config.cube_length_x;
    const float length_y = config.cube_length_y;
    // Spacing uses the same radius as the collision physics so the initial
    // packing matches the contact model (deduped from compute_rhs_kernel).
    const float radius = config.physics.particle_radius;
    int num_per_side_x = static_cast<int>(length_x / (2 * radius));
    int num_per_side_y = static_cast<int>(length_y / (2 * radius));
    resize(num_per_side_x * num_per_side_y + 1);

    for (size_t i = 0; i < num_per_side_x; ++i) {
        for (size_t j = 0; j < num_per_side_y; ++j) {
            host_state[4 * (i * num_per_side_y + j) + 0] = x0 + i * 2 * 1.01*radius;
            host_state[4 * (i * num_per_side_y + j) + 1] = y0 + j * 2 * 1.01*radius;
            host_state[4 * (i * num_per_side_y + j) + 2] = 0.0f;
            host_state[4 * (i * num_per_side_y + j) + 3] = 0.0f;
        }
    }

    // Make one particle of sled on top
    host_state[4 * (n - 1) + 0] = config.sled_x;
    host_state[4 * (n - 1) + 1] = config.sled_y;
    host_state[4 * (n - 1) + 2] = config.sled_vx;
    host_state[4 * (n - 1) + 3] = config.sled_vy;

    // Set all particles to the snow material except for the sled particle.
    for (size_t i = 0; i < n - 1; ++i) {
        host_material[i] = config.particle_material;
    }
    host_material[n - 1] = config.sled_material;
}


void ParticleDynamics::start_timer() {
    cudaEventRecord(start_event);
}

void ParticleDynamics::stop_timer(float& elapsed_time) {
    cudaEventRecord(stop_event);
    cudaEventSynchronize(stop_event);
    float time = 0.f;
    cudaEventElapsedTime(&time, start_event, stop_event);
    elapsed_time += time;
}


float ParticleDynamics::get_real_time_ratio() {
    auto now = std::chrono::steady_clock::now();
    double wall_delta = std::chrono::duration<double>(now - last_wall_clock_time).count();
    float ratio = 0.0f;
    if (wall_delta > 0.0) {
        ratio = (time - last_time) / static_cast<float>(wall_delta);
    }
    last_time = time;
    last_wall_clock_time = now;
    return ratio;
}


void ParticleDynamics::take_step() {
    if (config.time_integrator == "semi_implicit_euler") {
        (*find_neighbors_kernel)();
        (*interpolate_force_kernel)();
        (*compute_rhs_kernel)();
        semi_implicit_euler_kernel->set_dt(dt);
        (*semi_implicit_euler_kernel)();
    } else {
        CUDA_CHECK(cudaMemcpy(device_state_n.data(), device_state.data(),
                4 * n * sizeof(float), cudaMemcpyDeviceToDevice));
        for (int k = 0; k < config.picard_iterations; ++k) {
            (*find_neighbors_kernel)();
            (*interpolate_force_kernel)();
            (*compute_rhs_kernel)();
            backward_euler_picard_kernel->set_dt(dt);
            (*backward_euler_picard_kernel)();
        }
    }
    time += dt;
}

float ParticleDynamics::compute_total_energy() {
    (*energy_kernel)();
    host_energy.copy_from_device(device_energy);
    return host_energy[0];
}


bool ParticleDynamics::is_stable() {
    host_state.copy_from_device(device_state);
    const DomainParams& d = config.domain;
    for (int i = 0; i < n; ++i) {
        const float x = host_state[4 * i + 0];
        const float y = host_state[4 * i + 1];
        if (!std::isfinite(x) || !std::isfinite(y) ||
                x < d.x_min || x > d.x_max || y < d.y_min || y > d.y_max) {
            return false;
        }
    }
    return true;
}


float ParticleDynamics::compute_max_acceleration() {
    HostVector<float> host_rhs(device_rhs.size());
    host_rhs.copy_from_device(device_rhs);
    float max_accel = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float ax = host_rhs[4 * i + 2];
        const float ay = host_rhs[4 * i + 3];
        const float accel = std::sqrt(ax * ax + ay * ay);
        if (accel > max_accel) {
            max_accel = accel;
        }
    }
    return max_accel;
}


void ParticleDynamics::update_occupancy_grid() {
    (*occupancy_grid_kernel)();
}


int ParticleDynamics::grid_overflow_count() const { return find_neighbors_kernel->overflow_count(); }


float ParticleDynamics::find_neighbors_wct()    const { return find_neighbors_kernel->wall_clock_time(); }
float ParticleDynamics::interpolate_force_wct() const { return interpolate_force_kernel->wall_clock_time(); }
float ParticleDynamics::compute_rhs_wct()       const { return compute_rhs_kernel->wall_clock_time(); }
float ParticleDynamics::take_step_wct()         const {
    if (config.time_integrator == "semi_implicit_euler") {
        return semi_implicit_euler_kernel->wall_clock_time();
    }
    return backward_euler_picard_kernel->wall_clock_time();
}

ParticleDynamics::~ParticleDynamics() = default;
