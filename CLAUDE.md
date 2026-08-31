# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working agreements

### Record durable knowledge here, not in auto-memory

**Anything worth remembering across sessions belongs in this file** (or another checked-in doc), not
in Claude Code's auto-memory directory. Auto-memory lives under `~/.claude/` on this machine only,
and this machine may be wiped; `CLAUDE.md` is version-controlled and travels with the repo. When you
learn something durable — a build gotcha, an environment quirk, a decision and its rationale — write
it here in the same pass as the work, without being asked. Prefer updating an existing section over
appending a new one.

Corollary: if you find yourself about to write a memory file, write a CLAUDE.md section instead.

### Constraints

**The sandbox is disabled in this repository** (see "Sandbox and GPU access" below), so nothing
mechanically enforces the rules below. They are the only guardrail. Follow them unless the user
explicitly asks otherwise *in the moment* — a standing exception is never assumed.

- **Stay inside the repository.** Do not delete, move, or modify files outside
  `~/world`. Other repositories in `$HOME` are off limits. Scratch files go in the repo
  (gitignored) or `$TMPDIR` — never in a sibling project.
- **Never modify a git remote.** No `git push` (including `--force`), no creating/deleting remote
  branches or tags, no PR or release creation, no `git remote set-url`. Local commits are fine when
  asked for; publishing is always the user's action.
- **Never touch logged-in accounts or browser state.** No Chrome/Chromium/Firefox profiles, cookies,
  saved logins, session tokens, or browser-automation against authenticated sites.
- **Never read secrets or use keys.** No `~/.ssh`, `~/.gnupg`, `~/.aws`, `~/.config/gcloud`,
  `~/.netrc`, keyrings, or credential files — not directly, and not indirectly via a script,
  interpreter, or archive tool that would route around a deny rule. Do not read dotfiles in `$HOME`.
  (Repo-local project config — `.gitignore`, `.clangd`, `.claude/` — is ordinary project material and
  fine to read.)
- **Do not exfiltrate.** Repository contents, local data, and machine details do not go to external
  services. Network use is for fetching dependencies and documentation.
- **Do not reconfigure the harness.** Leave `~/.claude/settings.json`, hooks, and permission rules
  alone. Read them when diagnosing a tooling problem the user raised; never write them.
- **No privilege escalation or system changes.** No `sudo`, no system package installs, no changes
  outside the user account.
- **Confirm before irreversible local actions** — `rm -rf`, `git reset --hard`, history rewrites,
  overwriting uncommitted work. Look at what you are about to destroy first.

### Commits

Do not add a `Co-Authored-By: Claude` trailer or any other Claude Code attribution to commits in this
repo. Commit only when asked.

**Never `git add -A` in this repository.** The generated training sets are large and are gitignored by
directory (`dataset_heavy*/`, `decoded_heavy*/`, `dataset_dyn2/`, `dataset_dynamics/`, `decoded/`,
`decoded_uniform/`), but a newly named output directory will not be covered until a line is added, and
`dataset_heavy64/` alone is 16 GB. Stage explicitly, and check `git status --short` for `??` entries
before committing. Figures the README displays are tracked; intermediate figures it does not reference
are left on disk untracked.

## Naming, and the public README

**The project is named `world` (repository) and "World" (official name), as of 2026-08-27.** It was
previously called `real-time-particles`, and that string should not appear anywhere else. Three
places were easy to miss during the rename and are recorded here so they are not reintroduced: the
`~/world` path in the "Constraints" section above, the `world/.claude/settings.local.json`
reference under "Sandbox and GPU access", and `client/package.json`, which still carried the name
and description of the Google Cloud Node sample it was scaffolded from. The GitHub remote is
`git@github.com:alilasemi/world.git`.

Two consequences outside the repo. The positioning document in `~/job-search` keeps its original
filename, which still carries the former name; that file is outside this repo and was left alone.
And the Claude Code project directory changed with the working directory, so the transcripts under
`~/.claude/projects/` were merged forward into `-home-ali-lasemi-world` (session UUIDs do not
collide, so a plain copy was sufficient). The directory named after the old path was left in place
rather than deleted, and can be removed once nothing is needed from it.

**`README.md` is the public artifact and is written to a specific set of constraints. Keep them
when editing it.** The author's voice is the one in his IJMF 2026 paper
(`~/jcp-2026/context/ijmf-2025/main.tex`), and `~/jcp-2026/CLAUDE.md` holds the explicit rule list.
The ones that bite most often:

- **Plain hyphens only.** No em dash, no en dash, no `--` outside command-line flags.
- **No acronym that is not in the nomenclature of `surrogate/formulation.tex`** (ARD, FOM, GP, POD,
  QMC, RMS, SVD). In particular *do not write DEM*: say "dynamics simulation", "particle dynamics"
  or "contact dynamics". Spell out cloud-in-cell and material point method. Product and technology
  names (CUDA, GPU, WebGL2, YAML) are not covered by this.
- **No negative parallelism** ("not just X but Y", "it is not X, it is Y"). Watch for the
  substitutes too: an editing pass that mechanically replaced "rather than" with "and not" simply
  moved the tic. Vary the construction.
- **No bold for emphasis in body prose**, no contractions, no rhetorical questions.
- The banned-vocabulary list in `~/jcp-2026/CLAUDE.md` applies verbatim, including **genuinely** and
  **load-bearing**.
- **Every claim about the literature carries a numbered reference**, and every quantitative claim
  carries a measurement or a figure. The reference list at the bottom is checked: a script pass
  confirms that every `[[n]]` is defined and every definition is cited.
- **No strikethrough, no dev-log residue, no roadmap of completed items.** Status belongs in this
  file; the README states what is true now.
- **Never write "runout".** It is vague and collides with "rollout", which the README uses for
  autoregression. Say *front*, and *front radius* for the distance.
- **Never write "backend"** (web-developer register). Say *the solver*.
- **Never write "bit-identical"**; plain *identical* is what is meant.
- **No implementation detail below the level of algorithm and mathematics.** `atomicCAS`,
  `atomicAdd`, "atomic reduction", "device scalar", `uWebSockets`, `compute-sanitizer`,
  `memcheck`/`racecheck`/`initcheck` are all too low for the README. A compacted uniform grid
  whose storage grows with particle count is the right altitude; the four atomic passes that
  implement it are not.
- **Lead with the world model, and make both halves visible.** The showcase claim is that the
  repository holds a world model *and* the simulation it was trained on. The opening paragraph
  must begin with the world model, name it in the first line, and reach chaos only at the end.
  Section titles carry the split: "The solver" and "The world model".

**The README reports the 32,000-grain study at a 64x64x16 latent, not the first study.** See "The
32,000-grain study" below for the numbers and for the conclusions that changed. Anything quoting
0.174, 1.13x, a 0.154 floor, 3,375 grains, a 0.10 s interval, or momentum decaying to numerical creep
is from the superseded study and does not belong in the README.

