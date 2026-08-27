#include <stdio.h>
#include <assert.h>
#include <cmath>
#include <chrono>
#include <random>
#include "particle_dynamics.h"
#include "cuda_check.h"


ParticleDynamics::ParticleDynamics(const SimConfig& config_) : config(config_) {
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);

    // Pull the scalar grid/timestep settings out of the config.
    collision_grid_size_x = config.collision_grid_size_x;
    collision_grid_size_y = config.collision_grid_size_y;
    collision_grid_size_z = config.collision_grid_size_z;
    particles_per_cell = config.particles_per_cell;
    dt = config.dt;

    // Initialize on host according to the configured initialization type.
    if (config.init_type == "two_particles") {
        initialize_to_two_particles(config.init_x0, config.init_y0);
    } else if (config.init_type == "single_particle") {
        initialize_to_single_particle(config.init_x0, config.init_y0, config.init_z0);
    } else {
        initialize_to_cube(config.init_x0, config.init_y0, config.init_z0);
    }
    // Copy to device
    device_state.copy_from_host(host_state);
    device_material.copy_from_host(host_material);
    unpack_state();

    // FindNeighborsKernel owns the dense collision grid privately and writes
    // results as a flat n*kStencilCells*k neighbor array (27 cells in 3D).
    device_neighbors = DeviceVector<int>(
            static_cast<size_t>(n) * kStencilCells * static_cast<size_t>(particles_per_cell));

    time = 0.0f;
    last_time = 0.0f;
    last_wall_clock_time = std::chrono::steady_clock::now();
    real_time_ratio = 0.0f;
    // Per-material masses come from the config (index = material id:
    // 0 = wall, 1 = sand).
    const int num_materials = static_cast<int>(config.masses.size());
    host_mass = HostVector<float>(num_materials);
    device_mass = DeviceVector<float>(num_materials);
    for (int i = 0; i < num_materials; ++i) {
        host_mass[i] = config.masses[i];
    }
    device_mass.copy_from_host(host_mass);

    device_state_n = DeviceVector<float>(kStateStride * n);

    // Create CUDA kernels
    const bool kt = config.kernel_timing;
    find_neighbors_kernel = std::make_unique<FindNeighborsKernel>(
            device_state.data(), n, collision_grid_size_x, collision_grid_size_y,
            collision_grid_size_z, particles_per_cell,
            config.domain, config.threads_per_block, device_neighbors.data(), kt);
    compute_rhs_kernel = std::make_unique<ComputeRHSKernel>(device_state.data(), device_material.data(),
            device_mass.data(), device_neighbors.data(),
            n, particles_per_cell, config.physics, config.threads_per_block, device_rhs.data(), kt);
    semi_implicit_euler_kernel = std::make_unique<SemiImplicitEulerKernel>(device_state.data(),
            device_rhs.data(), n, dt, config.threads_per_block, kt);
    backward_euler_picard_kernel = std::make_unique<BackwardEulerPicardKernel>(device_state.data(),
            device_state_n.data(), device_rhs.data(), n, dt, config.threads_per_block, kt);

    device_energy = DeviceVector<float>(1);
    host_energy = HostVector<float>(1);
    energy_kernel = std::make_unique<EnergyKernel>(device_state.data(), device_material.data(),
            device_mass.data(), n, config.physics, config.threads_per_block, device_energy.data(), kt);
}


void ParticleDynamics::resize(const int new_n) {
    n = new_n;
    positions = std::vector<float>(kDim * n);

    host_state = HostVector<float>(kStateStride * n);
    device_state = DeviceVector<float>(kStateStride * n);
    device_rhs = DeviceVector<float>(kStateStride * n);
    host_material = HostVector<int>(n);
    device_material = DeviceVector<int>(n);
}


void ParticleDynamics::unpack_state() {
    time_unpack_state = 0.f;
    start_timer();

    CUDA_CHECK(cudaDeviceSynchronize());

    host_state.copy_from_device(device_state);

    for (size_t i = 0; i < n; ++i) {
        for (int a = 0; a < kDim; ++a) {
            positions[kDim*i + a] = host_state[kStateStride * i + a];
        }
    }
    stop_timer(time_unpack_state);
    real_time_ratio = get_real_time_ratio();
}



void ParticleDynamics::initialize_to_two_particles(const float x0, const float y0, const float z0) {
    // Separated along z (the gravity axis), so the pair stacks vertically just
    // as it did when y was up in the 2D version.
    const float separation = config.two_particle_separation;
    printf("Initializing to two particles at (%.2f, %.2f, %.2f) and (%.2f, %.2f, %.2f)...\n",
            x0, y0, z0, x0, y0, z0 + separation);
    resize(2);

    const float p0[kStateStride] = {x0, y0, z0,                0.f, 0.f, 0.f};
    const float p1[kStateStride] = {x0, y0, z0 + separation,   0.f, 0.f, 0.f};
    for (int a = 0; a < kStateStride; ++a) {
        host_state[a] = p0[a];
        host_state[kStateStride + a] = p1[a];
    }

    host_material[0] = config.particle_material;
    host_material[1] = config.particle_material;
}


