"""Compare the two particle samplers, and show why the choice matters.

Run first:
    decode_particles.py --sampler uniform --out decoded_uniform
    decode_particles.py --sampler poisson --out decoded
    build/bin/profile decoded/restart.yaml        > energy_poisson.csv
    build/bin/profile decoded_uniform/restart.yaml > energy_uniform.csv

The point of the layout: at domain scale the two samplers are indistinguishable,
so a top view alone cannot justify the extra machinery. The difference is entirely
sub-node, and it shows up in the nearest-neighbour distribution and in what the
solver does when handed the state.
"""

from __future__ import annotations

import argparse
import os

import numpy as np


def load_snapshot(directory, index):
    data = np.load(os.path.join(directory, f"snapshot_{index:03d}.npz"))
    return data["positions"], data["velocities"], float(data["time"])


def nearest_neighbour(positions, sample=4000, seed=0):
    rng = np.random.default_rng(seed)
    idx = rng.choice(len(positions), min(sample, len(positions)), replace=False)
    from scipy.spatial import cKDTree
    distance, _ = cKDTree(positions).query(positions[idx], k=2)
    return distance[:, 1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--poisson", default="decoded")
    parser.add_argument("--uniform", default="decoded_uniform")
    parser.add_argument("--snapshot", type=int, default=9)
    parser.add_argument("--radius", type=float, default=0.01)
    parser.add_argument("--energy-poisson", default="/tmp/energy_poisson.csv")
    parser.add_argument("--energy-uniform", default="/tmp/energy_uniform.csv")
    parser.add_argument("--out", default="surrogate/decoded_particles.png")
    parser.add_argument("--dpi", type=int, default=200)
    args = parser.parse_args()

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    diameter = 2 * args.radius
    pos_p, _, t = load_snapshot(args.poisson, args.snapshot)
    pos_u, _, _ = load_snapshot(args.uniform, args.snapshot)

    fig, axes = plt.subplots(2, 3, figsize=(16, 9.5))

    # --- row 0: domain-scale top views, where the two are indistinguishable ---
    for col, (pos, label) in enumerate(((pos_u, "uniform sampler"),
                                        (pos_p, "Poisson-disk sampler"))):
        axes[0, col].scatter(pos[:, 0], pos[:, 1], s=3.5, alpha=0.5, lw=0,
                             c="tab:orange" if col == 0 else "tab:blue")
        axes[0, col].set(title=f"{label}: full domain, t={t:.2f}s",
                         xlabel="x (m)", ylabel="y (m)", aspect="equal",
                         xlim=(-1, 1), ylim=(-1, 1))
        axes[0, col].grid(alpha=0.2)

    # --- nearest-neighbour distribution: the quantitative discriminator ---
    nn_u = nearest_neighbour(pos_u) / diameter
    nn_p = nearest_neighbour(pos_p) / diameter
    bins = np.linspace(0, 3, 70)
    axes[0, 2].hist(nn_u, bins=bins, alpha=0.6, label="uniform", color="tab:orange", density=True)
    axes[0, 2].hist(nn_p, bins=bins, alpha=0.6, label="Poisson-disk", color="tab:blue", density=True)
    axes[0, 2].axvline(1.0, color="black", ls="--", lw=1.6)
    axes[0, 2].annotate("one diameter:\ncontact", xy=(1.0, axes[0, 2].get_ylim()[1] * 0.72),
                        xytext=(1.12, axes[0, 2].get_ylim()[1] * 0.72), fontsize=9)
    axes[0, 2].set(xlabel="nearest-neighbour distance / diameter", ylabel="density",
                   title="Overlap: uniform %.0f%% vs Poisson %.0f%%"
                         % (100 * (nn_u < 1).mean(), 100 * (nn_p < 1).mean()))
    axes[0, 2].legend(fontsize=9); axes[0, 2].grid(alpha=0.2)

    # --- row 1: zoom to grain scale, where the difference is the whole point ---
    centre = pos_p.mean(axis=0)
    half = 12 * diameter
    for col, (pos, label) in enumerate(((pos_u, "uniform sampler"),
                                        (pos_p, "Poisson-disk sampler"))):
        near = (np.abs(pos[:, 0] - centre[0]) < half) & (np.abs(pos[:, 1] - centre[1]) < half) \
               & (np.abs(pos[:, 2] - centre[2]) < diameter)
        selected = pos[near]
        axes[1, col].scatter(selected[:, 0], selected[:, 1],
                             s=(args.radius / half * 620) ** 2, alpha=0.55,
                             c="tab:orange" if col == 0 else "tab:blue",
                             edgecolors="k", linewidths=0.4)
        axes[1, col].set(title=f"{label}: grain scale, one-diameter slab "
                               f"({len(selected)} grains)",
                         xlabel="x (m)", ylabel="y (m)", aspect="equal",
                         xlim=(centre[0] - half, centre[0] + half),
                         ylim=(centre[1] - half, centre[1] + half))

    # --- the consequence: what the solver does with each state ---
    for path, label, colour in ((args.energy_uniform, "uniform", "tab:orange"),
                                (args.energy_poisson, "Poisson-disk", "tab:blue")):
        if not os.path.exists(path):
            continue
        table = np.loadtxt(path, delimiter=",")
        axes[1, 2].semilogy(table[:, 1], table[:, 2], "o-", color=colour, lw=2, label=label)
    axes[1, 2].set(xlabel="time after restart (s)", ylabel="total energy",
                   title="Solver restarted from the decoded state")
    axes[1, 2].grid(alpha=0.3, which="both"); axes[1, 2].legend(fontsize=9)
    axes[1, 2].annotate("spurious elastic energy in\noverlapping contacts, released\n"
                        "as a violent transient",
                        xy=(0.0, 14893), xytext=(0.055, 6000), fontsize=9,
                        arrowprops=dict(arrowstyle="->", lw=1.1))

    fig.suptitle("Decoding a latent rollout to particles: the sampler determines whether the "
                 "state is re-initializable", fontsize=13)
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    fig.savefig(args.out, dpi=args.dpi)
    print(f"wrote {args.out}")
    print(f"  uniform  overlap {100 * (nn_u < 1).mean():.1f}%, median gap {np.median(nn_u):.3f} d")
    print(f"  poisson  overlap {100 * (nn_p < 1).mean():.1f}%, median gap {np.median(nn_p):.3f} d")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
