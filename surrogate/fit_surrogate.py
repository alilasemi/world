"""Stage 1 of the outcome surrogate: theta -> terminal mass field.

Composition (see surrogate/formulation.tex for the formal statement):

    theta in R^7  --GP--> a in R^K  --affine--> s_1 in R^N,  N = 16384

One independent scalar Gaussian process per retained POD mode. The mass channel
alone is predicted: the terminal momentum field is residual creep (RMS momentum
falls to 1.8% of its initial value by t=2 s) and the launch velocity already
enters theta directly, so nothing is lost.

The error of the composed surrogate separates into two independent parts, and
this script measures them SEPARATELY rather than reporting only the total:

  * truncation  -- the POD basis cannot represent the field exactly. Bounded by
                   the discarded singular values; independent of the regressor.
                   Measured by projecting the true field onto the basis.
  * regression  -- the GP does not recover the projected coefficients exactly.

Reporting only the total conflates "the basis is too small" with "the map is
hard to learn", which have different remedies.

The basis, the mean field, and the input/output normalisations are all fitted on
the TRAINING split only. Fitting any of them on the full ensemble leaks test
information and understates the error of the deployed surrogate.
"""

from __future__ import annotations

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dataset_io  # noqa: E402
from pod_study import pod_basis  # noqa: E402

MASS = 0


def fit_gps(theta_train: np.ndarray, coefficients: np.ndarray, seed: int):
    """One GP per mode. Returns (models, predict(theta) -> (mean, std))."""
    from sklearn.gaussian_process import GaussianProcessRegressor
    from sklearn.gaussian_process.kernels import Matern, WhiteKernel, ConstantKernel

    num_modes = coefficients.shape[1]
    num_inputs = theta_train.shape[1]
    models = []
    for k in range(num_modes):
        # Matern nu=5/2 with ARD (one length scale per input): sample paths are
        # twice differentiable, a weaker and more defensible assumption for a
        # contact-driven system than the analyticity of a squared-exponential.
        # WhiteKernel absorbs the irreducible chaotic scatter, so it is a
        # physical quantity here, not a regularisation knob.
        kernel = (ConstantKernel(1.0, (1e-3, 1e3))
                  * Matern(length_scale=np.ones(num_inputs),
                           # Upper bound generous on purpose: ARD signals an
                           # irrelevant input by sending its length scale to
                           # infinity, and a tight bound turns that into a
                           # spurious "converged at the boundary" warning.
                           length_scale_bounds=(1e-2, 1e6), nu=2.5)
                  + WhiteKernel(noise_level=1e-2, noise_level_bounds=(1e-8, 1e1)))
        model = GaussianProcessRegressor(kernel=kernel, normalize_y=True,
                                         n_restarts_optimizer=4, random_state=seed + k)
        model.fit(theta_train, coefficients[:, k])
        models.append(model)

    def predict(theta: np.ndarray):
        means = np.empty((len(theta), num_modes))
        stds = np.empty((len(theta), num_modes))
        for k, model in enumerate(models):
            means[:, k], stds[:, k] = model.predict(theta, return_std=True)
        return means, stds

    return models, predict


