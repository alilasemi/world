# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**POD study results (measured 2026-08-26 on the 150-rollout pilot; `surrogate/pod_study.py`,
plot in `assets/screenshots/pod_spectrum.png`).** Linear POD is ADEQUATE for the mass channel and
no autoencoder escalation is needed yet. Held-out relative L2 error on the settled mass field,
basis fitted on 120 rollouts and tested on 30:

| modes | mean-only (k=0) | k=5 | k=10 | k=20 | k=40 |
|---|---|---|---|---|---|
| mass | 0.871 | 0.516 | 0.381 | 0.243 | 0.192 |

So ~20 modes take a 16384-node field to 24% error, versus 87% for predicting the ensemble mean --
the basis is doing real work. 11 modes carry 90% of variance, 51 carry 99%.

**The momentum channels look unlearnable and are not — that was a measurement artifact of asking at
the wrong time.** At the final checkpoint, POD on momentum gives ~0.98 held-out error, i.e. no better
than zero. The reason is that there is nothing left to predict: `RMS|rho*u| / RMS|rho|` falls from
**2.29 at t=0.2 s to 0.018 at t=2.0 s** (a factor of ~120). The settled pile is at rest and its
momentum field is residual numerical creep. Asked *while the material is moving*, momentum is as
learnable as mass — held-out k=20 error at t=0.2 s is 0.35-0.40 for the momentum channels against
0.34 for mass. Consequences:
- **Stage 1 (theta -> settled outcome) should target the MASS channel only.** Fitting the settled
  momentum field would produce a meaningless score for a quantity that is 1.8% of its initial
  magnitude and is creep, not physics.
- **Stage 2 (latent dynamics) needs the momentum channels, and they are informative exactly where
  it needs them** -- the flight/impact window, t <~ 0.6 s.
- This **empirically confirms the Markov argument** rather than merely asserting it: at t=0.2-0.4 s
  momentum is ~2.3x the mass scale, so a density-only latent would omit the dominant part of the
  state. By t=2 s the state genuinely *is* density alone, which is why the settled outcome is a pure
  density field.

**Stage 1 surrogate fitted (2026-08-26; `surrogate/fit_surrogate.py`, figure in
`assets/screenshots/stage1_surrogate.png`).** `theta -> a -> s_1`, i.e. `R^7 -> R^20 -> R^16384`,
with 20 independent scalar GPs (Matern 5/2 + ARD + WhiteKernel), mass channel only, basis and
normalisations fitted on the training split alone. Held-out relative L2, 120 train / 30 test:

| contribution | error |
|---|---|
| ensemble mean only (K=0) | 0.871 |
| POD projection (truncation floor) | 0.243 |
| **composed surrogate** | **0.381** |
| regression alone, in coefficient space | 0.350 |

Truncation and regression contribute comparably, so **neither refining the basis nor improving the
regressor alone would move the total much** — worth knowing before optimizing either. Per-mode
held-out R^2 is >= 0.93 for the first six modes and decays to ~0.73-0.86 by mode 9: leading modes
carry smooth parameter-driven structure, the tail carries realisation-specific detail.

Uncertainty is close to calibrated and mildly overconfident: Pr(|z|<=1) = 0.743 vs nominal 0.683,
Pr(|z|<=2) = 0.918 vs 0.954, RMS(z) = 1.16.

**The leading POD modes are physically interpretable**, which is a real advantage of the hand-designed
latent over a learned one: mode 1 correlates 0.673 with the x launch velocity, mode 2 correlates 0.776
with the y launch velocity (and sigma_1 = 107.8 ~= sigma_2 = 101.7, as the x/y symmetry of the domain
requires), and mode 6 correlates 0.898 with grain mass.

ARD relevance ranking: `v_x (1.94) ~= v_y (1.66) > mu (1.25) > v_z (0.90) > m (0.78) ~= e (0.74) >>
z_0 (0.23)`. Release height is nearly irrelevant over its sampled range.

**Methodological trap, found the hard way:** aggregate ARD *relevance* (1/length_scale), never the
length scales themselves. The basis assigns the two horizontal velocities to *separate* modes of nearly
equal energy, so a sigma^2-weighted mean of length scales ranks `v_x` above `v_y` by the margin
`sigma_1^2 - sigma_2^2` alone. The first version of this analysis reported exactly that spurious
asymmetry, in a setup that is symmetric in x and y by construction — the symmetry is what made it
detectable.

**Predictability measured (2026-08-26; `build/bin/chaos`, `surrogate/plot_chaos.py`, figure in
`assets/screenshots/predictability.png`). This supplies the denominator, and it changes the
verdict.** An ensemble at fixed theta, members differing only by a random displacement of every
grain's initial position (`initialization.perturbation`, a fraction of the radius, with its own seed
so the base packing stays bit-identical):

*Chaos is real and strong.* With `perturbation = 1e-3 * radius` (eps = 10 um, one hundredth of a
grain diameter), the RMS per-grain separation amplifies **3806x**, from 1e-5 m to 3.81e-2 m, i.e. to
**1.9 grain diameters**. It reaches 90% of that plateau at **t = 0.62 s**, which is the per-grain
predictability horizon.

The growth is *not* a single exponential, and quoting one rate would be sloppy. Local
`d(ln D)/dt` by window: **23.3 /s** on [0.02, 0.06] (a fast transient as the contact network
rearranges), settling to a sustained **5.6-6.4 /s** on [0.14, 0.60] (e-folding time 0.16-0.18 s),
then collapsing to **0.17 /s** after 0.60 s. The sustained ~6 /s is the figure to quote.

*Saturation here is caused by jamming, not by grains exploring the domain.* Growth stops exactly when
the pile settles and friction locks the packing, which is why the plateau sits at only 1.9 grain
diameters rather than at the scale of the assembly. The horizon is therefore set by how long the
material flows. Consequently the textbook estimate
`t_horizon ~ (1/lambda) ln(D_sat/eps) = 1.38 s` overestimates the measured 0.62 s, because the early
growth is far faster than the sustained rate -- use the measured plateau, not the formula.

*The coarse field is dramatically more predictable.* Over the same interval, and past the point where
per-grain separation has saturated, the mass field differs by only **0.086** and the field's center of
mass by 0.009 grid nodes. That gap between the two curves is the whole argument for predicting a
coarse field rather than a trajectory: chaos destroys the estimator, not the quantity.

*The floor for the surrogate is 0.168, and getting this number right required care.* With
`perturbation = 0.25 * radius` (distinct realizations of the same macroscopic state), the **pairwise**
difference between realizations is 0.245, but that is **not** the floor. If `s = mu + n` with
independent zero-mean `n`, then `||s_a - s_b|| ~ sqrt(2)||n||` while a model predicting `mu` incurs
only `||n||`. Measured directly from `chaos/final_fields.bin` (16 realizations), the RMS deviation
**about the ensemble mean** is **0.168**, and the ratio to the pairwise number is 1.46 against
`sqrt(2) = 1.414` — the relation is confirmed empirically, so pairwise/sqrt(2) is a valid proxy when
fields aren't dumped. Comparing the surrogate to the pairwise 0.245 instead would have wrongly
suggested it was nearly optimal.

**Verdict:**

| quantity | value | vs floor |
|---|---|---|
| irreducible floor (scatter about the conditional mean) | **0.168** | — |
| POD truncation only, K=20 | 0.243 | 1.45x |
| full Stage-1 surrogate | 0.381 | **2.27x** |