**Two things are deliberately absent from the README and should stay absent.**

- **The parameters-to-outcome regression (formerly "Stage 1")** and everything derived from it:
  the 0.381 composed error, the 0.243 truncation floor, the per-mode coefficient of
  determination, the uncertainty calibration, the ARD relevance ranking, the 150-rollout pilot,
  and the figure `stage1_surrogate.png`. It was internal testing on too little data, and
  predicting a settled outcome from throw parameters is not a realistic setting. The code
  (`surrogate/fit_surrogate.py`) and the analysis in this file stay; the public write-up does
  not mention them. The `theta -> a(t)` baseline inside `fit_dynamics.py` belongs to the same
  family and is likewise omitted, so run it with `--skip-direct` for anything public.
- **`BackwardEulerPicardKernel`.** It never worked well. The code remains; the README describes
  semi-implicit Euler only.

## Current direction (as of 2026-08-25)

The project is being repositioned from "a real-time fluid simulation and rendering engine" to **a
GPU-native, real-time, interactive physics environment for embodied AI, plus a measurement of what is
actually predictable inside it**. The strategy documents driving this live *outside* this repo in
`~/job-search` (read-only reference: the positioning document there, whose filename still carries the
project's former name, is the live plan in its §3.0, and its `CLAUDE.md` holds the decision log). Read
them before proposing direction changes.

Decisions already argued through — do not re-litigate:
- **No reinforcement learning.** A prior PPO stack was removed (recoverable from git history at
  `52bd43b`…`0806084`). It is not coming back; a project that *ends* in an RL result invites
  interview questions on the weakest axis.
- **No automatic differentiation.** For chaotic, non-smooth granular contact the pathwise gradient
  has variance growing like `e^{λT}`; statistical observables are the differentiable objects, and
  zeroth-order estimation over parallel rollouts replaces AD.
- **3D is deliberately in scope** (reversing an earlier deferral): hard convert, z up, gravity `-z`,
  2D left in git history. Real time is the binding constraint, not particle count.
  *Status: landed 2026-08-25 — all five drivers, the wire protocol, the test suite AND the
  isometric renderer are 3D. Screenshots in `assets/screenshots/`.*
- **The surrogate** predicts a coarse **density field** (trilinear/CIC deposit, i.e. MPM's
  particle-to-grid transfer) from throw parameters, via POD + one Gaussian process per mode. If the
  POD spectrum decays slowly, escalate to a convolutional autoencoder — *not* shifted POD, which
  needs hand-designated reference frames and so doesn't generalize.
- **One POD basis PER CHANNEL, not a single joint basis over all four** (decided 2026-08-25). The
  latent has 4 channels (mass + momentum per axis, see `DensityGridKernel`), and a joint basis would
  require scaling them against each other. That scaling is not merely awkward, it is *nonphysical*:
  mass density is positive-definite while momentum density is signed, and momentum's dynamic range
  is far wider and more scenario-dependent. Asking one set of modes to span two incommensurable
  quantities makes the singular values meaningless — whichever channel is scaled larger dominates
  the spectrum, and the "energy captured" number stops describing anything real. Four independent
  bases also let each channel choose its own truncation rank, which mass (smooth, positive) and
  momentum (signed, rougher) will not agree on.
  *Marked as worth revisiting:* a joint basis is the only way to capture the physical coupling
  between mass and momentum, which a latent dynamics model arguably needs. Reconsider once the
  per-channel spectra are measured and there is evidence the coupling matters more than the
  scaling problem hurts.

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
`sqrt(2) = 1.414`, so pairwise/sqrt(2) is a valid proxy when fields aren't dumped. Comparing the
surrogate to the pairwise 0.245 instead would have wrongly suggested it was nearly optimal.

**CORRECTION (2026-08-28): that 1.46 is NOT empirical confirmation of anything, it is arithmetic.**
An earlier version of this note claimed the sqrt(2) relation was "confirmed empirically" because the
measured ratio came out at 1.46 rather than 1.414. For `E` independent zero-mean deviations, with the
about-mean RMS taken with divisor `E`, the ratio is exactly `sqrt(2E/(E-1))`, which for `E = 16` is
**1.4606**. The gap from 1.4142 is the finite-ensemble factor `sqrt(16/15) = 1.0328` and nothing
more. The tell was that the number came out identical to four digits in three unrelated
configurations. The relation still justifies the proxy; it just is not evidence about the physics.

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

**Stage 2 -- latent dynamics, `(a(t), theta_material) -> a(t+Delta)` rolled out autoregressively**
(`surrogate/fit_dynamics.py`, public figures `assets/screenshots/rollout_error.png` and
`rollout_fields.png` via `surrogate/plot_rollout.py`). Final
configuration: `dataset_dyn2` (1536 rollouts x 10 checkpoints, `Delta = 0.10` s), mass rank 96 +
momentum rank 24 per axis (latent 168, input 171), 900 training transitions, isotropic Matern.

| at t = 1.0 s | first attempt | final |
|---|---|---|
| reconstruction floor (cost of the latent alone) | 0.173 | **0.108** |
| one step from truth | 0.177 | 0.109 |
| **autoregressive rollout** | **0.445** | **0.174** |
| persistence baseline | 1.762 | 1.531 |
| compounding penalty | 0.268 | **0.065** |
| irreducible floor for that config | 0.168 | **0.154** |
| rollout / floor | 2.65x | **1.13x** |

**Always decompose rollout error into three separately measurable terms; which one dominates changes
the answer to "what should I fix" completely.**
- *truncation* -- encode the truth and decode it. Independent of the regressor.
- *one-step regression* -- teacher-forced error minus the truncation floor.
- *compounding* -- rollout error minus teacher-forced error.

At rank 16 the budget was truncation 0.401 / regression 0.000 / compounding 0.063, so truncation was
86% of the error and **more data would have done nothing** -- teacher-forced error equalled the
reconstruction floor *to four decimal places*. Raising the rank to 96 fixed truncation and flipped the
budget to compounding-dominated (0.173 / 0.004 / 0.268). Only then did the fixes below apply.

**What fixed compounding**, in order of effect:
1. **Doubling `Delta`** (0.05 -> 0.10 s) halved the compounding events, 19 -> 9. Dominant.
2. **Narrowing the theta ranges ~40% per axis** improved *both* terms: the same 900-point GP subset
   covers a smaller domain more densely (one-step latent error 0.128 -> 0.102 despite a longer, harder
   step), and less state diversity means rank 96 retains 99.74% rather than 99.43%, dropping the
   reconstruction floor 0.173 -> 0.108.
3. **More rollouts** (256 -> 1536) bought a better basis (14,160 training snapshots) and a
   well-covering subset drawn from 12,744 available transitions.

**An exact GP cannot consume a large database**: O(n^3) per likelihood evaluation makes anything past
~1000-1500 training points prohibitive. What a bigger database buys is a better POD basis and a
denser-covering *subset*. Reducing per-step error at fixed n therefore means shrinking the domain n
must cover -- which is why the theta narrowing was the effective lever, not the row count.

**Cost, measured:** 21 min to generate 1536 rollouts; 561 s to fit 168 GPs at n=900; ~9.4 min for
basis + fit + evaluation.

**Cost of a model step versus the solver, measured 2026-08-27, and the honest verdict: the model
is NOT yet the faster of the two.** One autoregressive step advances 0.10 s of simulated time and
costs **42 ms** for a single trajectory, or **10.5 ms** per trajectory in a batch of 120 (168 GPs,
900 training points, 171 inputs). The solver covers the same 0.10 s for the 3375-grain training
configuration (`dt=1e-4`, so 1000 steps) in **73 ms**. So the speedup is 1.7x unbatched and 7x
batched, which is not a headline. State it that way, and state the reason it may still be the
right trade: the model's cost is independent of grain count and timestep, while the solver's grows
with both, so the comparison moves toward the model exactly as the simulation gets harder. Do not
quote a speedup figure without the problem size attached.

**Two honest caveats.** The accuracy was bought with generality: the theta box is ~40% narrower per
axis, i.e. ~7% of the original volume in 7 dimensions, so 0.174 must never be quoted without stating
the domain. And the remaining compounding (0.065) is now comparable to the gap to the floor; reducing
it needs a multi-step (rollout-objective) loss, which scikit-learn's GP cannot express.

The error *peaks* at t ~ 0.3-0.4 s and then falls -- the impact and spreading phase is genuinely the
hardest part of the trajectory, after which the pile settles and prediction gets easier. A physics
signature, not a model artifact.

**The floor is configuration-specific and must be re-measured, never reused.** The 0.168 figure came
from the wide-theta config at t = 2.0 s and is *invalid* for the narrowed config at t = 1.0 s, where
the correct value is 0.154 (pairwise 0.225, ratio 1.463 against sqrt(2) = 1.414). `fit_dynamics.py`
takes `--floor` as an argument rather than hardcoding it: reusing the stale value flattered the result
from 1.13x to 1.04x.

**On the direct competitor.** A `theta -> a(t)` regression refit per horizon (Stage 1 applied at every
time) *beat* the rollout in the first dynamics study. That is expected, not a defect: with a single
action at t=0, theta is a complete description of the trajectory, so `a(t)` adds no information and the
rollout can only lose some through truncation while accumulating its own error. A dynamics model earns
its place when theta is unknown (state observed rather than parameterized), when the horizon exceeds
the training range, or when **actions arrive mid-trajectory** -- where no single theta exists and the
direct regression is not definable. `--skip-direct` omits it for that reason.

**Decoding a rollout back to particles (`surrogate/decode_particles.py`, figure
`assets/screenshots/decoded_particles.png`).** Closes the loop: observed initial field ->
project -> autoregressive latent rollout -> decode each snapshot to fields -> **sample particle
positions and velocities**. Run it on a held-out rollout so the test point interpolates the design by
construction.

**It is a sampler, not an inverse.** Interpolating particles onto a grid is many-to-one, so a field
has infinitely many consistent particle configurations and sampling draws one. `decode(encode(x))` is
therefore *not* `x` -- it reproduces the FIELD, not the configuration. Describe it that way.

Algorithm: clamp negative density (a truncated POD reconstruction is not non-negative); rescale to the
exactly-known total mass `n_p * m` so the decode conserves mass even though the reconstruction does
not; draw node indices multinomially with probability proportional to nodal mass; place uniformly
within the node's cell; assign velocity by interpolating momentum and mass at the sampled position
with the *same* hat weights used to deposit, then dividing.

**Thresholding low-density nodes is not optional -- it fixes two artifacts at once.** A truncated POD
reconstruction *rings*: it leaves densities far from the material that are individually negligible but,
summed over 16384 nodes, attract enough grains to produce a diffuse halo across the whole domain. Worse,
those grains sit where density is near zero, so `rho*u / rho` assigns them enormous velocities and the
sampled kinetic energy is inflated by more than an order of magnitude. Dropping nodes below 2% of peak
density (default `--threshold 0.02`, discarding 5-10% of mass, restored by renormalisation):

| at t = 0.70 s | no threshold | threshold 0.02 |
|---|---|---|
| spread error (m) | 0.0921 | **0.0063** |
| centroid error (m) | 0.0330 | **0.0126** |
| sampled KE (true 3.62) | 141.4 | **7.6** |

With it, centroid error is 0.002-0.026 m and spread error 0.005-0.034 m against a grain diameter of
0.02 m -- sub-diameter accuracy on the bulk moments -- and sampled kinetic energy tracks the true value
to ~15% while the material is moving.

**The output is NOT a valid simulation state**, and this is measured, not assumed: 70% of sampled
grains sit closer than one diameter to a neighbour and the median nearest-neighbour distance is 0.75
diameters, where a packed state would show ~0 overlaps and a ratio near 1. Node spacing is ~3 grain
diameters, so ~30 grains share a node and uniform placement gives a Poisson-like arrangement rather
than a packed one. Handing this to the solver would produce enormous contact forces. Making it
re-initializable needs Poisson-disk sampling with minimum separation `2r`, or a short damped
relaxation -- and that capability (fast-forward with the surrogate, then hand back to the solver) is
the reason it would be worth doing.

**Velocity fluctuations within a node are lost.** Only mean momentum is stored, not the second moment
`rho*u*u`, so granular temperature is discarded. Adding that channel would recover it.

One diagnostic bug worth remembering: the grid kinetic energy is `sum_a |rho*u_a|^2 / (2 rho_a)` --
momentum divided by density *at each node*. An earlier version divided by the mean density, which is
dimensionally wrong and announced itself as energy ratios of 10^3.

**The decoded state IS now re-initializable (`--sampler poisson`, the default), and this was verified
by restarting the solver from it, not by asserting it.**

`poisson_disk_sample()` does dart throwing with proposals drawn from the density field, accelerated by
a spatial hash of cell size `2r` so a candidate is tested only against the 27 neighbouring cells.
Bridson's algorithm is the usual Poisson-disk method but produces a *uniform* density; here the density
is prescribed by the field and the disk radius is fixed by physics, so the proposal distribution
carries the density instead. Result: **0.000 grains closer than one diameter**, median
nearest-neighbour distance 1.15 diameters, all 3375 placed.

`init_type: file` + `initialization.state_path` on the C++ side loads a flat binary (int32 `n`, then
`n*kStateStride` float32 in `host_state` layout) written by `decode_particles.py`, closing the loop:
simulate -> encode -> predict -> decode -> **simulate again**.

**The A/B test that proves the sampler matters.** Restarting from the two states, energy at t=0:

| sampler | overlapping pairs | E(0) | behaviour |
|---|---|---|---|
| uniform | 70% | **14893.5** | dumps 90% of it within 0.01 s -- a violent elastic burst |
| poisson | **0%** | **1270.8** | monotone decay, no transient |

The 11.7x excess is spurious *elastic* energy stored in overlapping contacts, converted immediately
into kinetic energy. Over a full second from the Poisson state the energy is **monotonically decreasing
throughout** (1270.8 -> 78.3), which is the signature of a state the solver accepts.

**The trade, stated:** enforcing separation costs bulk-moment fidelity, because the sampler cannot
exceed the local packing limit and therefore spreads grains outward where the predicted field demands
more density than physically fits. At t=1.0 s, centroid error 0.025 -> 0.057 m and spread error
0.011 -> 0.045 m going from uniform to Poisson. Use `--sampler uniform` for visualization and bulk
statistics, `--sampler poisson` when the state must be handed back to the solver.

Note also that a field demanding density above the packing limit is *locally unphysical*, so the
sampler failing to place grains there is information, not a defect -- `placed` vs `requested` is
reported for that reason.

**Bug worth remembering: never clip positions after Poisson-disk sampling.** The first version clipped
candidates into the domain afterwards; the pile rests on the floor, so a large fraction of proposals
fell below it and clamping them all to `z = z_min` collapsed a layer into one plane, destroying the
separation the sampler had just enforced (measured: 36% of pairs overlapping despite a correct disk
test). Out-of-domain candidates must be *rejected*, and a grain centre must stay a full radius clear of
each wall or it is half-buried in it. A chaotic system
has an outcome spread no model can beat, and the predictability-horizon measurement (epsilon-perturbed
ensemble) is what supplies it. If that spread is ~20-25%, a 24% surrogate is essentially optimal and
the story is complete; if it is ~5%, there is real modeling headroom. **Do not quote surrogate accuracy
without that denominator.**

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
   packing. Compare `assets/gifs/frictionless_collapse.gif` (mu=0: the material spreads until it
   reaches the side walls) with `frictional_collapse.gif` (mu=0.5 + jitter: the front arrests at a
   median radius of 0.55 m against 0.87 m). The same knob supplies the epsilon-perturbed ICs the
   predictability measurement needs.

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

## The 32,000-grain study (2026-08-28), which is what the README now reports

**The POD study, the Stage 1 and Stage 2 results, the predictability measurement and the decoding
notes recorded under "Current direction" above all belong to the FIRST study: 3,375 grains at
`dt=1e-4`, latent 32x32x16 over a 2 m domain, `Delta = 0.10` s, horizon 1.0 s. They are kept, they
are still internally valid, and they are no longer what the README describes.** (The solver facts in
that section, the contact model, the energy diagnostic, the float32 position stall, are not
study-specific and still apply.) The reason for a second study is that the
solver's headline figure was 32,768 grains while the model was trained on 3,375, so the README read
as though one system was being described when there were two. The new configuration is
`dataset/blob_heavy64.yaml`: 32,000 grains (40x40x20) at `dt=1e-3`, latent 64x64x16 over a 20 m
domain, `Delta = 0.20` s, horizon 2.0 s. At 0.617 ms/step it runs at **1.62x real time**, so the
training system and the interactive system are now the same system.

### The identity that governs the timestep, and why raising the mass cannot help

This is the single most useful thing to carry forward. Peak interpenetration on impact obeys

    overlap / radius  =  v * S * dt / (2 * pi * radius)

with `S` the number of steps per contact period. At fixed impact speed `v` and fixed `dt`, overlap is
**proportional** to `S`, because mass enters the contact period and the overlap identically. So
heavier grains buy a longer contact period and pay for it one-for-one in interpenetration. At
`radius = 0.01`, demanding `S = 20` at `dt = 1e-3` gives an overlap of **1.4 radii**, i.e. grains
passing through one another. **The radius is the only lever.** For calibration, the checked-in
interactive config runs at `S = 8.9` and an overlap of 0.44 radii, and the first study ran at
`S = 28` and 0.18 radii.

Probe record, kept in `probe/*.yaml` (all 32,768 grains, `dt=1e-3`, all stable with monotone energy
and no grid overflow):

| probe | radius | max_force | overlap | deposit depth | wall clearance |
|---|---|---|---|---|---|
| p1 | 0.03 | 12 kN | 0.36r | 1.85 dia | tight |
| p2 | 0.05 | 50 kN | 0.28r | 2.18 dia | 1.73x |
| p3 | 0.07 | 100 kN | 0.24r | 2.51 dia | 1.72x |
| p5 (chosen) | 0.05 | 50 kN | 0.20r | **2.96 dia** | **2.08x** |

In grain-diameter units p1 to p3 behaved identically, which confirms the scaling and means the choice
was free. p5 differs from p2 by flattening the blob from a 32^3 cube to 40x40x20: a cube is 3.2 m
tall, which forces a tall z domain and leaves the 16 latent z-nodes spanning 2.5 diameters each.
`c_*.yaml` are worst-corner probes (fast diagonal throw, downward, heaviest grain); the worst cleared
the side walls by 1.37x with no grain within two diameters of one.

Two further design points. The **grain mass range is set by the timestep, not by taste**: `k = 1e6`
N/m, so 20.3 kg gives exactly 20 steps per contact and 45.5 kg gives 30 (`--preset heavy` in
`make_design.py`, which leaves the original preset untouched). And the **throw velocities are ~10x
the first study's on purpose**: there the throw was +/-0.7 m/s against a 4 m/s impact, so the ACTION
was a small perturbation on a splat; here it moves the deposit centroid by ~10 grain diameters.

### The floor depends on the LATENT RESOLUTION too, not just theta and horizon

Measured with 16-member ensembles at fixed theta (`chaos/ensemble_heavy64.yaml`, and the same with
`density_grid_size_x/y` varied):

| latent nodes | node spacing | floor | field predictability horizon |
|---|---|---|---|
| 32 | 6.45 dia | 0.0198 | 0.96 s |
| 48 | 4.26 dia | 0.0274 | 0.88 s |
| 64 | 3.17 dia | **0.0345** | 0.84 s |
| 96 | 2.11 dia | 0.0478 | 0.82 s |

A coarser grid averages more grains per node, so realization-to-realization fluctuations average
away and the floor drops. **A low floor is therefore not automatically a sign that the physics is
reproducible; it can be a sign that the latent is discarding information.** 64 nodes was chosen
because it restores the first study's 3.17 diameters per node and makes the comparison like for like.

The eightfold floor difference between the studies (0.156 against 0.0199 at 32 nodes) factors
cleanly, and the factorisation is worth keeping: the ratio of per-grain scramble to node spacing is
2.70 in the first study against 0.33 here, a factor of 8.2, against a measured floor ratio of 7.8.
That splits into **4.1x from physics** (grains scramble 8.72 diameters there against 2.14 here,
because the first study dropped the blob 32 to 50 diameters and this one drops it 5) and **2.0x from
the grid**. Refining the grid recovers only the second factor.

*Do not try to predict the floor absolutely from `min(1, scramble/spacing)/sqrt(grains per node)`.*
That sketch overshoots both studies (0.049 against 0.020, and 0.55 against 0.156) because an
occupancy threshold of 1% of peak counts the diffuse tail and undercounts grains per node. The ratio
argument holds; the absolute formula does not.

### Results, and the conclusions that changed

At t = 2.0 s, nine autoregressive steps, 120 held-out rollouts, rank 96 mass + 24 per momentum axis
(`surrogate/stage2_heavy64.joblib`, figures `*_heavy64.png`):

| quantity | 64 nodes | 32 nodes | first study (t = 1.0 s) |
|---|---|---|---|
| irreducible floor | **0.0345** | 0.0199 | 0.156 |
| autoregressive rollout | **0.153** | 0.138 | 0.174 |
| rollout / floor | **4.43x** | 6.93x | 1.13x |
| truncation | 0.0576 | 0.0346 | 0.108 |
| truncation / floor | **1.67x** | 1.74x | 0.70x |
| one-step regression | 0.0020 | 0.0034 | 0.001 |
| compounding | 0.0930 | 0.0999 | 0.065 |
| persistence | 1.132 | 1.115 | 1.531 |
| mass variance retained at rank 96 | 0.9985 | 0.9991 | 0.9974 |

- **The 1.13x headline does not survive, and that is the honest outcome.** It was flattered by a floor
  inflated by a small, noisy assembly. At a resolved latent the model is 4.4x the floor, so there is
  real headroom.
- **Truncation/floor is invariant to resolution** (1.74x against 1.67x): quadrupling the nodes raised
  truncation and the floor by nearly the same factor. So **rank, not grid size, limits the
  representation.** Raising the rank is the fix.
- **Compounding is invariant in absolute terms** (0.100 against 0.093, and identical at every horizon)
  because it accumulates latent-space step error and the latent dimension is unchanged. It is 61
  percent of the error, so a **multi-step objective is the priority**, and the exact GP cannot express
  one.
- **The one-step regressor is essentially exact**, 0.002 of 0.153. More training data would do nothing.
- **The rollout error rises monotonically** instead of peaking at impact and falling. Only the ONE-STEP
  error peaks (0.105 at t = 0.4 s down to 0.060). The old "impact is the hardest part" line is a
  statement about the one-step map only; asserting it of the rollout is wrong here.
- **Momentum does not decay to numerical creep.** The ratio to the mass scale falls to 0.151, not
  0.018, so "the settled state is density alone" is false for this configuration. Note the convention:
  `pod_study.py` reports `RMS |rho*u| / RMS |rho|` using the momentum vector MAGNITUDE, which is
  `sqrt(3)` larger than pooling the three components separately. Quoting the two conventions side by
  side in one paragraph is a mistake already made once.
- **Linear compressibility is confirmed more strongly:** quadrupling the node count cost 0.0006 of
  retained mass variance at rank 96. The autoencoder escalation stays unnecessary.
- **momentum_z of the SETTLED field is high-rank**: 87 modes for 90% of variance against 12 for x and
  y. Not a contradiction with the fit retaining 0.988 at rank 24, because the fit's basis spans all
  ten checkpoints and is dominated by the smooth flight phase, while `--snapshots final` sees only
  slow settling.

### Decoding at this grain count

Both samplers redone on the 64-node model (`decoded_heavy64_*`, figure `decoded_particles_heavy64.png`).
The trade is the same as before but far sharper, and one new failure mode appears:

| at t = 2.0 s | uniform | minimum separation |
|---|---|---|
| grains closer than one diameter | 94% | **0%** |
| median nearest-neighbour distance | 0.56 dia | 1.03 dia |
| energy on restart | 3.59e7, loses 86% in 0.1 s | 4.75e6, loses 1.9% |
| centroid error | **0.026 m** | 0.203 m |
| spread error | **0.045 m** | 0.584 m |

**The minimum-separation sampler now fails to place every grain**: 27,091 of 32,000 at t = 0.6 s,
recovering to all 32,000 once the deposit spreads. A field demanding density above the packing limit
is locally unphysical, so a shortfall is information rather than a defect. The first study placed all
3,375 and never exposed this.

Threshold ablation, re-run: dropping nodes below 2% of peak takes the spread error from 0.079 m to
0.045 m and the sampled kinetic energy from 53,314 to 10,739 against a true 6,480.

**Cost:** the Poisson decode is now slow, ~149 s per snapshot against seconds before, because dart
throwing rejects candidates and there are ten times the grains. Budget ~25 min for ten snapshots.

### Cost of a model step versus the solver, like for like

One autoregressive step advances 0.20 s and costs **44 ms** for a single trajectory or **11.2 ms** per
trajectory in a batch of 120, against **123 ms** for the solver to cover the same interval for the
same 32,000 grains. So 2.8x unbatched and 11x batched. Exact GP inference over 900 points and 168
outputs is the cost. State it as a modest speedup, and state that the model's cost is independent of
grain count and timestep while the solver's grows with both.

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

- Client sends `"initialize"` -> server allocates a new `ParticleDynamics`, then sends a fixed sequence of one-shot BINARY messages, each read by one step of the client's `Client.onmessage` state machine (states 0→5) before it enters the steady loop: `int32 n`, three `int32`s `collision_grid_size_x/y/z`, `int32 num_triangles` and `float particle_radius` (rendering constants), 6 floats `x_min/x_max/y_min/y_max/z_min/z_max` (domain bounds — same ones `is_stable()` checks server-side; the client uses these for its view volume and to count particles currently inside the domain vs. the total, shown on the HUD), then `n*kDim` floats of initial `(x, y, z)` positions. **Message sizes for the checked-in `config.yaml`, n=32768: 4, 12, 4, 4, 24, 393216 bytes**, then `(n*kDim + 6)*4 = 393240` per `"run"` — re-verified end-to-end against the running server on 2026-08-27. (An earlier note here recorded n=32769, from a period when the cube was accompanied by a single "sled" particle; that material is gone, see the comment at `src/sim_config.h:94`, and the cube is exactly 32^3.)
- Client sends `"run"` -> server calls `sim->take_step()` `config.steps_per_frame` times (10 by default), then checks `sim->grid_overflow_count()` once (see "Collision grid" below) -- a nonzero count prints one line and `exit(1)`s the whole server rather than continuing to serve a diverged sim -- then unpacks state and sends back the updated `(x, y, z)` float buffer.

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
`config.yaml` (32768 grains, i.e. exactly 32^3, with no extra particle): `profile` prints
~35.8/14.6/1.03/0.19 ms for
grid/RHS/step/unpack over 100 steps (re-measured 2026-08-27, three runs, spread under 2%, i.e.
0.514 ms/step and 1.95x real time), `make test` passes 11/11, and all three sanitizer tools report
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

### Sandbox and GPU access

**Claude Code's Bash sandbox must be disabled in this repo or the GPU is invisible.**
`world/.claude/settings.local.json` carries `{"sandbox": {"enabled": false}}`, while
`~/.claude/settings.json` keeps `sandbox.enabled: true` so every *other* project stays sandboxed.

Symptom when this is wrong: `cudaMalloc ... failed: no CUDA-capable device is detected`, and
`nvidia-smi` reporting it "couldn't communicate with the NVIDIA driver", even though the driver is
loaded and the card is on the PCI bus. Diagnose with `ls /dev/nvidia0` — a sandboxed session sees an
empty synthetic `/dev` with no `nvidia*` nodes at all.

Three narrower approaches were tried and measured; all are dead ends, so don't re-propose them:
- `sandbox.excludedCommands` is both unreliable (Claude Code doesn't consistently consult it before
  sandboxing — upstream issue #17821, closed as not-planned) and leaky: when it *does* fire, the
  **entire** Bash invocation runs unsandboxed regardless of where the listed command sits, so
  `rm -rf ~ ; ./build/bin/profile` escapes. It also can't help `compute-sanitizer`, which is a shell
  wrapper that launches the target as a grandchild.
- `sandbox.filesystem.allowWrite` on `/dev/nvidia*` does nothing — it grants permission to a path but
  cannot materialize a character device inside the sandbox's synthetic `/dev`.
- `sandbox.filesystem.disabled: true` also fails: the sandbox still builds its own minimal `/dev`.
  It surrenders all filesystem isolation and yields no GPU, so it is strictly a regression.

Two consequences of running unsandboxed: `-arch=native` correctly detects `sm_75` (no `GPU_ARCH=`
override needed — and note that under the sandbox it *silently* fell back to a default arch with exit
0, producing a mis-targeted binary rather than an error), and the "Constraints" section above is the
only thing limiting filesystem and network access.

## Surrogate tooling (Python)

### Memory: the datasets are bigger than they look, and this caused two OOM kills

**`grids.bin` is 4 GB at a 32x32x16 latent and 16 GB at 64x64x16, on a 30 GB machine.** Two mistakes
in `dataset_io` put two Python processes at ~29 GB RSS and got them killed by the OOM killer
(2026-08-28, visible in the kernel log):

1. `np.fromfile` read the whole array into **anonymous** memory, which the kernel cannot evict.
2. Every consumer then wrote `data.grids[data.ok]`, which is **boolean fancy indexing and therefore
   always allocates a full second array**, even though the mask is all-True in every dataset
   generated so far. 16 + 16 = 32 GB.

Both are fixed: `load()` now **memory-maps** by default (file-backed page cache, which is reclaimable
and shows in RSS without being a hazard), and **`Dataset.usable()`** returns the array itself when no
rollout has to be dropped. Peak RSS for loading and slicing the 16 GB set went from ~29 GB to 35 MB,
and the fit's heaviest real step from 23 GB to 4.2 GB.

Rules that follow:
- **Never write `data.grids[mask]`.** Use `data.usable()`, or pass an index and slice inside the
  consumer. `build_bases` already took an index for exactly this reason and `main()` was defeating it.
- Basic slices of a memmap (`g[:1416:2, ::2, 0]`) are views and cost nothing; an integer-array index
  (`g[train_index, ::stride, channel]`) materialises just that slice, which is the intent.
- `--basis-stride 2` halves the basis snapshots and is the right first lever at 64 nodes.
- **`/usr/bin/time -v` on a representative slice answers "why is this 23 GB" in thirty seconds.**
  Treating tight headroom as a scheduling problem, which is what I did first, is the wrong response.

### Three traps that cost real time in this session

- **`python -u` for anything long and backgrounded.** Python block-buffers stdout to a file, and
  `fit_dynamics.py` only flushes the "building basis" line, so a 40-minute fit showed no progress
  after basis construction. RSS and CPU time were the only honest signals. Use `-u`.
- **A `while pgrep -f <pattern>` watcher never exits, because `pgrep -f` matches the watcher's own
  command line.** This is the same self-match trap as the `pkill` one under the headless-capture
  section, from the other direction, and both watchers written this session were dead on arrival.
  Wait on the PID: `while kill -0 <pid>; do sleep 20; done`.
- **Hardcoded plot limits and annotations silently corrupt a second study.** Three instances found:
  `ylim=(0, 0.62)` in `plot_chaos`, `ylim=(0.08, 2.6)` in `plot_rollout` (both clipped the new curves
  off the axis), and the `"...and is numerical creep here"` annotation in `pod_study`, which asserted
  an interpretation that was false at the new ratio. All three are now data-driven. **Prefer an
  annotation that prints the measured number over one that interprets it.**
- **`pod_study.py --per-checkpoint` costs 4x and the FIGURE does not need it.** The momentum-ratio
  panel uses `ratios`, computed unconditionally; the flag only adds the held-out error table. 3 min
  without it against 15 with.

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
- `surrogate/formulation.tex` — self-contained formal statement of the encoder, POD/SVD reduction and
  GP regression, with every symbol and dimension defined. Build with `pdflatex formulation.tex` (twice,
  for references). Read this before modifying the surrogate; it is the specification.
- `surrogate/fit_surrogate.py` — Stage 1: fits the GPs, reports the truncation/regression error
  decomposition, per-mode R^2, uncertainty calibration and ARD sensitivity, and writes the
  imagination-vs-truth figure.
- `surrogate/fit_dynamics.py` — Stage 2: the latent dynamics model
  `(a(t), theta_material) -> a(t+Delta)`, rolled out autoregressively, against three baselines.
- `surrogate/plot_chaos.py` — the predictability figure (per-grain divergence vs field
  divergence). It deliberately draws NO model error: the comparison against the realization
  scatter belongs with the model, in `plot_rollout.py`. The surrogate/truncation/floor lines that
  used to be annotated here were removed on 2026-08-27.
- `surrogate/measure_floor.py` — the irreducible scatter, from `chaos/final_fields*.bin`. Prints the
  scatter about the ensemble mean (the floor), the pairwise difference, and their ratio. This used to
  be done ad hoc, which is how a stale floor got reused; validated against the first study's recorded
  0.154 / 0.225 / 1.463 before being applied to new data.
- `scripts/probe_outcome.cjs` — deposit geometry off the wire in grain diameters, with wall clearance.
  The figure that decides whether a candidate configuration is usable: a deposit touching a wall is
  clipped, and a model fitted to clipped outcomes learns which wall was hit.
- `surrogate/plot_rollout.py` — both public figures for the dynamics model, from the artifacts
  `fit_dynamics.py --save` writes, so neither needs a refit (a refit is ~10 min; rolling the
  fitted step maps forward is ~25 s). `rollout_error.png` is error vs horizon on a LOG axis (the
  persistence baseline is 10x the model, and a linear axis flattens the three curves that matter),
  and `rollout_fields.png` decodes the rollout back to the mass field for the median and worst
  held-out rollout beside the simulation. The second figure is the strongest artifact the project
  has: at t=1.0 s the median rollout is visually indistinguishable from the simulation, and the
  worst predicts a compact pile where the simulation made a broader ring.
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

## Client/browser verification, and recording the README figures (headless)

This dev box can drive the actual WebGL2 client headlessly — use this to verify client-side
changes (the `client/world.js` render path, the WS protocol) end-to-end instead of only checking the
wire bytes, and to record the animations and stills the README uses. **Playwright is installed
globally** (`playwright --version`, currently 1.61.x) and **system Chrome** is on PATH
(`/usr/bin/google-chrome`).

**Headless Chrome reaches the real RTX 2080, and this is worth about two orders of magnitude per
pixel (2026-08-31).** The long-standing recipe here was SwiftShader (`--use-gl=angle
--use-angle=swiftshader --enable-unsafe-swiftshader`), which renders 32k impostor spheres in
software at ~4.6 s for a 900x900 frame, i.e. ~5.7 s per megapixel. Passing `--use-gl=angle
--use-angle=gl --ignore-gpu-blocklist --enable-gpu` instead gets `ANGLE (NVIDIA Corporation, NVIDIA
GeForce RTX 2080/PCIe/SSE2, OpenGL ES 3.2)` with no display server, and a 12.7-megapixel 3840x3310
frame then costs 0.55-0.7 s for a whole capture iteration -- solver step batch, render, screenshot
and PNG encode together -- so the rasterizer has stopped being the bottleneck rather than merely
getting faster. `--use-angle=vulkan` also reaches the card; `--use-gl=egl` silently falls back to
SwiftShader, so **confirm the backend rather than assuming it** — read
`WEBGL_debug_renderer_info`'s `UNMASKED_RENDERER_WEBGL`, which `capture_video.cjs` prints on every
run for exactly this reason. `capture_gif.cjs` still asks for SwiftShader and was left alone,
since its clips are small and it is the recipe the README figures were recorded with.

Four checked-in Node scripts do this and are the thing to reuse rather than re-deriving:

- **`scripts/capture_gif.cjs <out.gif> [frames] [fps]`** — records an animated GIF of the live
  client. `CAPTURE_FRAME_DIR` keeps the full-resolution PNGs so a clip can be re-encoded or
  trimmed without re-simulating; `CAPTURE_SCALE`, `CAPTURE_CROP`, `CAPTURE_WIDTH`/`HEIGHT`
  override the encode.
- **`scripts/capture_screenshot.cjs <out.png> [frames] [--grid]`** — one still, after a known
  number of frames, optionally with the collision-grid overlay on.
- **`scripts/capture_video.cjs <out.mp4> [frames] [fps]`** — records an H.264 MP4 for the
  showcase site: GPU-rendered, supersampled, 60 frames per second, 1x real time. See "Recording
  real-time video" below.
- **`scripts/measure_deposit.cjs [seconds]`** — speaks the binary protocol directly (no browser)
  and reduces the final positions to percentiles of radial extent and height. This is the cheap way
  to get a *number* out of a scene without adding a C++ driver.

Four things about the capture path are decisions, not accidents, and re-deriving them costs an hour:

1. **The capture must GATE the client's loop, not sample it.** `page.addInitScript` wraps
   `WebSocket.prototype.send` and holds every outgoing `"run"` until the script releases one, so
   exactly one step batch, one render and one screenshot happen per iteration. Without the gate the
   client outruns the capture badly: measured, a naive "screenshot every time the HUD sim time
   changes" loop got 4 frames spanning t=0.12 s to t=8.36 s, because the client renders far faster
   than Playwright screenshots. With the gate, frames are spaced by exactly
   `steps_per_frame * dt` and the recording is independent of rasterizer speed.
2. **Use `page.screenshot`, never `locator.screenshot`.** The locator variant first waits for the
   element to be stable across two animation frames; a software-rasterized frame of 32k impostor
   spheres takes long enough that this times out at 30 s every time. The canvas fills the viewport,
   so a page screenshot is equivalent.
3. **Hide the HUD for anything published, and set checkbox state by property.** The HUD's
   real-time ratio under SwiftShader is dominated by the rasterizer (it reports 0.31x and a WebGL
   render of 4622 ms while the solver is at 1.3 ms), which is actively misleading in a figure. Hide
   with `visibility: hidden`, not `display: none`, so the HUD text stays readable as the frame
   clock. A hidden `#gridButton` cannot be clicked even with `force: true`, so set
   `.checked = true` in `page.evaluate` instead.
4. **Resolve Playwright from `npm root -g`.** It is a global install under nvm
   (`~/.nvm/versions/node/*/lib/node_modules`), not `/usr/lib/node_modules`, and not a `client/`
   dependency.

Recipe: start the backend, then the static client server, then drive a browser at
`http://localhost:8080`:
```
setsid ./build/bin/world demos/sand.yaml >world.log 2>&1 </dev/null &   # see gotcha below
(cd client && setsid node index.js local >client.log 2>&1 </dev/null &)  # :8080, WS -> localhost:8081
node scripts/capture_gif.cjs assets/gifs/frictional_collapse.gif 60 25
```

**Gotcha:** launch long-running servers with `setsid ... </dev/null &` (detached from the shell's
process group). A plain `&` puts them in the Bash tool's process group, so they get killed when that
tool invocation's shell exits — the server appears to die "after one initialize" for no reason. The
incidental 404 in the browser console is just a missing `favicon.ico`, not a real error.

**Second gotcha, this one self-inflicted and worth remembering: `pkill -f <pattern>` in the Bash
tool kills the tool's own shell** whenever the pattern also matches something else in the same
compound command line (the shell's `/proc/*/cmdline` contains the whole command). It shows up as a
bare exit code 144 with no output and nothing started. Use `pkill -x world`, or put the kill in a
separate invocation from the launch.

**Third gotcha, from the same family: in `cmd && VAR=x && long_thing &`, the `&` backgrounds the
WHOLE and-list**, so the assignment happens in a subshell and the parent shell never sees `VAR`.
The symptom is a later command in the same invocation resolving `$VAR` to the empty string and
writing to `/` or reading a path that does not exist. Set variables in their own statement before
the backgrounded one.

### Recording real-time video for the showcase site

`scripts/capture_video.cjs` plus `demos/{sand,fluid}_video.yaml` produce
`assets/video/{frictional,frictionless}_collapse.mp4`: 1920x1080 (and a 2560x1440 variant) H.264 at
60 frames per second, 6.00 s long, playing at **1x real time**. Recorded 2026-08-31; the videos are
the animated form of `assets/gifs/{frictional,frictionless}_collapse.gif` and show the same scene.

**Real-time playback is a config property, not an encoder flag: `steps_per_frame * dt` must equal
`1 / fps` exactly.** The capture gates the client's loop, so one frame is one step batch and frame
spacing in simulated time is exactly that product; encoding at `fps` then plays back at
`steps_per_frame * dt * fps` times real time. `demos/sand.yaml` happens to satisfy this for the
GIFs (100 steps at 4e-4 = 0.04 s per frame, and 25 frames per second), which is why they are also
real time. 60 frames per second needs 1/60 s, and no integer number of 4e-4 steps gives it, so the
video configs move dt to **1/2520 s with `steps_per_frame: 42`**. dt going *down* rather than up was
deliberate: it keeps 22 steps per particle-particle contact period, above the 20 this codebase asks
for, so the change cannot destabilise anything. Verified: `measure_deposit.cjs` at t = 2.4 s gives
p50 radial extent 0.872 m / p50 height 0.026 m frictionless (recorded values 0.872 / 0.026) and
0.540 m / 0.043 m frictional (recorded 0.550 / 0.042), so the physics is unchanged.

**Framing has to be measured, and reasoning about it got it wrong twice.** The client's camera fits
the whole domain box to the canvas and expands the narrower axis to the canvas aspect, so the canvas
aspect is the only control. Two traps:
- **The isometric silhouette of the cubic domain is PORTRAIT, not landscape** — aspect 0.877. A
  world point projects to view-space height `0.8165*z - 0.4082*(x + y)`, spanning [-1.633, 1.633]
  over the unit cube, against a width of 2.83. Handing the client a 16:9 canvas therefore fits the
  box to the frame HEIGHT and leaves the grains occupying about a sixth of the frame, which is
  useless fullscreen. Render nearly square and crop the 16:9 frame out of that instead.
- **The vertical extent the scene needs is set by the frictionless wall splash, not by the standing
  column.** The obvious bound is the column's far top corner at +0.25; the frictionless material
  runs up the far walls and throws grains to **+0.55 at t = 1.0 s**, and the first two attempts
  clipped them. The check that settled it is worth reusing: threshold the retained full-resolution
  PNGs and find the topmost and bottommost row, **counting lit pixels per row rather than
  thresholding luminance**, because the box wireframe (0.45, 0.45, 0.52) is brighter than the shaded
  underside of a grain and a luminance test just finds the box. A grain is ~21 px across at a
  3840-wide render, so `> 30` lit pixels in a row means grains.

Final numbers: canvas aspect 1.16, 16:9 crop pushed to 93% of the spare height so the box's top
vertex leaves the frame (everything above the splash is empty air), leaving 42 px of headroom and
62 px of footroom at the render resolution, i.e. about two grain diameters. The frames are captured
at 2x the output resolution and downsampled by the encoder, because the impostor spheres have hard
`discard` edges with no multisampling and alias badly at native resolution.

**Cost:** ~1.8 frames/s for the frictional clip and ~1.4 for the frictionless one at a 3840x3310
render, so about 4 minutes per 360-frame clip. `CAPTURE_FRAME_DIR` keeps the full-resolution PNGs
(~1 GB per clip), which is what makes a re-crop or a second output resolution free; re-simulating to
change framing is the mistake to avoid.

## The demo configurations (`demos/`)

`demos/fluid.yaml` and `demos/sand.yaml` differ in exactly one value, `physics.friction`, and
`demos/grain_detail.yaml` is a 64-grain close-up for the renderer figure.
`demos/{sand,fluid}_video.yaml` are the same two scenes retimed for a 60 frames-per-second
real-time capture and differ from their originals only in `dt` and `steps_per_frame` (see
"Recording real-time video for the showcase site" above).

**Two grain counts appear in the README on purpose, and they are not in conflict.** The
performance scene is the checked-in `config.yaml`, a 32x32x32 cube, so **32,768** grains, and
every throughput number comes from it. The animation scene is the demo column, 25x25x52, so
**32,500** grains, and only the animation caption uses it. Grain count is a consequence of
geometry rather than a setting: spacing is one diameter, fixed by `physics.particle_radius`, so
`cube_length / (2*radius)` per axis fixes the total. The column count is not exactly 32,768
because no factorization of 32,768 gives the aspect ratio the collapse experiment needs. Do not
"reconcile" the two by editing one of them. All three are full
configs, not overlays: `SimConfig`'s member defaults are *not* the values in the repo-root
`config.yaml` (default `dt` is 1e-4, `init_jitter` 0, `cube_length_*` 0.2), so a partial demo config
would silently run something else.

**The scene is a granular column collapse, and that choice is deliberate.** The earlier
drop-a-cube-from-a-height scene produced a flat pancake in both the frictional and frictionless
cases, so the friction contrast was invisible. A column of aspect ratio ~4 released from rest is the
standard laboratory configuration (Lube et al. 2004, Lajeunesse et al. 2004), it collapses
dramatically on screen, and the friction difference shows up as the distance the front travels,
which is a
*measurable* quantity rather than an impression. Measured at t = 2.4 s with
`scripts/measure_deposit.cjs`, median radial extent 0.550 m at mu=0.5 against 0.872 m at mu=0, and
median height above the floor 0.042 m against 0.026 m.

**Do not try to produce a visually striking heap; the contact model cannot make one.** With no
rolling resistance the angle of repose is ~15 deg, so 32,500 grains of radius 0.01 m spread to a
natural heap radius of ~0.93 m and a height of ~0.25 m in a 2 m box, which reads as flat. Raising
`friction` to 2.0 was tried and barely moved it (median radial extent 0.72 m against 0.80 m):
free rolling, not sliding friction, is the limiter. The front-radius comparison is the honest way to show
the effect.

**Front radius against the laboratory correlation (measured 2026-08-27).** Lube et al. give
`R_inf/R_0 = 1 + 1.8 a^(1/2)` for axisymmetric columns with `a > 2`. `demos/sand.yaml` is 1.05 m
tall on a 0.505 m square footprint, so `a` is 3.69 (equal-area equivalent radius) or 4.16 (half-side
radius), predicting 1.27 m or 1.18 m. Re-run in a domain widened to [-2, 2] in x and y (so the front
is set by the material, not the walls) the measured front is 1.03 m, i.e. 13-19% short. The wide
domain changed the answer by under 1% against the [-1, 1] demo domain (max 1.026 m vs 1.092 m,
p99 0.933 vs 0.925), so the *frictional* demo is not wall-limited; the frictionless one is
(p99 1.31 m with walls at 1.0 m and corners at 1.41 m), and the README says so. Two plausible
reasons for the 13-19% deficit, neither chased down: only 25 grains across the column, far from the
continuum limit of the experiments, and no rolling resistance. Report it as a scaling check, never
as a calibration.
