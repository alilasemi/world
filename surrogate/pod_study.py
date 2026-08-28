"""POD (Proper Orthogonal Decomposition) study of the latent fields.

Answers the two questions that decide whether the linear-subspace plan works,
before any Gaussian process is fitted:

  1. HOW MANY MODES does each channel need? If the singular values decay fast,
     a handful of GP regressions reproduces the field and the plan stands. If
     they decay slowly, the fields do not live near any low-dimensional linear
     subspace and the escalation is a convolutional autoencoder.

  2. HOW MANY SNAPSHOTS are needed to *learn* the basis? Measured by fitting the
     basis on a subset and reconstructing held-out snapshots. This is the
     sample-complexity number that sizes the full dataset run, and it is not the
     same question as (1): a basis can be cheap to represent and still expensive
     to estimate.

ONE BASIS PER CHANNEL, never a joint basis over all four. A joint basis would
require scaling mass against momentum, which is nonphysical: mass density is
positive-definite while momentum density is signed and has a much wider,
scenario-dependent range. Whichever channel got the larger scale would dominate
the spectrum and "energy captured" would stop meaning anything. Separate bases
also let each channel choose its own rank, which smooth-positive mass and
signed-rougher momentum will not agree on.

Reconstruction error is reported on the RAW (uncentered) field -- reconstruct
mean + sum(a_k phi_k) and compare against the actual field -- because that is
the quantity the surrogate must ultimately produce. Quoting error on the
centered field flatters the method by hiding the mean.
"""

from __future__ import annotations

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dataset_io  # noqa: E402

CHANNEL_NAMES = ["mass", "momentum_x", "momentum_y", "momentum_z"]


def pod_basis(snapshots: np.ndarray, rank=None):
    """snapshots: (M, N) -> (mean (N,), modes (N, r), singular values (r,)).

    A full economy SVD costs O(M^2 N). That is fine for the terminal-state
    ensemble (M = 150) but not for the dynamics ensemble, where every checkpoint
    of every rollout is a snapshot: M = 4320 gives 4320^2 * 16384 ~ 3e11 flops
    per channel. Passing `rank` switches to a randomized SVD, O(M N rank), which
    is the right algorithm whenever only the leading modes are wanted -- and it
    is the leading modes that are always wanted.
    """
    mean = snapshots.mean(axis=0)
    centered = snapshots - mean
    if rank is None:
        # Economy SVD: centered = U diag(s) Vt, so the modes are the rows of Vt.
        _, singular_values, vt = np.linalg.svd(centered, full_matrices=False)
    else:
        from sklearn.utils.extmath import randomized_svd
        _, singular_values, vt = randomized_svd(
            centered, n_components=int(min(rank, min(centered.shape) - 1)),
            n_oversamples=20, random_state=0)
    return mean, vt.T, singular_values


def reconstruction_error(snapshots: np.ndarray, mean: np.ndarray, modes: np.ndarray,
                         num_modes: int) -> float:
    """Relative Frobenius error of the rank-`num_modes` reconstruction."""
    centered = snapshots - mean
    basis = modes[:, :num_modes]
    coefficients = centered @ basis            # project
    approx = coefficients @ basis.T + mean     # reconstruct (mean included)
    return float(np.linalg.norm(approx - snapshots) / np.linalg.norm(snapshots))


def modes_for(singular_values: np.ndarray, fraction: float) -> int:
    energy = np.cumsum(singular_values ** 2) / np.sum(singular_values ** 2)
    return int(np.searchsorted(energy, fraction) + 1)