So there **is** real headroom: the surrogate is 2.27x the floor, not at it. Both contributions are
above the floor, so both need work, and the priority is now measured rather than guessed:
- **Truncation** is above the floor at K=20 (0.243) and still marginally above at K=40 (0.192 held
  out). K ~ 40-60 should bring it to ~0.17 and stop being the binding constraint.
- **Regression** is then the limiter (the surrogate sits 0.38 against a 0.24 truncation error), which
  points at more training data — the pilot is only 150 realizations for a 7-dimensional input.

Nothing here suggests a nonlinear encoder is needed: the linear basis reaches 0.19 at K=40 against a
0.168 floor, so the subspace is nearly adequate and the autoencoder escalation stays unnecessary.

*A hypothesis that was checked and rejected:* that the fields are nearly disjointly supported (which
would force ~one mode per snapshot). Measured pairwise support overlap is 0.81 mean / 0.92 median and
pairwise cosine similarity 0.35, so the fields overlap heavily. Slow spectrum decay here is shape
variation, not disjoint support, and re-centring each field on its centroid barely helps
(10-mode variance captured 0.832 -> 0.858). Translation is **not** the bottleneck, contrary to the
Kolmogorov-barrier concern that motivated the study.

**Contact model now includes Coulomb friction (added 2026-08-25).** `physics.friction` (mu,
default 0.5) adds a tangential force capped at `mu * |F_normal|`, and the dashpot was narrowed to act
on the **normal component of the relative velocity only** — previously it was applied to the full
relative-velocity vector, which damped tangential sliding with a coefficient derived for head-on
restitution. Verified analytically, not by eyeballing a pile:
`model_problems/sliding_friction.yaml` slides one grain on the floor and it decelerates at
-4.49 m/s^2 against the ideal `-mu*g = -4.905`, then stops dead and stays stopped.

Three things learned in the process, all worth not rediscovering:

1. **Regularized Coulomb, and why.** True Coulomb friction is a set-valued constraint needing
   per-contact tangential-displacement history (Cundall-Strack), and this solver rebuilds its
   neighbor list every step so no contact identity survives to hang that history on. The stateless
   substitute is a viscous tangential force capped at the Coulomb limit, which saturates almost
   immediately. It does produce a genuine static hold in practice (the sliding grain stops and stays
   stopped), but it is *sliding* friction with no true static threshold. Also, spheres roll freely —
   with no rolling resistance the angle of repose comes out shallow (~15 deg here) versus ~30 deg for
   real sand. Rolling friction is the next fidelity step, not more normal-direction tuning.

   **Decision (2026-08-25): ~15 deg is good enough; do not spend time on this now.** The physics we
   need is present — the material has a yield stress, holds a slope, and produces a heap rather than
   a puddle — and the surrogate predicts *spreading outcomes*, which do not depend on matching sand's
   angle of repose quantitatively. Improving fidelity (rolling resistance, and the Cundall-Strack
   tangential spring for true static friction) is explicitly deferred to future work. Revisit only
   if a claim is made about granular statics specifically, where the shallow angle would matter.

2. **A perfect simple-cubic lattice is a degenerate initial condition.** Every sphere sits directly
   atop another with a vertical contact normal, so there is no lateral force anywhere and the block
   is self-supporting. Frictionless it collapsed anyway, because floating-point asymmetries grew
   unopposed; *with* friction those get suppressed and the lattice survives a 3 m/s impact
   completely intact — which looks like a bug and is not one. `initialization.jitter` (fraction of
   radius, deterministic via `initialization.seed`) breaks the symmetry and yields a real disordered
   packing. Compare `assets/screenshots/flat_without_friction.png` (frictionless: a flat carpet
   covering the whole floor) with `heap_with_friction.png` (mu=0.5 + jitter: a domed heap with a
   slope). The same knob supplies the epsilon-perturbed ICs the predictability measurement needs.

3. **`compute_total_energy()` now includes contact elastic energy — and that fixed a false alarm.**
   It originally summed only kinetic + gravitational potential, so a rising trace looked like
   numerical heating when it was really spring energy from compressed contacts reappearing as
   kinetic energy. With the contact term added the trace is **monotonically decreasing** as it must
   be (106240 -> 80630 over 3 s at dt=1e-3): there is no heating. Implementation notes:
   - The spring potential is **piecewise**, because the force is clamped: quadratic
     (`0.5*k*delta^2`) until the clamp engages and linear (`max_force*(delta - radius)`) after. Since
     `k = max_force/radius` the knee sits at exactly `delta = radius`. Treating it as purely
     quadratic would badly overestimate energy in precisely the large-overlap regime where it
     matters most.
   - Pair contacts are counted **once**, from the lower particle index — every pair appears in both
     particles' neighbor rows.
   - Dashpot and friction are *dissipative* and correctly contribute nothing.
   - `compute_total_energy()` rebuilds the neighbor list first, since `take_step()` leaves it one
     step stale (it searches, then moves the particles). That advances `FindNeighborsKernel`'s
     lifetime timing accumulator, so any caller that also reports per-frame kernel timings must
     subtract the difference — `src/profiling_sim.cpp` does exactly this via its `energy_at()`
     lambda, and without it the energy diagnostic would silently inflate reported neighbor-search
     time by one extra pass per outer iteration.
   - `device_neighbors` is memset to the `-1` sentinel at construction so an energy query before the
     first `take_step()` reads a valid empty list rather than uninitialized memory.

**Float32 position-update stall — a real precision trap.** Semi-implicit Euler does
`x += dt*v`, and when `dt*|v|` falls below half an ULP of `x`, the position simply does not change.
Near `|z| ~ 1` the float32 ULP is ~6e-8, so at `dt=1e-5` any `|v|` below ~6e-3 m/s produces *zero*
displacement. The velocity then persists indefinitely (the spring never sees a position change to
respond to) and its dashpot force keeps carrying part of the load. Directly observed: a resting grain
held `z = -0.990035951` bit-identical across 11,000 steps while `vz` stayed pinned at -2.298e-3 m/s,
and `c_wall * |vz| = 0.657 N` was exactly the shortfall between the normal force (7.19 N) and `m*g`
(7.848 N) — which is the entire 8% friction deficit above. Consequences: shrinking `dt` makes this
*worse*, not better, and coordinates whose magnitude is large relative to the displacement per step
waste precision. Fixing it properly means double-precision positions (or accumulating positions
relative to a local origin); neither is done, so treat sub-mm-per-second dynamics near the domain
walls as unresolved.

## What this is

A real-time **3D** particle/granular simulation: a CUDA simulation backend serves particle state over a WebSocket, and a browser-based WebGL2 client renders it. The simulation is 3D as of 2026-08-25 (`kDim = 3`, state stride 6, z up with gravity along `-z`); the 2D version is reachable only by checking out a commit before that conversion. There are two independent build worlds:

- **C++/CUDA backend** (repo root, `src/`, `test/`) — built with the root `Makefile`, requires CUDA + an NVIDIA GPU.
- **Node client** (`client/`) — an Express static server + vanilla WebGL2/JS frontend, no build step.

## Build & run (backend)

