"""Stage 2: a latent dynamics model, rolled out autoregressively.

Stage 1 mapped theta directly to the terminal field. Stage 2 instead learns to
STEP the latent forward,

    ( a(t), theta_material )  --GP-->  a(t + Delta),

and is then applied to its own output repeatedly to reach t + 2*Delta,
t + 3*Delta, ... Each GP step advances Delta = 0.05 s, i.e. 500 solver steps, so
the model is a temporally coarse ("jumpy") predictor rather than a step-level
surrogate.

Design points that are decisions, not defaults:

* ALL FOUR CHANNELS are carried. For the terminal state the momentum field was
  vacuous (the assembly is at rest), but a dynamics model needs it: mass alone is
  not a Markov state, since two assemblies with the same mass field and different
  velocity fields evolve differently.

* THE ACTION PARAMETERS ARE NOT GIVEN TO THE MODEL, only the material ones
  (mass, restitution, friction). The launch velocity has already done its work by
  the first recorded snapshot and is carried in the momentum channels; feeding it
  in as well would let the model shortcut the latent and would make "the latent is
  a sufficient state" untestable.

* THE SPLIT IS BY ROLLOUT, NOT BY TRANSITION. Consecutive transitions from one
  rollout are strongly correlated, so splitting transitions at random would leak
  a test rollout's trajectory into training and make the rollout error meaningless.

Two baselines are reported alongside, because an autoregressive model is only
worth its complexity if it beats them:
  persistence  -- a(t+Delta) = a(t): predict that nothing changes.
  direct       -- a separate Stage-1-style regression theta -> a(t) at each t.
                  This is the honest competitor: if predicting the state directly
                  from theta beats stepping there, the dynamics model earns nothing.
"""

from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dataset_io  # noqa: E402
from pod_study import pod_basis  # noqa: E402

MATERIAL_PARAMETERS = ["grain_mass", "restitution", "friction"]


def build_bases(grids: np.ndarray, train_index: np.ndarray, stride: int, ranks: list[int]):
    """POD basis per channel over all snapshots of the training rollouts.

    Takes the index rather than a pre-sliced array on purpose. `grids[train]`
    would materialize a copy of every channel at once -- 3.6 GB for the 1536
    rollout set -- whereas slicing one channel at a time inside the loop keeps
    peak extra memory to a single channel's snapshot matrix.
    """
    bases = []
    for channel, rank in enumerate(ranks):
        print(f"  building basis for channel {channel} "
              f"({len(train_index) * len(range(0, grids.shape[1], stride))} snapshots)...",
              flush=True)
        flat = grids[train_index, ::stride, channel]
        flat = flat.reshape(-1, np.prod(grids.shape[3:]))
        # Randomized SVD, not a full one. The dynamics ensemble has
        # rollouts * checkpoints snapshots (4320 at full scale), and a full SVD is
        # O(M^2 N) ~ 3e11 flops per channel there. Randomized SVD is O(M N rank),
        # which is the right algorithm whenever only leading modes are wanted. A
        # few extra components are computed so the retained-variance figure is not
        # 1.0 by construction.
        mean, modes, singular = pod_basis(flat, rank=rank + 24)
        bases.append({"mean": mean, "modes": modes[:, :rank], "rank": rank,
                      "retained": float(np.sum(singular[:rank] ** 2)
                                        / np.sum(singular ** 2))})
    return bases


def encode(grids: np.ndarray, bases) -> np.ndarray:
    """(rollouts, checkpoints, channels, ...) -> (rollouts, checkpoints, sum(ranks))."""
    pieces = []
    for channel, basis in enumerate(bases):
        flat = grids[:, :, channel].reshape(grids.shape[0], grids.shape[1], -1)
        pieces.append((flat - basis["mean"]) @ basis["modes"])
    return np.concatenate(pieces, axis=2)


def decode(latent: np.ndarray, bases, shape) -> np.ndarray:
    """Inverse of encode for the mass channel only (channel 0)."""
    rank = bases[0]["rank"]
    return latent[..., :rank] @ bases[0]["modes"].T + bases[0]["mean"]


