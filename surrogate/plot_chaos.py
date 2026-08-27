"""Plot the predictability measurement written by build/bin/chaos.

Two curves, one figure, and the whole argument for predicting a coarse field
instead of a trajectory:

  * per-grain separation grows exponentially and saturates at the scale of the
    assembly -- individual grain positions become unknowable;
  * the coarse mass field separates by a bounded, small amount over the same
    interval.

The second panel also marks the measured surrogate errors. The point of doing so
is that a surrogate error is uninterpretable on its own: the realization-to-
realization scatter of the simulation itself is a floor no model conditioned only
on theta can go below, and it is the only meaningful reference for the numbers
reported by fit_surrogate.py.
"""

from __future__ import annotations

import argparse
import csv
import os

import numpy as np

# Measured by surrogate/fit_surrogate.py, K=20, mass channel, held-out.
TRUNCATION_ERROR = 0.243
SURROGATE_ERROR = 0.381
# Measured directly from chaos/final_fields.bin: the RMS relative deviation of a
# realization from the ensemble mean at fixed theta. This -- not the pairwise
# difference between two realizations -- is the floor for a model that predicts
# the conditional mean, and the two differ by a factor of sqrt(2) (measured 1.46).
IRREDUCIBLE_FLOOR = 0.168


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
    args = parser.parse_args()

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    styles = {args.small: ("tab:blue", r"$\epsilon = 10^{-3}\,r$  (near-identical start)"),
              args.large: ("tab:red", r"$\epsilon = 0.25\,r$  (distinct realizations)")}

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

    axes[1].axhline(SURROGATE_ERROR, color="black", ls="-", lw=1.2)
    axes[1].axhline(TRUNCATION_ERROR, color="black", ls=":", lw=1.2)
    axes[1].axhspan(0.0, IRREDUCIBLE_FLOOR, color="green", alpha=0.10, lw=0)
    axes[1].axhline(IRREDUCIBLE_FLOOR, color="green", ls="-", lw=1.6)
    axes[1].annotate(f"surrogate  {SURROGATE_ERROR:.3f}",
                     xy=(0.02, SURROGATE_ERROR), xytext=(0.60, SURROGATE_ERROR + 0.02),
                     fontsize=9)
    axes[1].annotate(f"POD truncation  {TRUNCATION_ERROR:.3f}",
                     xy=(0.02, TRUNCATION_ERROR), xytext=(0.60, TRUNCATION_ERROR + 0.02),
                     fontsize=9)
    axes[1].annotate(f"irreducible floor  {IRREDUCIBLE_FLOOR:.3f}\n"
                     "(no model can enter)",
                     xy=(0.02, IRREDUCIBLE_FLOOR), xytext=(0.60, IRREDUCIBLE_FLOOR - 0.085),
                     fontsize=9, color="darkgreen")
    axes[1].set(xlabel="time (s)", ylabel=r"relative $\ell^2$ difference of mass field",
                title="Coarse field: bounded difference", ylim=(0, 0.62))
    axes[1].grid(alpha=0.3)
    axes[1].legend(fontsize=9, loc="upper right")

    fig.suptitle("What is predictable about a thrown granular blob "
                 "(fixed $\\theta$, perturbed initial conditions)")
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    fig.savefig(args.out, dpi=110)
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