`make` auto-detects `CUDA_HOME` from `nvcc` on `PATH` (or respects `CUDA_HOME`/`CUDA_PATH` env vars) and auto-detects GPU arch (`GPU_ARCH=native`). It also auto-clones/builds missing dependencies (GLFW, GoogleTest, uWebSockets) into `external/` on first build — expect the first `make` to take a while and to need network access. GLAD is the one dependency that is *not* auto-fetched: if `external/glad/src/glad.c` is missing, the Makefile errors out and tells you to generate it manually from https://glad.dav1d.de/ (GL 3.3, Core profile).

```
make              # build deps + build/bin/world (the WebSocket sim server)
make debug        # same, with -g -DDEBUG
make profile      # build build/bin/profile (no-network profiling binary, see src/profiling_sim.cpp)
make stability    # build build/bin/stability_and_accuracy (dt sweep, see src/stability_and_accuracy.cpp)
make model_problem   # build build/bin/model_problem (full-history single-run driver, see src/model_problem.cpp)
make dataset      # build build/bin/dataset (surrogate training-set generator, see src/dataset.cpp)
make test         # build and run build/bin/test (GoogleTest suite, see Testing below)
make clean        # rm -rf build/
make GPU_ARCH=sm_75   # override arch for cross-compiling / unusual setups
```

Run the server: `./build/bin/world` (listens on port 8081 for WebSocket connections). `world` and `profile` read a YAML config — see "Configuration" below — from `config.yaml` in the cwd by default, or from a path passed as the first CLI arg (`./build/bin/world my_config.yaml`). `stability_and_accuracy` instead defaults to `stability/config.yaml` (its sweep-specific config; see below), and `model_problem` defaults to `model_problems/single_particle.yaml`, both also overridable by a first CLI arg. A missing file is fine for any of them: each falls back to built-in defaults with a printed notice.

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

`src/broadcast.cpp` (compiled to `build/bin/world`) owns a `uWS::App` on port 8081 and a single `unique_ptr<ParticleDynamics>` sim instance (one sim per process, recreated on `"initialize"`). The protocol is a simple text-command / binary-response exchange:

- Client sends `"initialize"` -> server allocates a new `ParticleDynamics`, then sends a fixed sequence of one-shot BINARY messages, each read by one step of the client's `Client.onmessage` state machine (states 0→5) before it enters the steady loop: `int32 n`, three `int32`s `collision_grid_size_x/y/z`, `int32 num_triangles` and `float particle_radius` (rendering constants), 6 floats `x_min/x_max/y_min/y_max/z_min/z_max` (domain bounds — same ones `is_stable()` checks server-side; the client uses these for its view volume and to count particles currently inside the domain vs. the total, shown on the HUD), then `n*kDim` floats of initial `(x, y, z)` positions. **Message sizes for n=32769: 4, 12, 4, 4, 24, 393228 bytes**, then `(n*kDim + 6)*4 = 393252` per `"run"` — verified end-to-end against the running server.
- Client sends `"run"` -> server calls `sim->take_step()` `config.steps_per_frame` times (10 by default), then checks `sim->grid_overflow_count()` once (see "Collision grid" below) -- a nonzero count prints one line and `exit(1)`s the whole server rather than continuing to serve a diverged sim -- then unpacks state and sends back the updated `(x, y)` float buffer.

The client (`client/world.js`, `Client` class) mirrors this as a small state machine: `state 0` (waiting for `n`) -> `state 1` (waiting for `collisionGridSizeX/Y`) -> `state 2` (waiting for IC) -> `state 3+` (steady loop: draw, render, request next `"run"`). `client/index.js` is unrelated to the sim — it's just the Express static file/`,/config` server.

`src/profiling_sim.cpp` is an alternate driver entry point (no WebSocket) that runs `ParticleDynamics` directly and prints timings — useful for profiling/debugging without spinning up the client. `test/particle_dynamics_test.cpp` drives `ParticleDynamics` the same way, under GoogleTest (see "Testing" below).

`src/stability_and_accuracy.cpp` (`build/bin/stability_and_accuracy`) is a third driver: it 2D-sweeps `dt` × `max_force` (overriding `config.physics.max_force` per run) from the shared `config.sweep_dt`/`config.sweep_max_force` (see "Configuration" below), running each combo from the same default IC and classifying it into one of three states — `UNSTABLE`, `STEADY`, or `TIMED_OUT`. Two run modes, selected by `config.stability_mode`:
- `"fixed_time"` (default): run for a fixed `config.stability_sim_time` seconds.
- `"steady_state"`: keep stepping (still capped at `stability_sim_time`) until the sim visibly settles — every `stability_steps_per_steadiness_check` steps, `ParticleDynamics::compute_max_acceleration()` (max `|accel|` over all particles, read from `device_rhs` — the same buffer `ComputeRHSKernel` just wrote `ax`/`ay` into — via the same device→host-copy-then-host-loop idiom as `is_stable()`) is compared against `physics.gravity * stability_acceleration_cutoff`; dropping below it stops the run early.

Either way, `UNSTABLE` vs. `STEADY`/`TIMED_OUT` is decided **once**, from `ParticleDynamics::is_stable()` on the final state after the loop above finishes — not by checking every intermediate step. A transient mid-run excursion outside `DomainParams` bounds that the penalty forces recover from by the end doesn't fail the combo; only where it actually lands at `t_max` (or wherever `steady_state` mode stopped early) does. One consequence: an already-diverged run isn't caught early by `is_stable()` anymore, so it burns through its remaining step budget like any other combo — for a badly diverged `dt`, this makes it more likely to hit `FindNeighborsKernel`'s collision-grid capacity (see "Collision grid" below), since the sim now keeps stepping well past the point it would previously have stopped. `run_child`'s local `check_grid_overflow()` polls `sim.grid_overflow_count()` at the same cadence as the `steady_state` accel check (plus once more after the loop, covering `fixed_time` mode) and, if it's ever nonzero, prints one line (`dt`, `max_force`, and the overflow count) and calls `exit(1)` — deliberately *not* checked every step, since a host sync that often would serialize what would otherwise be async-queued kernel launches and made an earlier version of this check dramatically slower.

Because an overflowing combo now `exit(1)`s cleanly (see above) rather than the process getting `abort()`-ed by a device-side assert, each `(dt, max_force)` combo still runs in its own `fork()`+`execl()`-relaunched child process (the same binary re-invoked with `--dt <value> --max_force <value> --out <path> --config <path>`) so one crashing combo doesn't take down the rest of the sweep; the parent never touches CUDA itself (fork-after-CUDA-init is unsupported) and treats a non-zero/signaled child exit as `UNSTABLE` rather than reading the (possibly absent) output file. The parent prints the `dt`×`max_force` status grid to stdout live, one cell at a time as each combo's subprocess returns (with an `fflush` after each cell) rather than only at the end, so progress is visible on a slow sweep. After the sweep it also always writes `stability/stability_results.dat`/`stability_plot.gp`/`stability_plot.png` (the `stability/` dir is created if missing, regardless of which config file was actually passed in), invoking `gnuplot` via `system()` — a soft dependency: a nonzero exit just prints a warning, sweep results aren't lost. The plot is a grid of filled rectangles on an *index* grid (dt/max_force values aren't evenly spaced, so real-valued axes would distort cell sizes), red/green/yellow for unstable/steady/timed-out, with tics mapped back to the actual swept values. `stability/config.yaml` (the default config for this binary) is dedicated to this exploration (`stability_mode: steady_state`, a multi-value `max_force_sweep`); the repo-root `config.yaml` no longer carries a `stability:` section at all (only `stability/config.yaml` does) since it's for `world`/`profile`, not this driver.