def fit_step_gps(inputs: np.ndarray, targets: np.ndarray, seed: int, restarts: int):
    from sklearn.gaussian_process import GaussianProcessRegressor
    from sklearn.gaussian_process.kernels import Matern, WhiteKernel, ConstantKernel

    models = []
    for k in range(targets.shape[1]):
        # ISOTROPIC Matern for the step map, not ARD. Both practical and principled.
        #
        # Practical: ARD gives one length scale per input, and scikit-learn's
        # likelihood GRADIENT costs O(n^3) per hyperparameter -- so with ~33
        # latent+material inputs each optimizer step is ~30x the isotropic cost.
        # Measured: ARD at 23 inputs took 202 s for 20 modes on only 400 pairs,
        # and the cost is what forced the latent rank down.
        #
        # Principled: ARD earned its place in Stage 1 because the input was theta
        # and per-parameter relevance was itself a deliverable. Here the inputs are
        # latent coefficients that are already standardized and already ordered by
        # energy, so per-coordinate length scales are neither wanted as an output
        # nor identifiable from this much data.
        kernel = (ConstantKernel(1.0, (1e-3, 1e3))
                  * Matern(length_scale=1.0, length_scale_bounds=(1e-2, 1e4), nu=2.5)
                  + WhiteKernel(noise_level=1e-2, noise_level_bounds=(1e-8, 1e1)))
        model = GaussianProcessRegressor(kernel=kernel, normalize_y=True,
                                         n_restarts_optimizer=restarts,
                                         random_state=seed + k)
        model.fit(inputs, targets[:, k])
        models.append(model)
    return models


def predict_all(models, inputs: np.ndarray) -> np.ndarray:
    out = np.empty((len(inputs), len(models)))
    for k, model in enumerate(models):
        out[:, k] = model.predict(inputs)
    return out


