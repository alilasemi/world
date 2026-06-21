# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A real-time particle/fluid simulation: a CUDA simulation backend serves particle state over a WebSocket, and a browser-based WebGL2 client renders it. There are two independent build worlds:

- **C++/CUDA backend** (repo root, `src/`, `test/`) — built with the root `Makefile`, requires CUDA + an NVIDIA GPU.
- **Node client** (`client/`) — an Express static server + vanilla WebGL2/JS frontend, no build step.

## Build & run (backend)

`make` auto-detects `CUDA_HOME` from `nvcc` on `PATH` (or respects `CUDA_HOME`/`CUDA_PATH` env vars) and auto-detects GPU arch (`GPU_ARCH=native`). It also auto-clones/builds missing dependencies (GLFW, GoogleTest, uWebSockets) into `external/` on first build — expect the first `make` to take a while and to need network access. GLAD is the one dependency that is *not* auto-fetched: if `external/glad/src/glad.c` is missing, the Makefile errors out and tells you to generate it manually from https://glad.dav1d.de/ (GL 3.3, Core profile).

```
make              # build deps + build/bin/world (the WebSocket sim server)
make debug        # same, with -g -DDEBUG
make profile      # build build/bin/profile (no-network profiling binary, see src/profiling_sim.cpp)
make stability    # build build/bin/stability_and_accuracy (dt sweep, see src/stability_and_accuracy.cpp)
make test         # build and run build/bin/test (currently does not compile, see Known issues)
make clean        # rm -rf build/
make GPU_ARCH=sm_75   # override arch for cross-compiling / unusual setups
```

Run the server: `./build/bin/world` (listens on port 8081 for WebSocket connections).

There is no `nvcc`-only single-file shortcut for individual kernels — everything goes through the Makefile's pattern rules (`src/%.cpp` -> g++, `src/%.cu` -> nvcc).

## Run the client

```
cd client && npm install
node index.js local     # serves on :8080, points the WS client at localhost:8081
node index.js gcloud     # default; resolves the server's public IP via api.ipify.org
```
Open `http://localhost:8080` in a browser. The page hits `/config` to learn `mode`/`ip`, then opens `ws://<ip>:8081`.

## Architecture

### Backend: sim drives a request/response WS protocol

`src/broadcast.cpp` (compiled to `build/bin/world`) owns a `uWS::App` on port 8081 and a single `unique_ptr<ParticleDynamicsCUDA>` sim instance (one sim per process, recreated on `"initialize"`). The protocol is a simple text-command / binary-response exchange:

- Client sends `"initialize"` -> server allocates a new `ParticleDynamicsCUDA`, sends back `int32 n` then `n*2` floats of initial `(x, y)` state.
- Client sends `"run"` -> server calls `sim->take_step()` 10x, unpacks state, sends back the updated `(x, y)` float buffer.

The client (`client/webgl_demo.js`, `Client` class) mirrors this as a small state machine: `state 0` (waiting for `n`) -> `state 1` (waiting for IC) -> `state 2+` (steady loop: draw, render, request next `"run"`). `client/index.js` is unrelated to the sim — it's just the Express static file/`,/config` server.

`test/main.cpp` and `src/profiling_sim.cpp` are alternate driver entry points (no WebSocket) that run `ParticleDynamicsCUDA` directly and print timings — useful for profiling/debugging without spinning up the client.