`src/dataset.cpp` (`build/bin/dataset`) is a fifth driver, and the one that generates the surrogate's
training set. One rollout per row of a design CSV (`surrogate/make_design.py`), no WebSocket in the
loop. Reads `dataset/blob_throw.yaml` and `dataset/design.csv` by default; writes
`dataset/grids.bin` (self-describing: magic `RTPGRD01`, then shape header, checkpoint times, then
`rollouts x checkpoints x channels x nx x ny x nz` float32) plus `dataset/manifest.csv` (theta per
row, status, final time, overflow count, deposited mass). Read it from Python with
`surrogate/dataset_io.py`, which validates the magic and shape rather than trusting them.

Four things about this driver that are decisions, not accidents:
- **Common random numbers.** Every rollout uses the same `init_seed`, so the base packing and jitter
  pattern are identical across the design and the only thing varying is theta. Cheapest available
  variance reduction for the downstream sensitivity estimates (see positioning doc §3.5).
- **A diverged rollout does not kill the run.** Unlike `broadcast.cpp` and `stability_and_accuracy.cpp`
  (which `exit(1)` on grid overflow), this one flags the row in the manifest, zero-fills its grids, and
  continues — losing an entire design hours in to one bad parameter combination would be far worse. The
  `status` column is therefore load-bearing: **always filter on it**, never assume every row is usable.
- **Blob mass is varied through per-grain mass, not grain count**, so `n` is fixed across the whole
  design. A varying `n` would change the state size and the latent's normalization row to row.
- **Release x/y are fixed at the domain center, not swept.** Sampling them would translate the whole
  outcome field rigidly, which carries no physics and is the worst possible case for a linear POD
  basis. Release *velocity* also moves the outcome but couples that motion to real physics (spread
  grows with speed), so it stays.
- **Checkpoints, not just the final state.** Recording the latent at ~10 times per rollout is what lets
  ONE dataset serve both the one-shot surrogate (`theta -> final field`) and the autoregressive latent
  dynamics model (`field_t -> field_{t+1}`).

**Two dataset configs, and the second exists for a measured reason.**
`dataset/blob_throw.yaml` (10 snapshots over 2 s) is the Stage-1 terminal-state set;
`dataset/blob_dynamics.yaml` (20 snapshots over 1 s, i.e. `Delta = 0.05` s) is the Stage-2 set.
Identical physics -- only the sampling in time differs. The reason is that the assembly comes to rest
by ~0.6 s: the momentum/mass ratio falls 2.29 -> 0.06 over the first six snapshots of the Stage-1 set
and is negligible after. Consecutive pairs drawn from it would be dominated by transitions in which
*nothing moves*, and a dynamics model trained on those learns the identity map. Output directory is
the dataset driver's third argument so both sets coexist (`dataset/`, `dataset_dynamics/`).

**Stage 2 design decisions** (`surrogate/fit_dynamics.py`):
- **All four channels are carried**, unlike Stage 1. Mass alone is not a Markov state; the momentum
  channels are exactly what the transient needs, and the POD study showed they are informative for
  `t <~ 0.6` s.
- **The action parameters are withheld from the model** -- only the three material parameters are
  supplied. The launch velocity has already done its work by the first recorded snapshot and is
  carried in the momentum channels; feeding it in as well would let the model bypass the latent and
  make "is the latent a sufficient state?" untestable.
- **The train/test split is by ROLLOUT, never by transition.** Consecutive transitions from one
  rollout are strongly correlated, so splitting transitions at random leaks a test rollout's own
  trajectory into training and makes the rollout error meaningless. This is the easiest way to get a
  flatteringly wrong answer here.
- **Three baselines are reported**, because an autoregressive model has to earn its complexity:
  persistence (`a(t+Delta) = a(t)`), one-step-from-truth (isolates error accumulation from one-step
  accuracy), and **direct** `theta -> a(t)` refitted per horizon. The last is the honest competitor:
  if predicting the state directly from `theta` beats stepping to it, the dynamics model is worthless.

**`dataset/blob_throw.yaml` runs at `dt=1e-4`, ten times finer than the interactive demo, and that is
required, not cautious.** The contact period is `2*pi*sqrt(m_reduced/k)` with `k = max_force/radius`;
for the grain masses swept here that is 1.4-4.4 ms, so `dt=1e-3` gives only **1.4-4.4 steps per
contact** and rollouts explode. Measured before the fix: 5 of 8 rollouts diverged, every one at the
*first* checkpoint with **zero** grid-overflow cells — i.e. numerical blow-up, not grains escaping
geometrically. That distinction is what identified the cause; the overflow counter being zero ruled
out the collision grid entirely. At `dt=1e-4` the same masses get 14-44 steps per contact and 8 of 8
survive. The blob is small (3375 grains, ~2 s per rollout) so the 10x step cost is irrelevant for data
generation, where wall-clock per rollout matters far less than in the real-time demo. **General rule
for this codebase: want >= 20 steps per contact period; below ~5 is hopeless.**

`src/model_problem.cpp` (`build/bin/model_problem`) is a fourth driver, for granular debugging rather than pass/fail sweeping: for every `(dt, max_force, restitution)` combo in the same shared `sweep:` section as `stability_and_accuracy` (`config.sweep_dt` × `config.sweep_max_force`) plus a third axis unique to this driver, `config.sweep_restitution` (`sweep.restitution` in YAML, overriding `config.physics.restitution` per combo) — any axis left unset in the config degenerates to a single value at the config's own `simulation.dt`/`physics.max_force`/`physics.restitution`, so an unswept config is exactly the original single-run behavior — it records **every single step's** full state (`sim.host_state` — `x/y/vx/vy` per particle, not just position), unlike `stability_and_accuracy`'s pass/fail-only verdict. It calls `sim.unpack_state()` (a full device→host sync) after every `take_step()` with no batching or infrequent-polling trick — the opposite tradeoff from `stability_and_accuracy`/`broadcast.cpp`'s overflow checks, but fine here since model problems are small and short by design. All combos land in one `model_problems/history.dat`: a header row (`# step time x0 y0 vx0 vy0 ...`), then each combo as its own block (`# combo N: dt=... max_force=... restitution=...` followed by its rows), blocks separated by **two** blank lines — gnuplot's actual dataset-`index` separator (a single blank line only marks a within-index discontinuity; verified directly against gnuplot 6.0, since getting this wrong silently drops every combo past the first with a "no valid points" warning). The generated `model_problems/trajectory_plot.gp` (same `gnuplot`-`system()` idiom as `stability_and_accuracy`) plots each combo's `index i` as its own colored, legended line of time vs. particle 0's y-position (`kPalette` in model_problem.cpp — first color is always purple, so a single-combo run looks identical to before this supported sweeping), plus a red dashed reference line at `y = floor_y + particle_radius`. A combo whose grid overflows just warns and keeps its (diverged) data — unlike `broadcast.cpp`/`stability_and_accuracy.cpp`, there's no subprocess isolation here, so `exit(1)`ing would also kill every other combo in the sweep. Defaults to `model_problems/single_particle.yaml` — a single particle released from rest at the domain center under gravity, using the new `init_type: single_particle` (`ParticleDynamics::initialize_to_single_particle()`, alongside `"cube"`/`"two_particles"`; also takes `init_vx0`/`init_vy0`, unlike the always-at-rest `"two_particles"`/`"cube"`). `model_problems/first_collision.yaml` sweeps dt × max_force for a single high-speed floor impact (gravity off, particle released 3 radii up with `vy0=-10`) to see how timestep resolution and contact stiffness change the shape of one collision.