def relative_error(approx: np.ndarray, truth: np.ndarray) -> float:
    return float(np.linalg.norm(approx - truth) / np.linalg.norm(truth))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", default="dataset")
    parser.add_argument("--modes", type=int, default=20)
    parser.add_argument("--test-fraction", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--figure", default="surrogate/stage1_surrogate.png")
    args = parser.parse_args()

    data = dataset_io.load(args.directory)
    grids = data.usable()
    theta = data.parameters[data.ok]
    names = data.parameter_names
    fields = grids[:, -1, MASS].reshape(len(grids), -1)
    num_samples, num_nodes = fields.shape

    rng = np.random.default_rng(args.seed)
    order = rng.permutation(num_samples)
    num_test = max(10, int(round(args.test_fraction * num_samples)))
    test_index, train_index = order[:num_test], order[num_test:]
    print(f"{num_samples} realisations, N={num_nodes} nodes, p={theta.shape[1]} parameters")
    print(f"train {len(train_index)} / test {len(test_index)}, K={args.modes} modes")

    # --- reduction, fitted on the training split only ---
    mean_field, modes, singular_values = pod_basis(fields[train_index])
    basis = modes[:, :args.modes]
    coefficients_train = (fields[train_index] - mean_field) @ basis
    coefficients_test = (fields[test_index] - mean_field) @ basis
    retained = float(np.sum(singular_values[:args.modes] ** 2) / np.sum(singular_values ** 2))
    print(f"variance retained by K={args.modes}: {retained:.4f}")

    # --- input normalisation: theta components differ by an order of magnitude
    #     in range, which conditions the ARD length-scale optimisation badly ---
    lower, upper = theta[train_index].min(axis=0), theta[train_index].max(axis=0)
    scale = np.where(upper > lower, upper - lower, 1.0)
    normalise = lambda t: (t - lower) / scale

    print("\nfitting GPs...")
    models, predict = fit_gps(normalise(theta[train_index]), coefficients_train, args.seed)
    predicted_mean, predicted_std = predict(normalise(theta[test_index]))

    # ----- error decomposition -----
    truth = fields[test_index]
    baseline = np.tile(mean_field, (len(test_index), 1))
    projected = coefficients_test @ basis.T + mean_field      # truncation only
    surrogate = predicted_mean @ basis.T + mean_field         # truncation + regression

    print("\nHELD-OUT FIELD ERROR (relative l2)")
    print(f"  mean field only (K=0)          {relative_error(baseline, truth):.4f}")
    print(f"  POD projection  (truncation)   {relative_error(projected, truth):.4f}")
    print(f"  GP surrogate    (total)        {relative_error(surrogate, truth):.4f}")
    print(f"  regression alone, in coeff.    "
          f"{relative_error(predicted_mean, coefficients_test):.4f}")

    # Per-mode regression quality: the leading modes are what carry the field, so
    # a coefficient-space R^2 broken out by mode says where the map is hard.
    print("\nPER-MODE REGRESSION (held-out)")
    print(f"  {'mode':>5} {'sing.val':>10} {'R^2':>8} {'rel.err':>9} {'mean pred sd':>13}")
    for k in range(min(args.modes, 10)):
        truth_k = coefficients_test[:, k]
        pred_k = predicted_mean[:, k]
        ss_res = float(np.sum((truth_k - pred_k) ** 2))
        ss_tot = float(np.sum((truth_k - truth_k.mean()) ** 2))
        r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")
        print(f"  {k:>5} {singular_values[k]:>10.3f} {r2:>8.3f} "
              f"{relative_error(pred_k, truth_k):>9.3f} {predicted_std[:, k].mean():>13.4f}")

    # ----- uncertainty calibration -----
    # The predictive variance is the deliverable, not a bonus: it is how the
    # model states what it cannot know. Check it against realised error.
    z_scores = (predicted_mean - coefficients_test) / np.maximum(predicted_std, 1e-12)
    inside_1 = float(np.mean(np.abs(z_scores) <= 1.0))
    inside_2 = float(np.mean(np.abs(z_scores) <= 2.0))
    print("\nUNCERTAINTY CALIBRATION (all modes pooled)")
    print(f"  fraction within +/-1 sd: {inside_1:.3f}  (nominal 0.683)")
    print(f"  fraction within +/-2 sd: {inside_2:.3f}  (nominal 0.954)")
    print(f"  z-score RMS:             {float(np.sqrt(np.mean(z_scores ** 2))):.3f}  (nominal 1.0)")

    # ----- ARD length scales: sensitivity ranking, free from Eq. (mll) -----
    length_scales = np.full((args.modes, theta.shape[1]), np.inf)
    for k, model in enumerate(models):
        for part in model.kernel_.get_params().values():
            if hasattr(part, "length_scale") and np.size(part.length_scale) == theta.shape[1]:
                length_scales[k] = np.asarray(part.length_scale)

    # Aggregate RELEVANCE (1/length_scale), not length scale.
    #
    # This matters and is easy to get wrong. The POD modes split a symmetric pair
    # of inputs between separate modes: mode 0 is the x-displacement mode (driven
    # by throw_vx, insensitive to throw_vy) and mode 1 the y-displacement mode
    # (the reverse), with near-equal singular values as x/y symmetry demands.
    # A singular-value-weighted average of LENGTH SCALES then ranks throw_vx above
    # throw_vy purely because sigma_0 marginally exceeds sigma_1 -- an artefact of
    # the aggregation, not a property of the system. Averaging 1/ell instead asks
    # "does this input strongly drive ANY high-energy mode", which is the question
    # intended and is invariant to how the basis distributes a symmetric pair.
    weights = singular_values[:args.modes] ** 2
    weights = weights / weights.sum()
    relevance = (weights[:, None] * (1.0 / length_scales)).sum(axis=0)
    print("\nPARAMETER SENSITIVITY from ARD (relevance = sigma^2-weighted mean of 1/length_scale)")
    for index in np.argsort(-relevance):
        print(f"  {names[index]:<12} {relevance[index]:>9.4f}")

    print("\nPER-MODE ARD LENGTH SCALES (leading modes; large => that input is irrelevant)")
    print("  mode " + " ".join(f"{n[:9]:>10}" for n in names))
    for k in range(min(args.modes, 6)):
        cells = " ".join(f"{v:>10.2f}" if v < 1e4 else f"{'inf':>10}"
                         for v in length_scales[k])
        print(f"  {k:>4}  {cells}")

    # ----- figures -----
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        shape = grids.shape[3:]
        worst = int(np.argmax(np.linalg.norm(surrogate - truth, axis=1)
                              / np.linalg.norm(truth, axis=1)))
        median = int(np.argsort(np.linalg.norm(surrogate - truth, axis=1)
                               / np.linalg.norm(truth, axis=1))[len(test_index) // 2])
        fig, axes = plt.subplots(2, 4, figsize=(17, 7.5))
        for row, (case, label) in enumerate([(median, "median"), (worst, "worst")]):
            true_field = truth[case].reshape(shape).sum(axis=2)      # column density
            pred_field = surrogate[case].reshape(shape).sum(axis=2)
            vmax = max(true_field.max(), pred_field.max())
            for col, (field, title) in enumerate([
                    (true_field, "ground truth"), (pred_field, "surrogate"),
                    (pred_field - true_field, "difference")]):
                kw = dict(cmap="magma", vmin=0, vmax=vmax) if col < 2 else \
                     dict(cmap="coolwarm", vmin=-vmax * 0.5, vmax=vmax * 0.5)
                image = axes[row, col].imshow(field.T, origin="lower", **kw)
                axes[row, col].set_title(f"{label}: {title}")
                axes[row, col].set_xticks([]); axes[row, col].set_yticks([])
                plt.colorbar(image, ax=axes[row, col], fraction=0.046)
            axes[row, 3].plot(coefficients_test[case], "o-", label="projected (truth)")
            axes[row, 3].errorbar(np.arange(args.modes), predicted_mean[case],
                                  yerr=2 * predicted_std[case], fmt="s--",
                                  capsize=3, label="GP $\\pm 2\\sigma$")
            axes[row, 3].set(xlabel="mode index", ylabel="coefficient",
                             title=f"{label}: modal coefficients")
            axes[row, 3].grid(alpha=0.3); axes[row, 3].legend(fontsize=8)
        fig.suptitle("Stage 1 surrogate: terminal mass field, column density "
                     f"(K={args.modes} modes, {len(test_index)} held-out realisations)")
        fig.tight_layout()
        os.makedirs(os.path.dirname(args.figure) or ".", exist_ok=True)
        fig.savefig(args.figure, dpi=110)
        print(f"\nwrote {args.figure}")
    except ImportError:
        print("\n(matplotlib unavailable; skipping figure)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