def study_channel(name: str, snapshots: np.ndarray, seed: int) -> dict:
    num_snapshots, num_nodes = snapshots.shape
    mean, modes, singular_values = pod_basis(snapshots)

    result = {"name": name, "singular_values": singular_values}
    print(f"\n=== {name}  ({num_snapshots} snapshots x {num_nodes} nodes) ===")
    for fraction in (0.90, 0.95, 0.99):
        result[f"modes_{int(fraction*100)}"] = modes_for(singular_values, fraction)
        print(f"  modes for {fraction:.0%} of variance: "
              f"{result[f'modes_{int(fraction*100)}']}")

    print(f"  {'k':>4} {'rel. L2 error':>14}")
    errors = []
    mode_counts = [1, 2, 5, 10, 20, 40, 80]
    for k in mode_counts:
        if k > min(num_snapshots - 1, num_nodes):
            continue
        error = reconstruction_error(snapshots, mean, modes, k)
        errors.append((k, error))
        print(f"  {k:>4} {error:>14.4f}")
    result["errors"] = errors

    # --- sample complexity: fit the basis on a subset, test on held-out data ---
    rng = np.random.default_rng(seed)
    order = rng.permutation(num_snapshots)
    num_test = max(20, num_snapshots // 5)
    test = snapshots[order[:num_test]]
    pool = snapshots[order[num_test:]]
    print(f"  held-out reconstruction error at k=10 (test set of {num_test}):")
    print(f"  {'train':>6} {'rel. L2 error':>14}")
    curve = []
    for num_train in (10, 20, 40, 80, len(pool)):
        if num_train > len(pool):
            continue
        train = pool[:num_train]
        train_mean, train_modes, _ = pod_basis(train)
        k = min(10, num_train - 1)
        error = reconstruction_error(test, train_mean, train_modes, k)
        curve.append((num_train, error))
        print(f"  {num_train:>6} {error:>14.4f}")
    result["sample_curve"] = curve
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", default="dataset")
    parser.add_argument("--snapshots", choices=["final", "all"], default="final",
                        help="'final' = the Stage-1 target (one field per rollout); "
                             "'all' = every checkpoint, the ensemble a dynamics model sees")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--plot", default="surrogate/pod_spectrum.png")
    parser.add_argument("--dpi", type=int, default=200)
    parser.add_argument("--per-checkpoint", action="store_true",
                        help="also report held-out error per checkpoint, which is what "
                             "reveals that momentum is only learnable while the material moves")
    args = parser.parse_args()

    data = dataset_io.load(args.directory)
    grids = data.usable()
    print(f"loaded {grids.shape[0]} usable rollouts, latent {tuple(grids.shape[2:])}, "
          f"{data.grids.shape[1]} checkpoints")

    # Momentum decays to nothing as the pile settles, so a single number for
    # "how well can momentum be predicted" is meaningless -- it depends entirely
    # on when you ask. Report the scale of each channel over time first.
    ratios = []
    print("\nCHANNEL SCALE vs TIME (why momentum cannot be judged by the final state alone)")
    print(f"  {'t (s)':>7} {'|rho*u| RMS':>12} {'|rho| RMS':>10} {'ratio':>7}")
    for k in range(grids.shape[1]):
        momentum_rms = float(np.sqrt((np.sqrt((grids[:, k, 1:] ** 2).sum(axis=1)) ** 2).mean()))
        mass_rms = float(np.sqrt((grids[:, k, 0] ** 2).mean()))
        ratios.append(momentum_rms / mass_rms)
        print(f"  {data.checkpoint_times[k]:>7.2f} {momentum_rms:>12.5f} "
              f"{mass_rms:>10.5f} {ratios[-1]:>7.3f}")

    if args.per_checkpoint:
        rng = np.random.default_rng(args.seed)
        print("\nHELD-OUT ERROR PER CHECKPOINT (k=20, 30 test snapshots)")
        print(f"  {'t (s)':>7} " + " ".join(f"{n:>12}" for n in CHANNEL_NAMES))
        for k in range(grids.shape[1]):
            cells = []
            for channel in range(len(CHANNEL_NAMES)):
                X = grids[:, k, channel].reshape(len(grids), -1)
                order = rng.permutation(len(X))
                test, pool = X[order[:30]], X[order[30:]]
                mean, modes, _ = pod_basis(pool)
                cells.append(f"{reconstruction_error(test, mean, modes, 20):>12.3f}")
            print(f"  {data.checkpoint_times[k]:>7.2f} " + " ".join(cells))

    results = []
    for channel, name in enumerate(CHANNEL_NAMES):
        if args.snapshots == "final":
            field = grids[:, -1, channel]
        else:
            field = grids[:, :, channel].reshape(-1, *grids.shape[3:])
        results.append(study_channel(name, field.reshape(len(field), -1), args.seed))

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, axes = plt.subplots(1, 3, figsize=(16, 4.5))
        for result in results:
            s = result["singular_values"]
            energy = np.cumsum(s ** 2) / np.sum(s ** 2)
            # Drop the final value: the snapshot matrix is mean-centered, so its rank is at
            # most one less than the number of snapshots and the last singular value is
            # numerically zero. Plotting it puts a spurious cliff at the right edge.
            axes[0].semilogy((s / s[0])[:-1], label=result["name"])
            axes[1].plot(np.arange(1, len(energy) + 1), energy, label=result["name"])
        axes[0].set(xlabel="mode index", ylabel="singular value / first",
                    title="POD spectrum (per channel)")
        axes[0].grid(alpha=0.3); axes[0].legend()
        axes[1].axhline(0.99, color="grey", ls="--", lw=1)
        axes[1].axhline(0.90, color="grey", ls=":", lw=1)
        axes[1].set(xlabel="modes retained", ylabel="cumulative variance",
                    title="Energy captured", xlim=(1, 60), ylim=(0, 1.02))
        axes[1].grid(alpha=0.3); axes[1].legend()
        axes[2].semilogy(data.checkpoint_times, ratios, "o-", color="crimson")
        axes[2].set(xlabel="time (s)", ylabel="RMS |rho*u| / RMS |rho|",
                    title="Momentum scale collapses as the pile settles")
        axes[2].grid(alpha=0.3)
        # Annotate the measured ratios rather than an interpretation of them. An earlier
        # version said "numerical creep" at the last checkpoint, which was true when the
        # ratio fell to 0.018 and false at 0.15: whether the residual motion is creep or
        # real slow spreading depends on the configuration, so let the number say it.
        axes[2].annotate(f"momentum is {ratios[0]:.1f}x the mass\nscale here: the dominant\n"
                         "part of the state",
                         xy=(data.checkpoint_times[0], ratios[0]),
                         xytext=(0.05, 0.42), textcoords="axes fraction",
                         arrowprops=dict(arrowstyle="->", lw=1), fontsize=9)
        axes[2].annotate(f"...and {ratios[-1]:.3f}x here",
                         xy=(data.checkpoint_times[-1], ratios[-1]),
                         xytext=(0.30, 0.12), textcoords="axes fraction",
                         arrowprops=dict(arrowstyle="->", lw=1), fontsize=9)
        fig.suptitle(f"POD study -- {args.snapshots} snapshots, one basis per channel")
        fig.tight_layout()
        os.makedirs(os.path.dirname(args.plot) or ".", exist_ok=True)
        fig.savefig(args.plot, dpi=args.dpi)
        print(f"\nwrote {args.plot}")
    except ImportError:
        print("\n(matplotlib unavailable; skipping plot)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