### Configuration: `SimConfig` + `config.yaml` (src/sim_config.{h,cpp})

All tunable constants (timestep, grid sizes, gravity/radius/max-force/restitution, domain bounds, per-material masses, initialization geometry, rendering `num_triangles`, driver settings like port / steps-per-frame, `stability_and_accuracy`'s run mode/steadiness cutoff under its own `stability:` YAML section, `model_problem`'s `sim_time` under its own `model_problem:` section, and the `sweep:` section's `dt`/`max_force` (`config.sweep_dt`/`config.sweep_max_force`, shared by both `stability_and_accuracy` and `model_problem`) plus `restitution` (`config.sweep_restitution`, `model_problem`-only) — every one empty by default, meaning "just the config's own `simulation.dt`/`physics.max_force`/`physics.restitution`") live in a single `SimConfig` struct (`src/sim_config.h`). Its member defaults reproduce the values that were previously hardcoded, so a **default-constructed `SimConfig` (and `ParticleDynamics{}`) behaves exactly as before** — this is what the GoogleTest suite and the default driver paths rely on.

`load_config(path)` (`src/sim_config.cpp`) is a small hand-written YAML reader (no external dependency) supporting the subset the config needs: top-level `section:` headers, indented `key: value` lines, `[a, b, c]` inline lists, and `#` comments. It starts from defaults and overrides only the keys present, warns on unknown keys, and returns defaults (with a notice) if the file is absent. The example `config.yaml` at the repo root lists every value at its current default. If you add a new tunable: add the field to `SimConfig`, a case in `apply_kv` in `sim_config.cpp`, and a line to `config.yaml`.

Two small POD structs in `sim_config.h` — `DomainParams` (`x_min/x_max/y_min/y_max`) and `PhysicsParams` (gravity, particle_radius, max_force, floor/ceiling/left-wall/right-wall positions) — are passed **by value** into the kernels (the standard way to get these constants device-side; kernels store them as members and forward them to the `<<<>>>` launch). The domain is no longer assumed to be `[-1,1]`: every coordinate→cell mapping (`find_neighbors`) and `is_stable()` derives its range from `DomainParams`. `threads_per_block` is also config-driven, forwarded through each kernel ctor to the `Kernel` base.

### Simulation: `ParticleDynamics` (src/particle_dynamics.{h,cu})

`ParticleDynamics`'s constructor takes a `const SimConfig&` (defaulted), stores it as the public `config` member, and pulls all its grid/physics/init parameters from it instead of hardcoding them. State layout is a flat device array of `kStateStride*n` floats: `[x, y, z, vx, vy, vz]` per particle, with position components at offsets `0..kDim-1` and velocity at `kDim..kStateStride-1`. `kDim`, `kStateStride` and `kStencilCells` are declared in `sim_config.h` so the layout is stated once instead of as a bare `6 *` scattered through every kernel. **z is up** — gravity acts along `-z` — which costs one axis flip in the renderer's view matrix (GL is y-up) but matches DEM/CFD convention. `ParticleDynamics::positions` (renamed from `xy`) holds `kDim` floats per particle and is what gets streamed to the client. Each particle has a `material` id (0 = wall, 1 = snow, 2 = sled) which indexes a `mass` array — this is what differentiates particle types rather than separate classes. Per `take_step()`:

1. `FindNeighborsKernel` (src/find_neighbors_kernel.{h,cu}) — buckets particles into a `collision_grid_size_x x collision_grid_size_y` collision grid (see "Collision grid" below), then gathers each particle's neighbors into a flat `device_neighbors` array for `ComputeRHSKernel` to consume; `particles_per_cell` capacity per cell, gracefully drops excess particles on overflow (see "Collision grid" below) rather than corrupting memory or crashing.
2. `ComputeRHSKernel` (src/compute_rhs_kernel.{h,cu}) — computes forces per particle: gravity, a penalty-based floor/wall repulsion, and a penalty + damping repulsion against neighbors read from `device_neighbors` (no collision-grid knowledge of its own). The repulsion is a linear spring (`-max_force/radius * overlap`, clamped to `max_force`) plus a linear dashpot that only acts while overlapping — a proper Kelvin-Voigt/linear spring-dashpot contact model (Cundall & Strack 1979) — plus a **tangential Coulomb friction force** capped at `physics.friction * |F_normal|` (see `add_tangential_friction()`). The dashpot acts on the **normal component of the relative velocity only**; the relative velocity is decomposed against the contact normal, and the tangential remainder is what friction opposes. For the axis-aligned walls the normal is `+/- e_a`, so `vel[a]` is already the normal component and the tangential part is `vel` with that component zeroed. The dashpot coefficient `c` isn't a free parameter: `restitution_to_damping()` derives it from `physics.restitution` (a `[0,1]` coefficient-of-restitution target — 0 = critically damped, 1 = perfectly elastic) via the standard DEM restitution-to-damping relation (Tsuji, Tanaka & Ishida 1992; surveyed in Di Renzo & Di Maio 2004), using the colliding particle's own mass for floor/ceiling/wall contacts (the fixed-boundary = infinite-mass limit) and the two-body reduced mass `m_i*m_j/(m_i+m_j)` for particle-particle contacts. This replaced an earlier model where the damping force was `-floor_force * v_rel` — i.e. the damping *coefficient* was the spring force itself, which injected extra stiffness that grew with impact speed (`∂F/∂y` from that term scaled with `v_rel`), the opposite of what you want when trying to relax the timestep constraint. One caveat (see the comment above `restitution_to_damping()`): the restitution-to-damping formula assumes the spring stays linear for the whole contact, so if an impact is energetic enough to hit the `max_force` clamp, the achieved restitution comes out measurably below the `physics.restitution` target (verified against `model_problems/first_collision.yaml`).
3. `SemiImplicitEulerKernel` (src/semi_implicit_euler_kernel.{h,cu}) or `BackwardEulerPicardKernel` (src/backward_euler_picard_kernel.{h,cu}) — the active time integration kernel, selected by `config.time_integrator`. Semi-implicit Euler updates velocity then position in one pass (using the updated velocity). Backward Euler Picard iterates `config.picard_iterations` times, re-running steps 1–2 each iteration and computing `x^{n+1} = x^n + dt·f(x^{n+1}_k)`, starting from `x^{n+1}_0 = x^n`.

Two kernels are not part of `take_step()` and are invoked on demand:

**`DensityGridKernel`** (`src/density_grid_kernel.{h,cu}`) via `ParticleDynamics::compute_density_grid()` — deposits the coarse **latent** the outcome surrogate is trained on. Four channels in one pass (`kChannels = 1 + kDim`): mass, then momentum density `rho*u` per axis. The momentum channels are not optional: a density field alone is **not a Markov state** (two piles with identical shape but different velocity fields evolve differently), so a latent *dynamics* model needs them. This is the same defect diagnosed in the removed RL branch, whose position-only observation space was non-Markov; dividing channels 1..kDim by channel 0 recovers a mass-weighted velocity field, and downstream code must guard the empty-node divide-by-zero exactly as the old `occupancy_velocity_kernel` did.

Key properties, each pinned by a test in `test/particle_dynamics_test.cpp`:
- **Trilinear / cloud-in-cell deposit**, i.e. MPM's particle-to-grid transfer — chosen over nearest-node binning on purpose. Binning makes the field jump the instant a grain crosses a cell boundary, and that discontinuity would land in the regression target as noise the surrogate cannot explain. CIC is C0 in particle position, which conditions both the POD basis and the GP fit much better.
- **Node-centered**: `density_grid_size_*` nodes span the domain *inclusive*, so spacing is `(hi-lo)/(size-1)` and a grain exactly on a boundary deposits wholly onto the boundary node.
- **Exactly mass-conserving.** Out-of-range indices are *clamped*, not dropped, so the eight weights always sum to 1 (`DensityGridConservesTotalMass` checks the summed mass channel against total particle mass for a jittered, mid-flight configuration). The cost is a slight pile-up on boundary nodes, acceptable because the walls confine the grains anyway.
- **Channel-major layout**: channel `c` occupies `[c*nodes, (c+1)*nodes)`, node index `(ix*size_y + iy)*size_z + iz`. Each field stays contiguous, which is what POD/PCA wants when slicing one channel.
- Sized **independently of the collision grid** (`density_grid_size_x/y/z`, default 32^3): the collision grid must be ~one particle diameter per cell for neighbor search to stay cheap, while the latent wants to be coarse enough that a learned model has few outputs and each node averages many grains.
- Zero-mass materials (the wall id) are skipped before the eight atomics.

**`EnergyKernel`** (`src/energy_kernel.{h,cu}`) via `ParticleDynamics::compute_total_energy()` — sums kinetic + gravitational + **contact elastic** energy via `atomicAdd` into a single device scalar, re-zeroed (`cudaMemset`) each call. See the energy notes under "Working agreements" for why the elastic term matters and for the piecewise (clamped-spring) potential.

All of these derive from the `Kernel` base class (`src/kernel.h`/`.cu`), which is the standard pattern for any new CUDA kernel in this codebase: the constructor takes `n` (and optionally `threads_per_block`, default 256), and `operator()()` auto-computes `blocks`/`threads_per_block` from `n`, times the call via CUDA events (read back with `wall_clock_time()`), and checks both `cudaGetLastError()` (bad launch config) and `cudaDeviceSynchronize()` (execution errors) via `CUDA_CHECK` — which `abort()`s on failure rather than logging and continuing. Derived classes only implement `call_kernel(int blocks, int threads_per_block)`, launching their `__global__` function(s) with the config handed down (don't recompute it). All kernels use the standard `index = blockIdx.x * blockDim.x + threadIdx.x; stride = blockDim.x * gridDim.x` grid-stride loop over `n`.

### Collision grid: dense `collision_grid` + compact `particles_in_cell`, owned by `FindNeighborsKernel`

The collision grid (`collision_grid_size_x × collision_grid_size_y`, default 32×32, independently sized) is the only spatial grid in the simulation: owned privately by `FindNeighborsKernel`, used for neighbor lookup, and not exposed to external code.

`FindNeighborsKernel` owns three private `DeviceVector<int>` arrays that together form the collision grid:

- **`collision_grid`** (`collision_grid_size_x*collision_grid_size_y*collision_grid_size_z` ints): `collision_grid[(cell_x*grid_size_y + cell_y)*grid_size_z + cell_z]` (x slowest-varying, z fastest, so cells adjacent along the gravity axis are adjacent in memory) is -1 if the cell is empty, or a compact *occupied-cell index* (0-based) if at least one particle landed there this step.
- **`particles_in_cell`** (n×k ints): row `occ_idx` (of width `particles_per_cell`) lists the particle indices in that occupied cell. Memory is O(n·k) rather than O(collision_grid_size_x*collision_grid_size_y·k).
- **`num_per_cell`** (n ints): `num_per_cell[occ_idx]` is the count of particles actually *stored* in occupied cell `occ_idx` -- clamped to `particles_per_cell`, never larger. Also used as an atomic slot counter during the fill pass.
- **`num_occupied_cells`** (1 int): device-side counter, atomically incremented during the compact pass to assign occ_idx values.

`FindNeighborsKernel::call_kernel` does four launches per step (three `cudaMemset` resets first):

1. **`mark_cells_kernel`** (n threads): each particle does `atomicCAS(&collision_grid[cell_index], -1, 1)` to mark its cell as occupied. No index assignment yet, so no spin-waiting is needed.
2. **`compact_cells_kernel`** (`collision_grid_size_x*collision_grid_size_y` threads): each thread owns one cell; if `collision_grid[c] == 1`, assigns `collision_grid[c] = atomicAdd(num_occupied_cells, 1)`. Produces unique compact indices. Single-writer per cell → no race on the store.
3. **`fill_cells_kernel`** (n threads): each particle reads its cell's occ_idx, atomically gets a slot via `atomicAdd(&num_per_cell[occ_idx], 1)`, and writes `particles_in_cell[occ_idx * k + slot] = i` — unless `slot >= particles_per_cell` (cell over capacity), in which case the particle is dropped from this step's neighbor search instead of writing out of bounds, its reservation is `atomicSub`'d back so `num_per_cell` stays clamped to `particles_per_cell` (see "Known issues"), and the cell is flagged in `cell_overflowed` (deduped via `atomicCAS` so a cell with many excess particles still only counts once) with the count accumulated into `num_overflowed_cells`. This replaces an earlier `assert(slot < particles_per_cell)`, which used to print once per dropped particle (thousands of lines under a bad `dt`) before `CUDA_CHECK`'s `cudaDeviceSynchronize()` check aborted the process. `num_overflowed_cells` is a **lifetime** counter (not reset per frame, unlike everything else here) so callers can poll it occasionally via `FindNeighborsKernel::overflow_count()` / `ParticleDynamics::grid_overflow_count()` without a host sync every step — `stability_and_accuracy.cpp`'s `check_grid_overflow()` and `broadcast.cpp`'s `"run"` handler both poll it (once per sweep-loop checkpoint / once per `steps_per_frame` batch, respectively) and print one clean message + `exit(1)` instead of a device-assert flood.
4. **`find_neighbors_kernel`** (n threads): walks the 3×3 stencil (bounds-checked against `collision_grid_size_x`/`collision_grid_size_y` independently), reads occ_idx from `collision_grid` (-1 → skip), iterates `particles_in_cell[occ_idx*k .. +min(num_per_cell[occ_idx], k)-1]`, writes found neighbors (excluding self) into `device_neighbors`, terminated by a `-1` sentinel.

`ComputeRHSKernel` only ever reads `device_neighbors` (owned by `ParticleDynamics`, sized `n*kStencilCells*particles_per_cell`, i.e. `n*27*k` in 3D) — it has no knowledge of the collision grid. Verify changes with `compute-sanitizer --tool racecheck` (atomic slot increments in fill pass) and `--tool initcheck` (`device_neighbors` read/write).

`particles_per_cell` should be sized to just above the actual worst-case occupancy of a single collision-grid cell (with `collision_grid_size_x/y/z` chosen so each cell is roughly one particle-diameter across, that's typically only a handful — the checked-in `config.yaml` uses 100 cells/axis over a 2.0-wide domain at radius 0.01, i.e. exactly one diameter per cell, with `particles_per_cell: 8`), not padded generously "to be safe" — `find_neighbors_kernel`'s per-cell loop bound is the *real* occupied count (`num_per_cell[occ_idx]`) and `compute_rhs_kernel`'s neighbor loop stops at the `-1` sentinel, so neither one does more arithmetic work as `particles_per_cell` grows. But `device_neighbors` (`n*27*particles_per_cell` in 3D) and `particles_in_cell` (`n*particles_per_cell`) are laid out as one fixed-width row per particle/cell, so a larger `particles_per_cell` widens `row_stride` and spreads adjacent particles' rows further apart in memory — this breaks warp-level coalescing (threads for adjacent particles no longer land in the same cache line) and inflates the working set, even though the useful data per row is unchanged. Measured on this dev box: raising `particles_per_cell` from 8 to 512 on an otherwise-fixed config roughly doubled `find_neighbors`/`compute_rhs` per-step time, with no change in correctness or actual neighbor counts.

### Memory ownership: `HostVector<T>` / `DeviceVector<T>`

`src/host_vector.h` and `src/device_vector.h` are move-only RAII wrappers (malloc/cudaMalloc-backed) replacing raw host/device pointers. They're forward-declared against each other so `HostVector::copy_from_device` / `DeviceVector::copy_from_host` can cross-reference; `DeviceVector` deliberately has no `operator[]` since dereferencing device memory from host code is UB. All CUDA API calls should go through `CUDA_CHECK(...)` (src/cuda_check.h), which aborts with file/line/expr context on failure.

### Testing

`test/main.cpp` is just the GoogleTest entrypoint (`InitGoogleTest`/`RUN_ALL_TESTS`); the actual tests live in `test/particle_dynamics_test.cpp`, which constructs `ParticleDynamics` directly (plain g++-compiled `.cpp`, same pattern as `src/profiling_sim.cpp` — no special CUDA-aware compilation needed for host code that just calls into the class). `test_src = $(wildcard test/*.cpp)` and the test binary links against every kernel `.o`, so dropping a new `test/*.cpp` file in is enough to pick it up; no Makefile changes needed.

Tests mutate `sim.host_state[...]`/`sim.device_state` etc. directly (all public) rather than calling `resize()`/`initialize_to_*()` after construction — those reallocate `host_state`/`device_state` to a new `n`, but the kernel objects (`compute_rhs_kernel` etc.) already baked in the *original* `n` at construction time via the `Kernel` base class, so calling them post-construction desyncs kernel loop bounds from buffer size and causes out-of-bounds device access. Push host-side edits to the device with `sim.device_state.copy_from_host(sim.host_state)` (or the relevant `DeviceVector`) instead.

### Rendering

The client renders 3D with a **fixed isometric orthographic camera** — the simulation animates, the camera never moves (no orbit, no pan, no perspective).

**Impostor spheres, not sphere meshes.** Each particle is *one camera-facing quad* drawn with `gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, n)`: attribute 0 is a static 4-vertex quad (`vertexAttribDivisor(0, 0)`), attribute 1 is the per-particle center (`divisor 1`), re-uploaded each frame straight from the server's position buffer via `bufferSubData`. The vertex shader offsets the quad in **view space** so it always faces the camera; the fragment shader reconstructs the sphere from the quad's local coords (`discard` outside the unit circle), shades it Lambert + specular from a fixed view-space light, and writes an **analytic `gl_FragDepth`** from the projected sphere-surface point so spheres occlude each other and the domain box correctly. This is the standard molecular-viewer approach: fewer vertices than instanced meshes, a silhouette that stays smooth at any zoom, and **no per-frame geometry generation at all** — which is why the JS `ParticleDrawer` was deleted and the "keep C++ and JS geometry in sync" hazard is gone. Depth testing is on; all geometry is opaque, so nothing needs sorting.

**Camera.** `buildCamera()` uses view direction `normalize(1,1,1)`, which is what makes the projection *truly* isometric (azimuth 45 deg, elevation ~35.26 deg: the three world axes project 120 deg apart, and equal world lengths along each axis project to equal screen lengths). World up is `+z` because gravity is `-z` server-side, so the `up` vector is `[0,0,1]` rather than GL's usual `+y` — that single flip is the whole cost of using the physics convention, and it lives in `mat4LookAt`. The orthographic extents are **fitted to the domain box's eight corners in view space** rather than hardcoded, then widened on one axis so the projection aspect matches the canvas aspect. Widening (never independent per-axis scaling) is what keeps spheres circular: the world-to-pixel scale stays uniform on both screen axes. Consequently `fitCanvasToDomain` was replaced by `fitCanvasToWindow` — the canvas just fills the window and the projection handles aspect.

`mat4Ortho`/`mat4LookAt`/`mat4TransformPoint` are hand-rolled column-major (`m[col*4 + row]`) rather than pulling in gl-matrix, keeping the client free of any build step or browser-side dependency.

**Overlay.** The domain box's 12 edges are always drawn (with a fixed camera and no perspective, the box is the only thing establishing scale and orientation). The `#gridButton` checkbox toggles the collision grid's cell boundaries **on the floor plane only** — deliberately not the full 3D grid, because cells are sized to roughly one particle diameter, so the checked-in config has ~100 cells per axis and drawing all three families of planes is opaque visual noise. On the floor it still shows the running simulation's actual cell size, which is the point of the overlay.

`ParticleDrawer` (`src/particle_drawer.{h,cpp}`) is now **dead code**: nothing calls it, it is still 2D, and the impostor path replaced it rather than porting it. `src/shader.h` is likewise an unused native-OpenGL GLFW-era helper; the actual rendering path is the WebGL2/JS one in `client/`.

`Graphics` (in `client/world.js`) also draws a red gridline overlay for the collision grid (`collisionGridSizeX`/`collisionGridSizeY`): `buildGridLineVertices(gridSizeX, gridSizeY, domain)` builds static line-segment geometry once per `setup()` (the grid doesn't move, unlike particles), in raw world coordinates spanning the actual domain, drawn via a second minimal `Shader` (position-only attribute, hardcoded red fragment output, no per-vertex color needed) and `gl.drawArrays(gl.LINES, ...)` before the particle draw call so particles render on top. `collisionGridSizeX`/`Y` are sent from the server over the WS protocol (see above) rather than hardcoded client-side, specifically to avoid a third copy of a magic number that's already manually duplicated once (C++ `ParticleDynamics` constructor) — the whole point of the overlay is to reflect the *actual* running simulation's grid.

**World-to-clip transform.** Both vertex shaders (particle and grid) take raw world-space vertex positions and map them to clip space via two uniforms, `uDomainMin`/`uDomainScale` (`clip = (world - domainMin) * domainScale - 1`), set once per `Graphics` instance from the domain bounds the server sends at `initialize`. Before this existed, the client assumed the simulation domain *was* clip space (`[-1,1]×[-1,1]`) — harmless while the domain defaulted to exactly that, but meant changing `domain:` in the config silently clipped/mis-scaled everything client-side with no server-side symptom. `Graphics`' constructor also calls `fitCanvasToDomain(canvas, domain)` (`client/world.js`) to size the canvas to the domain's aspect ratio at up to the full browser window size — independent x/y scale factors in the shader transform still yield circular particles because the canvas's pixel aspect ratio matches the domain's, so equal world-space distances map to equal pixel distances on both axes. `Client` registers exactly one `window` `"resize"` listener for the page's lifetime (delegating to `this.graphics?.handleResize()`) rather than one per `Graphics` instance, since `setup()` constructs a new `Graphics` on every `initialize` (including "Restart") and a per-instance listener would accumulate stale ones. The `#controls` checkboxes and `#hud` are both absolutely-positioned overlays (`client/index.html`) rather than in-flow content, specifically so they don't eat into the vertical space `fitCanvasToDomain` sizes the canvas against.

## Known issues

Both previously-listed issues were fixed on 2026-08-25 and are recorded here only so the fixes aren't
undone by someone reading old notes:

- **Header dependency tracking now exists.** The pattern rules pass `-MMD -MP` (`$(dep_flags)`) and the
  Makefile `-include`s the generated `build/obj/*.d`. Editing a widely-included header rebuilds exactly
  the objects that include it, so `make clean` is no longer required after a header change. Verified by
  touching `particle_dynamics.h` and confirming only `particle_dynamics.o` and `profiling_sim.o` rebuild.
- **`num_per_cell` is now clamped.** `fill_cells_kernel` `atomicSub`s its reservation back when
  `slot >= particles_per_cell`, so the counter reflects particles actually *stored* rather than
  particles that wanted in, and `find_neighbors_kernel` additionally `min`s the loop bound against
  `particles_per_cell`. Covered by `ParticleDynamicsTest.GridOverflowIsCountedAndStaysInBounds`, which
  forces the path with a 1x1 grid and `particles_per_cell = 2`. Negative control: reverting either half
  makes `compute-sanitizer --tool memcheck ./build/bin/test` report 2 out-of-bounds errors, so the test
  genuinely covers the regression rather than merely passing.

## GPU verification

This dev box has a real CUDA toolkit + NVIDIA GPU and `compute-sanitizer` installed — when changing kernel code (especially anything touching the collision grid's mark/compact/fill logic), actually build and run it rather than reasoning about correctness statically:
```
make profile && ./build/bin/profile             # sanity: should run to completion, print 4 nonzero timings
compute-sanitizer --tool memcheck ./build/bin/profile   # out-of-bounds / invalid access
compute-sanitizer --tool racecheck ./build/bin/profile  # shared/global memory race hazards
compute-sanitizer --tool initcheck ./build/bin/profile  # reads of uninitialized device memory
```

Hardware/toolchain as of 2026-08-25: CUDA 13.1 (`nvcc` at `/usr/local/cuda`), NVIDIA driver
580.173.02, GeForce RTX 2080 (TU104, `sm_75`, 8 GB). Known-good 3D baseline with the checked-in
`config.yaml` (32768 grains + 1 sled): `profile` prints ~35/13/1.0/0.23 ms for
grid/RHS/step/unpack over 100 steps, `make test` passes 6/6, and all three sanitizer tools report
zero findings. (The pre-3D 2D baseline, for comparison, was ~53/16/1.6/0.33 ms at 100k grains.)

**Real-time operating point.** Real time is the binding constraint (see "Current direction"), so the
config is sized to it rather than to a headline particle count. Measured on the 2080 at `dt=1e-3`
(1 ms/step budget), varying only `cube_length_*`:

| grains | ms/step | x real time |
|---|---|---|
| 32,768 (32³) | 0.486 | **2.06x** <- checked-in config |
| 46,656 (36³) | 1.155 | 0.87x |
| 64,000 (40³) | 1.381 | 0.72x |
| 91,125 (45³) | 2.26 | 0.44x |
| 343,000 (70³) | 8.44 | 0.12x |

Through the full server path (unpack + WebSocket) the checked-in config measures **1.86x real
time**. Note the scaling is *superlinear*: 4.6x the grains costs 12.6x the time between 19.7k and
91k, because `device_neighbors` (`n*27*k` ints) blows past the 2080's 4 MB L2. That is the wall to
attack before chasing bigger particle counts.

## Surrogate tooling (Python)

The simulator has no Python dependency; the analysis side does. It lives in `surrogate/` behind a
virtualenv so nothing leaks into the system interpreter:
```
python3 -m venv surrogate/.venv
surrogate/.venv/bin/pip install -r surrogate/requirements.txt
```
- `surrogate/make_design.py` — Latin-hypercube (scrambled Sobol when scipy is present) design over the
  throw/material parameters, written to `dataset/design.csv`. A tensor grid costs `k**d` and dies past
  d~4 — the existing `dt x max_force x restitution` sweep is exactly that design and already at its
  limit at three axes — whereas LHS decouples sample count from dimension.
- `surrogate/fit_surrogate.py` — Stage 1: fits the GPs, reports the truncation/regression error
  decomposition, per-mode R^2, uncertainty calibration and ARD sensitivity, and writes the
  imagination-vs-truth figure.
- `surrogate/plot_chaos.py` — the predictability figure (per-grain divergence vs field divergence,
  with the surrogate error and the measured floor marked).
- `surrogate/pod_study.py` — per-channel POD/SVD study: spectrum, energy captured, held-out
  reconstruction error vs rank, sample complexity (error vs number of training snapshots), and the
  per-checkpoint breakdown that revealed the momentum-decay effect. `--per-checkpoint` is the flag
  worth using; a single final-state number is misleading for momentum.
- `surrogate/dataset_io.py` — reader for `dataset/grids.bin`. Exposes `mass()`, `momentum()`, and
  `velocity()` (which guards the empty-node 0/0 divide), plus an `ok` mask built from the manifest's
  `status` column.

`surrogate/.venv/` and the generated `dataset/*.bin` / `dataset/*.csv` are gitignored;
`dataset/blob_throw.yaml` and `surrogate/requirements.txt` are tracked.

Sanity check the whole path with:
```
surrogate/.venv/bin/python surrogate/make_design.py --rollouts 8 --out dataset/design_smoke.csv
./build/bin/dataset dataset/blob_throw.yaml dataset/design_smoke.csv
surrogate/.venv/bin/python surrogate/dataset_io.py
```
Deposited mass per checkpoint should equal `grains * grain_mass` to ~1e-7 relative for every rollout —
that single check validates the binary layout (a wrong stride scrambles it) and the CIC deposit's mass
conservation at the same time.

## Client/browser verification (headless)

This dev box can drive the actual WebGL2 client headlessly — use this to verify client-side
changes (the `client/world.js` render path, the WS protocol) end-to-end instead of only checking the
wire bytes. **Playwright is installed globally** (`playwright --version`, currently 1.61.x; require
it from the global `node_modules`, it is *not* a `client/` dependency) and **system Chrome** is on
PATH (`/usr/bin/google-chrome`). WebGL works headless via SwiftShader — launch Chrome with
`channel: 'chrome'` and args `--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader`.
A Playwright script can then capture `console`/`pageerror` events and a canvas screenshot, which
renders the particle cube + red grid overlay + HUD (a useful visual smoke test).

Recipe: start the backend, then the static client server, then drive a browser at
`http://localhost:8080`:
```
setsid ./build/bin/world >world.log 2>&1 </dev/null &   # see gotcha below
(cd client && setsid node index.js local >client.log 2>&1 </dev/null &)  # :8080, WS -> localhost:8081
# then a Playwright .cjs that goto()'s http://localhost:8080, waits a few seconds, screenshots
```

**Gotcha:** launch long-running servers with `setsid ... </dev/null &` (detached from the shell's
process group). A plain `&` puts them in the Bash tool's process group, so they get killed when that
tool invocation's shell exits — the server appears to die "after one initialize" for no reason. The
incidental 404 in the browser console is just a missing `favicon.ico`, not a real error.
