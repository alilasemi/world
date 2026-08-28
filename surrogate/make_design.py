"""Generate a Latin-hypercube design over the throw-parameter space.

Written to `dataset/design.csv`, which `build/bin/dataset` reads one row at a
time -- one row is one rollout.

Why LHS and not a tensor grid: a full-factorial grid costs k**d points and dies
past about four dimensions (the existing `dt x max_force x restitution` sweep in
stability_and_accuracy is exactly that design, and it is already at its limit at
three axes). A Latin hypercube gives one sample per stratum on *every* axis
independently, so the number of points is decoupled from the dimension and the
marginal coverage of each parameter is uniform by construction. Scrambled Sobol
is used when scipy is available since it has better projection properties than
plain LHS for the same budget.

The parameter space is deliberately free of *pure-translation* nuisance
parameters: the release x/y position is fixed at the domain center rather than
sampled. Sampling it would translate the whole outcome field rigidly, which
carries no physics and is the worst possible case for a linear (POD) basis --
see the Kolmogorov-barrier note in CLAUDE.md. Release *velocity* also moves the
outcome, but couples that motion to real physics (spread grows with speed), so
it stays.
"""

import argparse
import csv
import sys

import numpy as np

# name, low, high, kind
#   "action"   -- the throw itself, what an agent would choose
#   "material" -- what system identification would try to infer from data
# Velocity and release-height ranges are set to keep the blob off the walls.
# Measured on the first pilot (+/-3 m/s, release_z +/-0.6): 136 of 150 rollouts
# ended with >10% of their mass piled within two nodes of a boundary, and 31 with
# >50%. A wall-saturated outcome is CLIPPED -- the surrogate would learn "which
# wall it hit" instead of the throw physics, and the wall compresses the output
# distribution. Narrower ranges keep the landing point interior.
PARAMETERS = [
    ("throw_vx",    -0.7,   0.7,   "action"),
    ("throw_vy",    -0.7,   0.7,   "action"),
    ("throw_vz",    -0.6,   0.6,   "action"),
    ("release_z",   -0.35,  0.0,   "action"),
    # Lower bound raised from 0.02: lighter grains mean a stiffer contact
    # relative to inertia (shorter contact period), and 0.02 leaves only ~14
    # steps per contact even at dt=1e-4. See dataset/blob_throw.yaml.
    ("grain_mass",   0.06,  0.14,  "material"),
    ("restitution",  0.15,  0.60,  "material"),
    ("friction",     0.20,  0.60,  "material"),
]

# A SECOND parameter set, for the 32,000-grain study at dt=1e-3 (dataset/blob_heavy.yaml).
# Selected with --preset heavy; the default preset above is unchanged.
#
# The grain mass range is set by the timestep, not by taste. The contact period is
# 2*pi*sqrt(m_reduced/k) with k = max_force/radius = 1e6 N/m, so 20.3 kg gives exactly 20 steps
# per contact at dt=1e-3 and 45.6 kg gives 30. Below about 20 steps this solver does not
# integrate a contact; see the probe record in CLAUDE.md.
#
# Throw velocities are ~10x the first study's, on purpose. There the throw was +/-0.7 m/s
# against a 4 m/s impact speed, so the ACTION was a small perturbation on a splat. Here the
# blob is released 0.2-0.8 m above the floor and thrown at up to 1.5 m/s per horizontal axis,
# which moves the deposit centroid by ~10 grain diameters, so the action dominates the outcome.
# Probed at the worst corner (vx=vy=1.5, vz=-1.0, heaviest grain): the deposit still clears the
# side walls by 1.37x with no grain within two diameters of one.
HEAVY_PARAMETERS = [
    ("throw_vx",    -1.5,   1.5,   "action"),
    ("throw_vy",    -1.5,   1.5,   "action"),
    ("throw_vz",    -1.0,   1.0,   "action"),
    ("release_z",   -9.8,  -9.2,   "action"),
    ("grain_mass",  20.3,  45.5,   "material"),
    ("restitution",  0.15,  0.60,  "material"),
    ("friction",     0.20,  0.60,  "material"),
]

PRESETS = {"blob": None, "heavy": HEAVY_PARAMETERS}   # "blob" resolves to PARAMETERS below

# Ranges narrowed ~40% from the first dynamics study, deliberately. An exact
# Gaussian process is O(n^3) per likelihood evaluation, so it cannot consume an
# arbitrarily large database however many rollouts are generated; what a bigger
# database buys is a better POD basis and a denser-covering training SUBSET. The
# way to actually reduce per-step regression error at fixed n is therefore to
# shrink the domain the same n has to cover. Generality is traded for accuracy
# knowingly here.


def sample_unit(num_samples: int, num_dimensions: int, seed: int) -> np.ndarray:
    """Points in [0,1]^d. Scrambled Sobol if available, else stratified LHS."""
    try:
        from scipy.stats import qmc
        engine = qmc.Sobol(d=num_dimensions, scramble=True, seed=seed)
        # Sobol's balance guarantees only hold for n = 2**m, so ask for the next
        # power of two and trim. Trimming preserves the low-discrepancy prefix
        # (Sobol points are nested), unlike asking for a non-power-of-two
        # directly, which scipy warns about for exactly this reason.
        import math
        power_of_two = 1 << max(1, math.ceil(math.log2(max(num_samples, 2))))
        points = engine.random(power_of_two)[:num_samples]
        if power_of_two != num_samples:
            print(f"note: drew {power_of_two} Sobol points (next power of 2) "
                  f"and kept the first {num_samples}")
        return points
    except ImportError:
        rng = np.random.default_rng(seed)
        # Classic LHS: one sample per stratum per axis, independently permuted.
        strata = (np.arange(num_samples)[:, None] + rng.random((num_samples, num_dimensions)))
        strata /= num_samples
        for dimension in range(num_dimensions):
            rng.shuffle(strata[:, dimension])
        return strata


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rollouts", type=int, default=150,
                        help="number of design points (default: the pilot size)")
    parser.add_argument("--seed", type=int, default=20260825)
    parser.add_argument("--out", default="dataset/design.csv")
    parser.add_argument("--preset", choices=sorted(PRESETS), default="blob",
                        help="'blob' is the original 3375-grain study; 'heavy' is the "
                             "32,000-grain study that runs at dt=1e-3")
    args = parser.parse_args()

    parameters = PRESETS[args.preset] or PARAMETERS
    unit = sample_unit(args.rollouts, len(parameters), args.seed)
    columns = []
    for index, (_, low, high, _) in enumerate(parameters):
        columns.append(low + (high - low) * unit[:, index])
    design = np.column_stack(columns)

    import os
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", newline="") as handle:
        # lineterminator="\n": the default "\r\n" leaves a stray CR on the last
        # field of every line for any reader that splits on "\n" alone.
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow([name for name, _, _, _ in parameters])
        for row in design:
            writer.writerow([f"{value:.9g}" for value in row])

    print(f"wrote {args.rollouts} design points x {len(parameters)} parameters "
          f"to {args.out} (preset '{args.preset}')")
    for index, (name, low, high, kind) in enumerate(parameters):
        values = design[:, index]
        print(f"  {name:<12} [{low:>6.2f}, {high:>6.2f}] {kind:<9}"
              f" sampled min={values.min():>7.3f} max={values.max():>7.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
