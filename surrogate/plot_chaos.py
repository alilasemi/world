"""Plot the predictability measurement written by build/bin/chaos.

Two curves, one figure, and the whole argument for predicting a coarse field
instead of a trajectory:

  * per-grain separation grows exponentially and saturates at the scale of the
    assembly -- individual grain positions become unknowable;
  * the coarse mass field separates by a bounded, small amount over the same
    interval.

This figure measures the simulation alone. No model error is drawn on it: the
comparison of a prediction against the realization-to-realization scatter belongs
with the model, in fit_dynamics.py and plot_rollout.py.
"""

from __future__ import annotations

import argparse
import csv
import os

import numpy as np


def load(path: str):
    rows = list(csv.DictReader(open(path)))
    time = np.array([float(r["time"]) for r in rows])
    member = np.array([int(r["member"]) for r in rows])
    times = np.unique(time)
    def pivot(column):
        values = np.array([float(r[column]) for r in rows])
        return np.array([[values[(member == mm) & (time == tt)][0] for tt in times]
                         for mm in np.unique(member)])
    return times, pivot("grain_rms_displacement"), pivot("field_relative_difference")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--small", default="chaos/divergence_eps1e-3.csv")
    parser.add_argument("--large", default="chaos/divergence_eps0.25.csv")
    parser.add_argument("--out", default="surrogate/predictability.png")
    parser.add_argument("--dpi", type=int, default=200)
    args = parser.parse_args()

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    styles = {args.small: ("tab:blue", r"$\epsilon = 10^{-3}\,r$  (near-identical start)"),
              args.large: ("tab:red", r"$\epsilon = 0.25\,r$  (distinct realizations)")}

    field_peak = 0.0
    for path, (color, label) in styles.items():
        if not os.path.exists(path):
            continue
        times, grain, field = load(path)

        axes[0].semilogy(times, grain.mean(axis=0), color=color, lw=2, label=label)
        axes[0].fill_between(times, grain.min(axis=0), grain.max(axis=0),
                             color=color, alpha=0.18, lw=0)
        axes[1].plot(times, field.mean(axis=0), color=color, lw=2, label=label)
        axes[1].fill_between(times, field.min(axis=0), field.max(axis=0),
                             color=color, alpha=0.18, lw=0)
        field_peak = max(field_peak, float(field.max()))

        if path == args.small:
            mean_grain = grain.mean(axis=0)
            lo, hi = np.searchsorted(times, 0.04), np.searchsorted(times, 0.5)
            slope, intercept = np.polyfit(times[lo:hi], np.log(mean_grain[lo:hi]), 1)
            fit_times = times[lo:hi]
            axes[0].plot(fit_times, np.exp(intercept + slope * fit_times), "k--", lw=1.4,
                         label=rf"fit: $\lambda = {slope:.2f}\,\mathrm{{s}}^{{-1}}$"
                               rf"  ($1/\lambda = {1/slope:.2f}$ s)")

    axes[0].set(xlabel="time (s)", ylabel="RMS per-grain separation (m)",
                title="Individual grains: exponential divergence, then saturation")
    axes[0].grid(alpha=0.3, which="both")
    axes[0].legend(fontsize=9, loc="lower right")

    axes[1].set(xlabel="time (s)", ylabel=r"relative $\ell^2$ difference of mass field",
                title="Coarse field: bounded difference",
                ylim=(0, 1.15 * field_peak))
    axes[1].grid(alpha=0.3)
    axes[1].legend(fontsize=9, loc="upper right")

    fig.suptitle("What is predictable about a thrown granular blob "
                 "(fixed $\\theta$, perturbed initial conditions)")
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    fig.savefig(args.out, dpi=args.dpi)
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