`src/stability_and_accuracy.cpp` (`build/bin/stability_and_accuracy`) is a third driver: it sweeps a fixed list of `dt` values, running each from the same default IC to simulated `t = 0.1s` and reporting final total energy (`ParticleDynamicsCUDA::compute_total_energy()`) and a stability verdict (`ParticleDynamicsCUDA::is_stable()` — all particles finite and within `[-1,1]x[-1,1]`). Because `CUDA_CHECK` `abort()`s the whole process on any CUDA error (e.g. `UpdateGridKernel`'s grid-overflow assert, plausible under a large/unstable `dt`), each `dt` runs in its own `fork()`+`execl()`-relaunched child process (the same binary re-invoked with `--dt <value> --out <path>`) so one crashing `dt` doesn't take down the rest of the sweep; the parent never touches CUDA itself (fork-after-CUDA-init is unsupported) and reports a non-zero/signaled child exit as `stable = false` rather than reading the (possibly absent) output file.

### Simulation: `ParticleDynamicsCUDA` (src/particle_dynamics_cuda.{h,cu})

State layout is a flat device array of `4*n` floats per particle: `[x, y, vx, vy]`. Each particle has a `material` id (0 = wall, 1 = snow, 2 = sled) which indexes a `mass` array — this is what differentiates particle types rather than separate classes. Per `take_step()`:

1. `UpdateGridKernel` (src/update_grid_kernel.{h,cu}) — buckets particles into a uniform `grid_size x grid_size` spatial grid (`particles_per_cell` capacity per cell, hard assert-fails on overflow) for neighbor lookups.
2. `InterpolateForceKernel` (src/interpolate_force_kernel.{h,cu}) — bilinearly interpolates the m×m `device_grid_force_x/y` field (see "AI control hooks" below) onto each particle's position, writing per-particle `device_body_force_x/y`.
3. `ComputeRHSKernel` (src/compute_rhs_kernel.{h,cu}) — computes forces per particle: gravity, a penalty-based floor/wall repulsion, a penalty + damping repulsion against neighbors found via the 3x3 neighboring-cell stencil in the grid, and (added last) the interpolated `body_force_x/y` from step 2.
4. `TakeStepKernel` (src/take_step_kernel.{h,cu}) — explicit Euler integration of velocity then position, in a single per-particle pass (no cross-particle dependency within a step).

Two more kernels are not part of `take_step()` — both are invoked on demand:
- `EnergyKernel` (`src/energy_kernel.{h,cu}`) via `ParticleDynamicsCUDA::compute_total_energy()` — sums each particle's `0.5*m*(vx^2+vy^2) + m*g*h` (`h` = height above the floor) via `atomicAdd` into a single device scalar, re-zeroed (`cudaMemset`) at the start of every call.
- `OccupancyGridKernel` (`src/occupancy_grid_kernel.{h,cu}`) via `ParticleDynamicsCUDA::update_occupancy_grid()` — see "AI control hooks" below.

All six derive from the `Kernel` base class (`src/kernel.h`/`.cu`), which is the standard pattern for any new CUDA kernel in this codebase: the constructor takes `n` (and optionally `threads_per_block`, default 256), and `operator()()` auto-computes `blocks`/`threads_per_block` from `n`, times the call via CUDA events (read back with `wall_clock_time()`), and checks both `cudaGetLastError()` (bad launch config) and `cudaDeviceSynchronize()` (execution errors) via `CUDA_CHECK` — which `abort()`s on failure rather than logging and continuing. Derived classes only implement `call_kernel(int blocks, int threads_per_block)`, launching their `__global__` function(s) with the config handed down (don't recompute it). All six kernels use the standard `index = blockIdx.x * blockDim.x + threadIdx.x; stride = blockDim.x * gridDim.x` grid-stride loop over `n`.

### AI control hooks: m×m force/occupancy grids

`ParticleDynamicsCUDA` exposes a second, independent grid (`force_grid_size`, default 16 — distinct from the 256×256 collision `grid_size`) meant for an external model: it would read an occupancy snapshot and output a per-cell force field. `device_grid_force_x`/`device_grid_force_y` (length `force_grid_size^2`, row-major `grid[cell_x*m+cell_y]`, one scalar per cell) hold that externally-supplied field — explicitly `cudaMemset`-zeroed at construction (a `DeviceVector`'s `cudaMalloc` does not zero memory, and `InterpolateForceKernel` reads this every step starting from the first `take_step()` call, so zero is both the safe default and the "no model attached yet" no-op state). Nothing in this codebase writes a non-zero field yet — a future driver would call the existing `DeviceVector::copy_from_host(...)` directly on these public members. `device_occupancy_grid` (same layout, 0/1) is the other direction: `update_occupancy_grid()` refreshes it from current particle positions but is never called automatically, so a future AI-driving loop must call it itself before reading the grid back to host — the snapshot is only as fresh as the last call.

`UpdateGridKernel::call_kernel` issues **two** kernel launches with a `cudaDeviceSynchronize()` barrier between them: `zero_grid_counts_kernel` then `update_grid_kernel`. This is required, not optional — under real parallelism there's no implicit ordering between particles' threads (unlike the old single-threaded version), so all relevant cells must be zeroed before any bucketing write happens. The zero pass zeros the same 3x3 cell neighborhood around each particle that `compute_rhs_kernel`'s stencil will later read (not just the particle's own cell) — otherwise cells with no particles of their own but adjacent to occupied ones are read uninitialized (`cudaMalloc` doesn't zero memory); this was caught by `compute-sanitizer --tool initcheck` and is the reason the zero pass isn't just "zero my own cell." Still O(n) since each particle only ever touches 9 cells. The bucket pass itself uses `atomicAdd` to reserve a slot per cell (`grid[cell] += 1` is not thread-safe under parallelism); verify any change here with `compute-sanitizer --tool racecheck`.

### Memory ownership: `HostVector<T>` / `DeviceVector<T>`

`src/host_vector.h` and `src/device_vector.h` are move-only RAII wrappers (malloc/cudaMalloc-backed) replacing raw host/device pointers. They're forward-declared against each other so `HostVector::copy_from_device` / `DeviceVector::copy_from_host` can cross-reference; `DeviceVector` deliberately has no `operator[]` since dereferencing device memory from host code is UB. All CUDA API calls should go through `CUDA_CHECK(...)` (src/cuda_check.h), which aborts with file/line/expr context on failure.

### Rendering

`ParticleDrawer` (`src/particle_drawer.{h,cpp}` and its hand-ported JS twin in `client/webgl_demo.js`) turns each particle's `(x, y)` into a small N-gon (fan of `num_triangles` triangles around a center point) for rasterization — geometry generation logic must be kept in sync between the C++ and JS versions if changed. `src/shader.h` is an unused native-OpenGL GLFW-era shader helper; the actual rendering path is the WebGL2/JS one in `client/`.

## Known issues

- `test/integrator_test.cpp` and `test/system_test.cpp` `#include` headers (`dormand_prince.h`, `particle_dynamics.h`) that do not exist anywhere in `src/`. `make test` will fail at compile time until these tests are rewritten against current code (e.g. `particle_dynamics_cuda.h`).
- `ParticleDynamicsCUDA::time` (the host-side simulation clock) is never actually incremented anywhere — a pre-existing, currently-harmless latent bug (nothing reads it).
- The Makefile's pattern rules (`$(obj_dir)/%.o: $(src_dir)/%.cpp`/`.cu`) only depend on the matching source file, not on any headers it includes — there's no `-MMD`/`.d`-file dependency tracking. Editing a widely-included header (e.g. `particle_dynamics_cuda.h`) and then doing an incremental `make` can silently link a stale `.o` (compiled against the old struct layout) against a freshly-rebuilt one, corrupting the stack at runtime (observed as "stack smashing detected" with garbage timing values). `make clean` before rebuilding after a header change is the reliable workaround.

## GPU verification

This dev box has a real CUDA toolkit + NVIDIA GPU and `compute-sanitizer` installed — when changing kernel code (especially anything touching the spatial grid's `atomicAdd`/zero-pass logic), actually build and run it rather than reasoning about correctness statically:
```
make profile && ./build/bin/profile             # sanity: should run to completion, print 4 nonzero timings
compute-sanitizer --tool memcheck ./build/bin/profile   # out-of-bounds / invalid access
compute-sanitizer --tool racecheck ./build/bin/profile  # shared/global memory race hazards
compute-sanitizer --tool initcheck ./build/bin/profile  # reads of uninitialized device memory
```