void ParticleDynamics::initialize_to_single_particle(const float x0, const float y0, const float z0) {
    const float vx0 = config.init_vx0;
    const float vy0 = config.init_vy0;
    const float vz0 = config.init_vz0;
    printf("Initializing to a single particle at (%.2f, %.2f, %.2f) with velocity (%.2f, %.2f, %.2f)...\n",
            x0, y0, z0, vx0, vy0, vz0);
    resize(1);

    const float p[kStateStride] = {x0, y0, z0, vx0, vy0, vz0};
    for (int a = 0; a < kStateStride; ++a) {
        host_state[a] = p[a];
    }

    host_material[0] = config.particle_material;
}


void ParticleDynamics::initialize_to_cube(const float x0, const float y0, const float z0) {
    // Spacing uses the same radius as the collision physics so the initial
    // packing matches the contact model (deduped from compute_rhs_kernel).
    const float radius = config.physics.particle_radius;
    const float origin[kDim] = {x0, y0, z0};
    const float length[kDim] = {config.cube_length_x, config.cube_length_y, config.cube_length_z};
    int num_per_side[kDim];
    for (int a = 0; a < kDim; ++a) {
        num_per_side[a] = static_cast<int>(length[a] / (2 * radius));
    }
    // Particle count now scales as the cube of the side length, not the square.
    resize(num_per_side[0] * num_per_side[1] * num_per_side[2]);
    const float velocity[kDim] = {config.init_vx0, config.init_vy0, config.init_vz0};
    printf("Initializing to a %dx%dx%d cube (%d grains) at (%.2f, %.2f, %.2f) "
           "with velocity (%.2f, %.2f, %.2f)...\n",
            num_per_side[0], num_per_side[1], num_per_side[2], n, x0, y0, z0,
            velocity[0], velocity[1], velocity[2]);

    // Deterministic jitter (see SimConfig::init_jitter): a fixed seed keeps
    // repeated runs of the same config bit-identical.
    std::mt19937 rng(config.init_seed);
    std::uniform_real_distribution<float> jitter(-config.init_jitter * radius,
                                                  config.init_jitter * radius);
    // Independent stream, so changing the perturbation seed leaves the base
    // packing bit-identical (see SimConfig::init_perturbation).
    std::mt19937 perturbation_rng(config.init_perturbation_seed);
    std::uniform_real_distribution<float> perturbation(
            -config.init_perturbation * radius, config.init_perturbation * radius);
    for (int i = 0; i < num_per_side[0]; ++i) {
        for (int j = 0; j < num_per_side[1]; ++j) {
            for (int k = 0; k < num_per_side[2]; ++k) {
                const int lattice[kDim] = {i, j, k};
                const int p = (i * num_per_side[1] + j) * num_per_side[2] + k;
                for (int a = 0; a < kDim; ++a) {
                    host_state[kStateStride * p + a] =
                            origin[a] + static_cast<float>(lattice[a]) * 2.f * 1.01f * radius
                            + (config.init_jitter > 0.f ? jitter(rng) : 0.f)
                            + (config.init_perturbation > 0.f ? perturbation(perturbation_rng) : 0.f);
                    host_state[kStateStride * p + kDim + a] = velocity[a];
                }
                host_material[p] = config.particle_material;
            }
        }
    }


    // Set all particles to the snow material except for the sled particle.
    for (size_t i = 0; i < n - 1; ++i) {
        host_material[i] = config.particle_material;
    }
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
        (*compute_rhs_kernel)();
        semi_implicit_euler_kernel->set_dt(dt);
        (*semi_implicit_euler_kernel)();
    } else {
        CUDA_CHECK(cudaMemcpy(device_state_n.data(), device_state.data(),
                kStateStride * n * sizeof(float), cudaMemcpyDeviceToDevice));
        for (int k = 0; k < config.picard_iterations; ++k) {
            (*find_neighbors_kernel)();
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
    const float lo[kDim] = {d.x_min, d.y_min, d.z_min};
    const float hi[kDim] = {d.x_max, d.y_max, d.z_max};
    for (int i = 0; i < n; ++i) {
        for (int a = 0; a < kDim; ++a) {
            const float p = host_state[kStateStride * i + a];
            if (!std::isfinite(p) || p < lo[a] || p > hi[a]) {
                return false;
            }
        }
    }
    return true;
}


float ParticleDynamics::compute_max_acceleration() {
    HostVector<float> host_rhs(device_rhs.size());
    host_rhs.copy_from_device(device_rhs);
    float max_accel = 0.0f;
    for (int i = 0; i < n; ++i) {
        float accel_sq = 0.f;
        for (int a = 0; a < kDim; ++a) {
            const float acc = host_rhs[kStateStride * i + kDim + a];
            accel_sq += acc * acc;
        }
        const float accel = std::sqrt(accel_sq);
        if (accel > max_accel) {
            max_accel = accel;
        }
    }
    return max_accel;
}


int ParticleDynamics::grid_overflow_count() const { return find_neighbors_kernel->overflow_count(); }


float ParticleDynamics::find_neighbors_wct()    const { return find_neighbors_kernel->wall_clock_time(); }
float ParticleDynamics::compute_rhs_wct()       const { return compute_rhs_kernel->wall_clock_time(); }
float ParticleDynamics::take_step_wct()         const {
    if (config.time_integrator == "semi_implicit_euler") {
        return semi_implicit_euler_kernel->wall_clock_time();
    }
    return backward_euler_picard_kernel->wall_clock_time();
}

ParticleDynamics::~ParticleDynamics() = default;