def field_error(predicted_latent, true_fields, bases, shape) -> float:
    approx = decode(predicted_latent, bases, shape)
    truth = true_fields.reshape(len(true_fields), -1)
    return float(np.linalg.norm(approx - truth) / np.linalg.norm(truth))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", default="dataset_dynamics")
    parser.add_argument("--mass-rank", type=int, default=8)
    parser.add_argument("--momentum-rank", type=int, default=4)
    parser.add_argument("--train-pairs", type=int, default=1000,
                        help="subsample of transitions used to fit each GP (exact GP is "
                             "O(n^3) per likelihood evaluation)")
    parser.add_argument("--restarts", type=int, default=1)
    parser.add_argument("--test-rollouts", type=int, default=40)
    parser.add_argument("--max-rollouts", type=int, default=0,
                        help="use only the first N rollouts (0 = all)")
    parser.add_argument("--basis-stride", type=int, default=1,
                        help="use every Nth checkpoint when building the POD basis; the "
                             "basis only needs to span the states, not enumerate them")
    parser.add_argument("--skip-direct", action="store_true",
                        help="omit the direct theta -> a(t) baseline. It is only a meaningful "
                             "competitor when a single theta describes the whole trajectory, "
                             "which fails as soon as the state is observed rather than "
                             "parameterized, or actions arrive mid-trajectory.")
    parser.add_argument("--direct-every", type=int, default=1,
                        help="compute the expensive per-horizon direct baseline every Nth step")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--figure", default="surrogate/stage2_dynamics.png")
    parser.add_argument("--save", default="",
                        help="persist the fitted artifacts (POD bases, step GPs, input "
                             "normalization) so a rollout can be decoded later without "
                             "a 9-minute refit")
    parser.add_argument("--floor", type=float, default=0.154,
                        help="irreducible scatter for THIS configuration, from build/bin/chaos. "
                             "It is configuration-specific: narrowing the theta ranges or "
                             "changing the horizon changes it, so a stale value silently "
                             "misplaces the only reference line that matters.")
    args = parser.parse_args()

    data = dataset_io.load(args.directory)
    grids = data.usable()
    theta = data.parameters[data.ok]
    if args.max_rollouts:
        grids, theta = grids[:args.max_rollouts], theta[:args.max_rollouts]
    names = data.parameter_names
    material = np.array([names.index(n) for n in MATERIAL_PARAMETERS])
    num_rollouts, num_checkpoints = grids.shape[0], grids.shape[1]
    shape = grids.shape[3:]
    delta = float(data.checkpoint_times[1] - data.checkpoint_times[0])
    print(f"{num_rollouts} rollouts x {num_checkpoints} checkpoints, "
          f"Delta = {delta:.3f} s, latent grid {tuple(shape)}")

    # --- split by ROLLOUT ---
    rng = np.random.default_rng(args.seed)
    order = rng.permutation(num_rollouts)
    test_rollouts, train_rollouts = order[:args.test_rollouts], order[args.test_rollouts:]
    print(f"train {len(train_rollouts)} rollouts / test {len(test_rollouts)} rollouts")

    ranks = [args.mass_rank] + [args.momentum_rank] * 3
    bases = build_bases(grids, train_rollouts, args.basis_stride, ranks)
    latent_dim = sum(ranks)
    for channel, basis in enumerate(bases):
        print(f"  channel {channel}: rank {basis['rank']}, "
              f"variance retained {basis['retained']:.4f}")

    latent = encode(grids, bases)                      # (rollouts, checkpoints, latent_dim)
    theta_material = theta[:, material]

    # --- assemble one-step transitions ---
    def transitions(rollout_index):
        inputs, targets, tags = [], [], []
        for r in rollout_index:
            for k in range(num_checkpoints - 1):
                inputs.append(np.concatenate([latent[r, k], theta_material[r]]))
                targets.append(latent[r, k + 1])
                tags.append((r, k))
        return np.array(inputs), np.array(targets), tags

    train_inputs, train_targets, _ = transitions(train_rollouts)
    print(f"{len(train_inputs)} training transitions available, "
          f"input dim {train_inputs.shape[1]}, output dim {latent_dim}")

    # Standardize inputs: latent coefficients decay by orders of magnitude with
    # mode index, and mixing them with material parameters of order 0.1 would
    # leave the ARD optimisation badly conditioned.
    centre = train_inputs.mean(axis=0)
    spread = train_inputs.std(axis=0)
    spread[spread < 1e-12] = 1.0
    standardise = lambda x: (x - centre) / spread

    subset = rng.permutation(len(train_inputs))[:args.train_pairs]
    started = time.time()
    models = fit_step_gps(standardise(train_inputs[subset]), train_targets[subset],
                          args.seed, args.restarts)
    print(f"fitted {latent_dim} step GPs on {len(subset)} transitions "
          f"in {time.time() - started:.1f} s")

    if args.save:
        import joblib
        joblib.dump({"bases": bases, "models": models, "centre": centre, "spread": spread,
                     "ranks": ranks, "shape": shape, "delta": delta,
                     "checkpoint_times": data.checkpoint_times,
                     "material_parameters": MATERIAL_PARAMETERS,
                     "parameter_names": names,
                     "test_rollouts": test_rollouts, "train_rollouts": train_rollouts,
                     "directory": args.directory},
                    args.save, compress=3)
        print(f"saved fitted artifacts to {args.save}")

    # --- one-step accuracy on held-out rollouts ---
    test_inputs, test_targets, _ = transitions(test_rollouts)
    one_step = predict_all(models, standardise(test_inputs))
    print(f"\nONE-STEP latent error (held-out rollouts): "
          f"{np.linalg.norm(one_step - test_targets) / np.linalg.norm(test_targets):.4f}")
    persistence = test_inputs[:, :latent_dim]
    print(f"ONE-STEP persistence baseline:                "
          f"{np.linalg.norm(persistence - test_targets) / np.linalg.norm(test_targets):.4f}")

    # --- autoregressive rollout ---
    # Start from the TRUE encoded state at the first checkpoint, then feed the
    # model its own output. Error is reported per horizon in FIELD space (mass
    # channel), which is the quantity of interest.
    # The reconstruction floor is the error incurred by encoding the TRUTH and
    # decoding it: the cost of the latent itself, before any prediction. Measured
    # here because at low rank it dominates everything else, and reporting rollout
    # error without it invites the wrong conclusion about what to fix.
    print(f"\nAUTOREGRESSIVE ROLLOUT, field error (mass channel) vs horizon")
    print(f"  {'steps':>5} {'t (s)':>7} {'rollout':>9} {'one-step':>9} "
          f"{'recon':>9} {'persist':>9} {'direct':>9}")

    state = latent[test_rollouts, 0].copy()
    rollout_errors, teacher_errors, persist_errors, direct_errors, horizons = [], [], [], [], []
    recon_errors = []

    # "direct" competitor: theta -> a(t), refitted per horizon from training rollouts.
    from sklearn.gaussian_process import GaussianProcessRegressor
    from sklearn.gaussian_process.kernels import Matern, WhiteKernel, ConstantKernel
    theta_lo, theta_hi = theta[train_rollouts].min(axis=0), theta[train_rollouts].max(axis=0)
    theta_scale = np.where(theta_hi > theta_lo, theta_hi - theta_lo, 1.0)
    theta_norm = lambda t: (t - theta_lo) / theta_scale

    persistent_state = latent[test_rollouts, 0].copy()
    for k in range(1, num_checkpoints):
        state = predict_all(models, standardise(
            np.concatenate([state, theta_material[test_rollouts]], axis=1)))
        truth_fields = grids[test_rollouts, k, 0]
        rollout_errors.append(field_error(state, truth_fields, bases, shape))
        recon_errors.append(field_error(latent[test_rollouts, k], truth_fields, bases, shape))

        teacher_input = np.concatenate([latent[test_rollouts, k - 1],
                                        theta_material[test_rollouts]], axis=1)
        teacher = predict_all(models, standardise(teacher_input))
        teacher_errors.append(field_error(teacher, truth_fields, bases, shape))
        persist_errors.append(field_error(persistent_state, truth_fields, bases, shape))

        direct_models = []
        skip = args.skip_direct or (k - 1) % args.direct_every != 0
        for j in range(0 if skip else ranks[0]):
            kernel = (ConstantKernel(1.0, (1e-3, 1e3))
                      * Matern(length_scale=np.ones(theta.shape[1]),
                               length_scale_bounds=(1e-2, 1e6), nu=2.5)
                      + WhiteKernel(noise_level=1e-2, noise_level_bounds=(1e-8, 1e1)))
            model = GaussianProcessRegressor(kernel=kernel, normalize_y=True,
                                             n_restarts_optimizer=0, random_state=args.seed + j)
            model.fit(theta_norm(theta[train_rollouts]), latent[train_rollouts, k, j])
            direct_models.append(model)
        if direct_models:
            direct_latent = np.zeros((len(test_rollouts), latent_dim))
            direct_latent[:, :ranks[0]] = predict_all(direct_models,
                                                      theta_norm(theta[test_rollouts]))
            direct_errors.append(field_error(direct_latent, truth_fields, bases, shape))
        else:
            direct_errors.append(np.nan)

        horizons.append(float(data.checkpoint_times[k]))
        print(f"  {k:>5} {horizons[-1]:>7.2f} {rollout_errors[-1]:>9.4f} "
              f"{teacher_errors[-1]:>9.4f} {recon_errors[-1]:>9.4f} "
              f"{persist_errors[-1]:>9.4f} {direct_errors[-1]:>9.4f}")

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(figsize=(8.5, 5.5))
        ax.plot(horizons, rollout_errors, "o-", lw=2, label="autoregressive rollout")
        ax.plot(horizons, teacher_errors, "s--", lw=1.5,
                label="one step from truth (no accumulation)")
        ax.plot(horizons, recon_errors, "-", color="purple", lw=2,
                label="reconstruction floor (cost of the latent itself)")
        ax.plot(horizons, persist_errors, "^:", lw=1.5, label="persistence baseline")
        direct = np.array(direct_errors)
        finite = np.isfinite(direct)
        ax.plot(np.array(horizons)[finite], direct[finite], "d-.", lw=1.5,
                label=r"direct $\theta \to a(t)$")
        ax.axhline(args.floor, color="green", lw=1.6)
        ax.annotate(f"irreducible floor {args.floor:.3f}", xy=(horizons[0], args.floor),
                    xytext=(horizons[0], args.floor * 1.08), color="darkgreen", fontsize=9)
        ax.axvline(0.62, color="grey", ls="--", lw=1.2)
        ax.annotate("per-grain predictability\nhorizon, 0.62 s", xy=(0.62, 0.9),
                    xytext=(0.64, 0.86), fontsize=9, color="dimgrey")
        ax.set(xlabel="time (s)", ylabel="relative $\\ell^2$ error, mass field",
               title="Stage 2: latent dynamics rolled out autoregressively")
        ax.grid(alpha=0.3); ax.legend(fontsize=9)
        fig.tight_layout()
        os.makedirs(os.path.dirname(args.figure) or ".", exist_ok=True)
        fig.savefig(args.figure, dpi=110)
        print(f"\nwrote {args.figure}")
    except ImportError:
        print("\n(matplotlib unavailable; skipping figure)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
