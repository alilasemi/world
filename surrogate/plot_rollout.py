"""Figures for the autoregressive latent rollout, from the artifacts fit_dynamics.py saves.

Two figures, no refit required (a refit costs about ten minutes; rolling the fitted step
maps forward costs seconds):

  * curves  -- field error against horizon, against the cost of the reduced representation
               alone, one step from truth, a persistence baseline, and the irreducible
               scatter of the simulation itself.
  * fields  -- the same rollout decoded back to the mass field, median and worst held-out
               rollout beside the simulation, with the modal coefficients that produced them.
               An error number says how wrong the model is; this says how it is wrong.

  surrogate/.venv/bin/python surrogate/plot_rollout.py --model surrogate/stage2_model.joblib
"""

from __future__ import annotations

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dataset_io  # noqa: E402
from fit_dynamics import decode, encode, field_error, predict_all  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default="surrogate/stage2_model.joblib")
    parser.add_argument("--time", type=float, default=1.0,
                       help="horizon for the field figure; snapped to the nearest checkpoint")
    parser.add_argument("--modes", type=int, default=20,
                        help="leading mass-channel coefficients to plot")
    parser.add_argument("--floor", type=float, default=0.154,
                        help="irreducible scatter for THIS configuration, from build/bin/chaos. "
                             "Configuration-specific: a stale value misplaces the only "
                             "reference line that matters.")
    parser.add_argument("--curves-out", default="surrogate/rollout_error.png")
    parser.add_argument("--fields-out", default="surrogate/rollout_fields.png")
    parser.add_argument("--dpi", type=int, default=200)
    args = parser.parse_args()

    import joblib
    fitted = joblib.load(args.model)
    bases, models = fitted["bases"], fitted["models"]
    centre, spread, shape = fitted["centre"], fitted["spread"], fitted["shape"]
    times = np.asarray(fitted["checkpoint_times"])
    test = fitted["test_rollouts"]

    data = dataset_io.load(fitted["directory"])
    grids = data.grids[data.ok]
    theta = data.parameters[data.ok]
    material = np.array([data.parameter_names.index(n)
                         for n in fitted["material_parameters"]])
    theta_material = theta[test][:, material]

    latent = encode(grids, bases)
    standardise = lambda x: (x - centre) / spread

    # Roll forward from the true state at the first checkpoint, feeding the model its own
    # output, and record the three separately measurable error terms at every horizon.
    state = latent[test, 0].copy()
    persistent = latent[test, 0].copy()
    horizons, rollout, teacher, recon, persist = [], [], [], [], []
    states = {}
    for k in range(1, len(times)):
        state = predict_all(models, standardise(
            np.concatenate([state, theta_material], axis=1)))
        truth_fields = grids[test, k, 0]
        rollout.append(field_error(state, truth_fields, bases, shape))
        recon.append(field_error(latent[test, k], truth_fields, bases, shape))
        teacher.append(field_error(predict_all(models, standardise(
            np.concatenate([latent[test, k - 1], theta_material], axis=1))),
            truth_fields, bases, shape))
        persist.append(field_error(persistent, truth_fields, bases, shape))
        horizons.append(float(times[k]))
        states[k] = state.copy()
        print(f"  t = {horizons[-1]:.2f} s   rollout {rollout[-1]:.4f}   "
              f"one step {teacher[-1]:.4f}   reduction {recon[-1]:.4f}   "
              f"persistence {persist[-1]:.4f}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # ----- curves -----
    fig, ax = plt.subplots(figsize=(9, 5.5))
    # Log scale: the persistence baseline is an order of magnitude above the model, and on a
    # linear axis it flattens the three curves that are actually being compared.
    ax.semilogy(horizons, persist, "^:", color="tab:green", lw=1.5,
                label="persistence baseline")
    ax.semilogy(horizons, rollout, "o-", lw=2.2, label="autoregressive rollout")
    ax.semilogy(horizons, teacher, "s--", lw=1.5, label="one step from truth")
    ax.semilogy(horizons, recon, "-", color="purple", lw=2,
                label="cost of the reduced representation alone")
    ax.axhline(args.floor, color="green", lw=1.6)
    ax.annotate(f"irreducible scatter of the simulation, {args.floor:.3f}",
                xy=(horizons[-1], args.floor), xytext=(horizons[-1], args.floor * 0.86),
                color="darkgreen", fontsize=9, ha="right")
    ax.set(xlabel="time (s)", ylabel=r"relative $\ell^2$ error, mass field",
           title="Latent dynamics rolled out autoregressively",
           ylim=(0.08, 2.6))
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=9, loc="upper right")
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.curves_out) or ".", exist_ok=True)
    fig.savefig(args.curves_out, dpi=args.dpi)
    print(f"wrote {args.curves_out}")

    # ----- fields -----
    target = int(np.argmin(np.abs(times - args.time)))
    predicted = decode(states[target], bases, shape)
    truth = grids[test, target, 0].reshape(len(test), -1)
    error = np.linalg.norm(predicted - truth, axis=1) / np.linalg.norm(truth, axis=1)
    order = np.argsort(error)
    median, worst = int(order[len(order) // 2]), int(order[-1])
    print(f"t = {times[target]:.2f} s, {len(test)} held-out rollouts, "
          f"mean {error.mean():.4f}, median {error[median]:.4f}, worst {error[worst]:.4f}")

    rank = bases[0]["rank"]
    shown = min(args.modes, rank)
    true_coefficients = latent[test, target, :rank]

    fig, axes = plt.subplots(2, 4, figsize=(17, 7.5))
    for row, (case, label) in enumerate([(median, "median"), (worst, "worst")]):
        true_field = truth[case].reshape(shape).sum(axis=2)          # column density
        pred_field = predicted[case].reshape(shape).sum(axis=2)
        vmax = max(true_field.max(), pred_field.max())
        panels = [(true_field, "simulation"), (pred_field, "rollout prediction"),
                  (pred_field - true_field, "difference")]
        for col, (field, title) in enumerate(panels):
            kw = dict(cmap="magma", vmin=0, vmax=vmax) if col < 2 else \
                 dict(cmap="coolwarm", vmin=-vmax * 0.5, vmax=vmax * 0.5)
            image = axes[row, col].imshow(field.T, origin="lower", **kw)
            axes[row, col].set_title(f"{label}: {title}")
            axes[row, col].set_xticks([]); axes[row, col].set_yticks([])
            plt.colorbar(image, ax=axes[row, col], fraction=0.046)
        axes[row, 3].plot(true_coefficients[case, :shown], "o-", label="simulation")
        axes[row, 3].plot(states[target][case, :shown], "s--", label="rollout")
        axes[row, 3].set(xlabel="mode index", ylabel="coefficient",
                         title=f"{label}: leading {shown} of {rank} mass modes")
        axes[row, 3].grid(alpha=0.3); axes[row, 3].legend(fontsize=8)

    fig.suptitle(f"Autoregressive rollout at t = {times[target]:.2f} s, "
                 f"{target} steps of {fitted['delta']:.2f} s from the observed initial "
                 f"field: mass field column density")
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.fields_out) or ".", exist_ok=True)
    fig.savefig(args.fields_out, dpi=args.dpi)
    print(f"wrote {args.fields_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
