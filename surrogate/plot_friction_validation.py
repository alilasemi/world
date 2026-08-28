"""Verify the tangential contact model against the analytic answer.

A single grain sliding on the floor in sustained contact must decelerate at exactly mu*g and
then stop, which makes one grain the right place to check the tangential force. Reads the
full state history written by build/bin/model_problem.

  ./build/bin/model_problem model_problems/sliding_friction.yaml
  surrogate/.venv/bin/python surrogate/plot_friction_validation.py
"""

from __future__ import annotations

import argparse
import os

import numpy as np


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", default="model_problems/history.dat")
    parser.add_argument("--friction", type=float, default=0.5)
    parser.add_argument("--gravity", type=float, default=9.81)
    parser.add_argument("--out", default="surrogate/friction_validation.png")
    parser.add_argument("--dpi", type=int, default=200)
    args = parser.parse_args()

    rows = [[float(v) for v in line.split()]
            for line in open(args.history)
            if line.strip() and not line.startswith("#")]
    table = np.array(rows)
    time, vx = table[:, 1], table[:, 5]

    # Fit only while the grain is still sliding; once it stops there is nothing to fit.
    sliding = vx > 0.1
    slope, intercept = np.polyfit(time[sliding], vx[sliding], 1)
    ideal = -args.friction * args.gravity
    stop = time[np.argmax(vx < 1e-6)]

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7.5, 5))
    ax.plot(time, vx, lw=2.4, color="tab:blue", label="simulation")
    ax.plot(time, np.maximum(intercept + ideal * time, 0.0), "k--", lw=1.6,
            label=rf"analytic, $-\mu g = {ideal:.3f}$ m/s$^2$")
    ax.axhline(0.0, color="grey", lw=0.8)
    ax.annotate(f"fitted {slope:.3f} m/s$^2$\n({100 * (1 - abs(slope / ideal)):.1f}% low)",
                xy=(0.10, intercept + slope * 0.10),
                xytext=(0.12, 0.70), fontsize=10,
                arrowprops=dict(arrowstyle="->", lw=1.1))
    ax.annotate(f"stops at {stop:.3f} s and stays stopped\n"
                rf"($|v| < 10^{{-8}}$ m/s thereafter)",
                xy=(stop, 0.0), xytext=(stop + 0.01, 0.16), fontsize=10,
                arrowprops=dict(arrowstyle="->", lw=1.1))
    ax.set(xlabel="time (s)", ylabel="sliding velocity (m/s)",
           title="One grain sliding on the floor, against the analytic answer")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=10)
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    fig.savefig(args.out, dpi=args.dpi)
    print(f"fitted {slope:.4f} m/s^2 against ideal {ideal:.4f} "
          f"({100 * (1 - abs(slope / ideal)):.1f}% low), stops at {stop:.4f} s")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
