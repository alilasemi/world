"""Decode a latent rollout back into particle positions and velocities.

Pipeline, end to end:

    observed initial field  --project-->  z(t_1)
    z(t_k) --GP step--> z(t_{k+1})            (autoregressive, Stage 2)
    z(t_k) --affine--> (rho, rho*u) fields    (per-channel POD bases)
    fields --sample-->  particle positions and velocities

The last arrow is the one this file adds. It is a *sampler*, not an inverse:
interpolating particles onto a grid is many-to-one, so a field has infinitely
many particle configurations consistent with it. Sampling draws one of them.
Consequently decode(encode(x)) != x in general -- it reproduces the FIELD, not
the configuration -- and that is the honest way to describe what comes out.

The sampling itself:

  1. Clamp negative density. A truncated linear reconstruction
     rho = rho_bar + Phi a is not guaranteed non-negative, and near-empty
     regions do go slightly negative; a negative probability mass is not
     samplable. The fraction of mass discarded is reported.
  2. Rescale to the known total mass. n_p and the per-grain mass are both known
     from theta, so the exact total is known and the decode can be made exactly
     mass-conserving even though the reconstruction is not.
  3. Draw node indices multinomially with probability proportional to nodal mass.
  4. Place each particle uniformly within its node's cell. This reproduces the
     nodal mass in expectation and is the natural counterpart of the hat-function
     deposit at this resolution.
  5. Assign velocity by interpolating the momentum and mass fields at the
     particle's sampled position with the SAME hat weights used to deposit, then
     dividing: v = sum_a w_a (rho u)_a / sum_a w_a rho_a, guarded where the
     denominator vanishes.

Three limitations are measured rather than asserted, and are printed by this
script:

  * SUB-NODE STRUCTURE IS LOST. Node spacing is ~3 grain diameters, so ~30 grains
    share a node and uniform placement produces a Poisson-like arrangement, not a
    packed one. Grains overlap. The output is therefore valid for visualization
    and for bulk statistics but is NOT a valid state to hand back to the solver;
    doing so would produce enormous contact forces. Making it re-initializable
    needs Poisson-disk sampling with minimum separation 2r, or a short damped
    relaxation.
  * VELOCITY FLUCTUATIONS ARE LOST. Only the mean momentum per node is stored, not
    the second moment (rho u u), so the within-node velocity spread -- granular
    temperature -- is discarded and the sampled kinetic energy is biased low.
    Adding a second-moment channel would recover it.
  * The latent truncation error is inherited: the sampled particles are consistent
    with the *predicted* field, which itself differs from the truth.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dataset_io  # noqa: E402


def read_domain(config_path: str):
    """Pull the domain bounds and grid sizes out of the sim config."""
    text = open(config_path).read()
    def get(key, default=None):
        m = re.search(rf"^\s*{key}\s*:\s*([-\d.eE+]+)", text, re.M)
        if m is None:
            if default is None:
                raise KeyError(key)
            return default
        return float(m.group(1))
    lo = np.array([get("x_min"), get("y_min"), get("z_min")])
    hi = np.array([get("x_max"), get("y_max"), get("z_max")])
    radius = get("particle_radius")
    return lo, hi, radius


def node_positions(lo, hi, shape):
    """Node coordinates, in the flattening order (ix*Ny + iy)*Nz + iz."""
    axes = [np.linspace(lo[j], hi[j], shape[j]) for j in range(3)]
    grid = np.meshgrid(*axes, indexing="ij")
    return np.stack([g.ravel() for g in grid], axis=1), \
           np.array([(hi[j] - lo[j]) / (shape[j] - 1) for j in range(3)])


def sample_particles(mass_field, momentum_fields, lo, hi, shape, spacing,
                     total_mass, grain_mass, rng, threshold=0.0,
                     sampler="uniform", radius=0.01):
    """Fields -> (positions, velocities, diagnostics)."""
    nodes, _ = node_positions(lo, hi, shape)
    rho = mass_field.astype(np.float64)

    negative_mass = float(-rho[rho < 0].sum())
    positive_mass = float(rho[rho > 0].sum())
    rho = np.clip(rho, 0.0, None)
    # Discard nodes below a fraction of the peak density before sampling.
    #
    # A truncated POD reconstruction rings: it leaves small positive densities
    # far from the material. Individually negligible, but spread over all 16384
    # nodes they attract a substantial number of grains and produce a diffuse
    # halo across the whole domain that is absent from the truth. Thresholding
    # removes it; mass is restored by the renormalisation below, so the only cost
    # is that genuinely sparse outlying material is discarded along with the
    # ringing. Reported as `halo mass` so the size of that cost is visible.
    halo_mass = 0.0
    if threshold > 0.0 and rho.max() > 0:
        cut = threshold * rho.max()
        halo_mass = float(rho[rho < cut].sum()) / max(rho.sum(), 1e-30)
        rho = np.where(rho < cut, 0.0, rho)
    if rho.sum() <= 0:
        raise ValueError("decoded density is everywhere non-positive")
    rho *= total_mass / rho.sum()          # exact mass conservation by construction

    num_particles = int(round(total_mass / grain_mass))
    if sampler == "poisson":
        positions, place_diag = poisson_disk_sample(
            rho, nodes, spacing, num_particles, radius, rng, lo, hi)
        num_particles = len(positions)
        counts = np.zeros(len(rho), dtype=int)
    else:
        probability = rho / rho.sum()
        counts = rng.multinomial(num_particles, probability)
        chosen = np.repeat(np.arange(len(counts)), counts)
        # Uniform within the node's cell -- reproduces the nodal mass in
        # expectation but ignores the packing constraint entirely.
        offsets = (rng.random((num_particles, 3)) - 0.5) * spacing
        positions = nodes[chosen] + offsets
        place_diag = {"placed": num_particles, "requested": num_particles, "attempts": 0}
        np.clip(positions, lo + radius, hi - radius, out=positions)

    # Velocity: interpolate momentum and mass with the deposit's own hat weights,
    # then divide. Doing it at the sampled position rather than taking the node
    # value keeps the velocity field continuous across cell boundaries.
    momentum = np.stack([f.astype(np.float64) for f in momentum_fields], axis=1)
    velocities = np.zeros((num_particles, 3))
    grid_index = np.floor((positions - lo) / spacing).astype(int)
    np.clip(grid_index, 0, np.array(shape) - 2, out=grid_index)
    frac = (positions - lo) / spacing - grid_index
    numerator = np.zeros((num_particles, 3))
    denominator = np.zeros(num_particles)
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                weight = (np.where(dx, frac[:, 0], 1 - frac[:, 0])
                          * np.where(dy, frac[:, 1], 1 - frac[:, 1])
                          * np.where(dz, frac[:, 2], 1 - frac[:, 2]))
                flat = ((grid_index[:, 0] + dx) * shape[1]
                        + (grid_index[:, 1] + dy)) * shape[2] + (grid_index[:, 2] + dz)
                numerator += weight[:, None] * momentum[flat]
                denominator += weight * rho[flat]
    usable = denominator > 1e-12
    velocities[usable] = numerator[usable] / denominator[usable, None]

    diagnostics = {
        "negative_mass_fraction": negative_mass / max(positive_mass, 1e-30),
        "halo_mass_fraction": halo_mass,
        "num_particles": num_particles,
        "nodes_occupied": int((counts > 0).sum()),
        "max_per_node": int(counts.max()) if counts.size else 0,
        "placed": place_diag["placed"],
        "requested": place_diag["requested"],
        "attempts": place_diag["attempts"],
    }
    return positions, velocities, diagnostics


def poisson_disk_sample(rho, nodes, spacing, num_particles, radius, rng,
                        domain_lo, domain_hi, attempt_budget=200):
    """Sample positions with density proportional to `rho` and NO pair closer
    than one diameter (2*radius).

    Dart throwing with density-proportional proposals, accelerated by a spatial
    hash of cell size 2*radius so a candidate need only be tested against the 27
    neighbouring cells. Bridson's algorithm is the usual choice for Poisson-disk
    sets but generates a *uniform* density; here the density is prescribed by the
    field, and the disk radius is fixed by physics, so proposals are drawn from
    the field instead.

    Where the field demands a density above the maximum packing fraction, the
    grains simply cannot be placed. That is not a defect of the sampler: it means
    the predicted field is locally unphysical, and the shortfall is reported
    rather than hidden.
    """
    cell = 2.0 * radius
    lo = nodes.min(axis=0) - spacing
    probability = rho / rho.sum()
    order = np.arange(len(rho))

    accepted = np.empty((num_particles, 3))
    count = 0
    buckets: dict = {}
    neighbourhood = [(dx, dy, dz) for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1)]

    # Propose in batches: drawing one multinomial sample at a time dominates the
    # runtime otherwise.
    attempts = 0
    max_attempts = attempt_budget * num_particles
    while count < num_particles and attempts < max_attempts:
        batch = min(4096, (num_particles - count) * 8)
        chosen = rng.choice(order, size=batch, p=probability)
        offsets = (rng.random((batch, 3)) - 0.5) * spacing
        candidates = nodes[chosen] + offsets
        # Reject rather than clip. Clipping a candidate onto the domain boundary
        # was the original bug here: the pile rests on the floor, so a large
        # fraction of proposals fall below it, and clamping them all to z = z_min
        # collapses a layer into a single plane and destroys the separation the
        # sampler had just enforced (measured: 36% of pairs overlapping despite a
        # correct disk test). A grain centre must also stay a full radius clear of
        # each wall, or it is half-buried in it.
        inside = np.all((candidates >= domain_lo + radius)
                        & (candidates <= domain_hi - radius), axis=1)
        for candidate in candidates[inside]:
            attempts += 1
            key = tuple(((candidate - lo) / cell).astype(int))
            clear = True
            for d in neighbourhood:
                bucket = buckets.get((key[0] + d[0], key[1] + d[1], key[2] + d[2]))
                if not bucket:
                    continue
                for index in bucket:
                    if np.sum((accepted[index] - candidate) ** 2) < cell * cell:
                        clear = False
                        break
                if not clear:
                    break
            if clear:
                accepted[count] = candidate
                buckets.setdefault(key, []).append(count)
                count += 1
                if count == num_particles:
                    break
    return accepted[:count], {"placed": count, "requested": num_particles,
                              "attempts": attempts}


def overlap_fraction(positions, radius, rng, sample=2000):
    """Fraction of sampled grains closer than one diameter to their nearest
    neighbour -- i.e. how far the output is from being a valid packed state."""
    n = len(positions)
    idx = rng.choice(n, min(sample, n), replace=False)
    try:
        from scipy.spatial import cKDTree
        tree = cKDTree(positions)
        distance, _ = tree.query(positions[idx], k=2)
        nearest = distance[:, 1]
    except ImportError:
        nearest = np.array([np.partition(
            np.linalg.norm(positions - positions[i], axis=1), 1)[1] for i in idx])
    return float((nearest < 2 * radius).mean()), float(np.median(nearest) / (2 * radius))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--model", default="surrogate/stage2_model.joblib")
    parser.add_argument("--config", default="dataset/blob_dynamics2.yaml")
    parser.add_argument("--rollout", type=int, default=0,
                        help="index into the held-out set; guaranteed to interpolate the "
                             "training design rather than extrapolate it")
    parser.add_argument("--sampler", choices=["uniform", "poisson"], default="poisson",
                        help="'uniform' places grains uniformly in each node's cell, ignoring "
                             "the packing constraint (fine for visualization). 'poisson' enforces "
                             "a minimum separation of one diameter, which is what makes the state "
                             "acceptable to the solver.")
    parser.add_argument("--threshold", type=float, default=0.02,
                        help="drop nodes below this fraction of peak density before sampling, "
                             "to suppress the halo produced by POD ringing")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--out", default="decoded")
    parser.add_argument("--figure", default="surrogate/decoded_particles.png")
    args = parser.parse_args()

    import joblib
    art = joblib.load(args.model)
    bases, models = art["bases"], art["models"]
    centre, spread, ranks = art["centre"], art["spread"], art["ranks"]
    shape, times = art["shape"], art["checkpoint_times"]
    names, material_names = art["parameter_names"], art["material_parameters"]

    data = dataset_io.load(art["directory"])
    grids, theta = data.usable(), data.parameters[data.ok]
    test = art["test_rollouts"]
    r = int(test[args.rollout % len(test)])
    lo, hi, radius = read_domain(args.config)
    _, spacing = node_positions(lo, hi, shape)
    rng = np.random.default_rng(args.seed)

    material_index = [names.index(n) for n in material_names]
    theta_m = theta[r, material_index]
    grain_mass = float(theta[r, names.index("grain_mass")])
    print(f"held-out rollout {r}")
    for n, v in zip(names, theta[r]):
        print(f"  {n:<12} {v:+.4f}")

    # --- encode the observed initial field, then roll out ---
    def encode_snapshot(k):
        pieces = []
        for c, b in enumerate(bases):
            flat = grids[r, k, c].reshape(-1)
            pieces.append((flat - b["mean"]) @ b["modes"])
        return np.concatenate(pieces)

    latent = encode_snapshot(0)
    trajectory = [latent.copy()]
    for _ in range(1, len(times)):
        x = ((np.concatenate([trajectory[-1], theta_m]) - centre) / spread)[None, :]
        trajectory.append(np.array([m.predict(x)[0] for m in models]))
    trajectory = np.array(trajectory)
    print(f"\nrolled out {len(trajectory)} snapshots, Delta = {art['delta']:.3f} s")

    # --- decode each snapshot to fields, then sample particles ---
    os.makedirs(args.out, exist_ok=True)
    total_mass = float(grids[r, 0, 0].sum())
    print(f"\n{'t (s)':>7} {'neg mass':>9} {'n_p':>7} {'occupied':>9} "
          f"{'halo':>7} {'centroid err':>13} {'spread err':>11} "
          f"{'KE true':>10} {'KE sampled':>11}")
    records, summary = [], []
    for k, t in enumerate(times):
        fields = []
        offset = 0
        for c, b in enumerate(bases):
            a = trajectory[k][offset:offset + b["rank"]]
            fields.append(a @ b["modes"].T + b["mean"])
            offset += b["rank"]
        positions, velocities, diag = sample_particles(
            fields[0], fields[1:], lo, hi, shape, spacing, total_mass, grain_mass, rng,
            threshold=args.threshold, sampler=args.sampler, radius=radius)

        # Compare bulk moments against the TRUE field at the same instant.
        truth = grids[r, k, 0].reshape(-1).astype(np.float64)
        nodes, _ = node_positions(lo, hi, shape)
        true_total = truth.sum()
        true_centroid = (truth[:, None] * nodes).sum(axis=0) / true_total
        true_spread = np.sqrt((truth[:, None] * (nodes - true_centroid) ** 2).sum(axis=0)
                              / true_total)
        centroid = positions.mean(axis=0)
        spread_sampled = positions.std(axis=0)
        # Kinetic energy from the grid is sum_a |rho u_a|^2 / (2 rho_a): the momentum
        # must be divided by the density AT EACH NODE, not by its mean. (An earlier
        # version divided by the mean and produced ratios of 10^3, which is how the
        # error announced itself.) Reported as absolute energies, not a ratio: once
        # the assembly is at rest both numerator and denominator go to zero and a
        # ratio of two vanishing quantities is meaningless.
        true_momentum = np.stack([grids[r, k, 1 + j].reshape(-1) for j in range(3)], axis=1)
        live = truth > 1e-10
        true_ke = float(0.5 * ((true_momentum[live] ** 2).sum(axis=1) / truth[live]).sum())
        sampled_ke = float(0.5 * grain_mass * (velocities ** 2).sum())

        np.savez_compressed(os.path.join(args.out, f"snapshot_{k:03d}.npz"),
                            positions=positions.astype(np.float32),
                            velocities=velocities.astype(np.float32), time=t)
        # Also write the flat binary the solver reads (init_type: file): int32 n,
        # then n * kStateStride float32 laid out [x, y, z, vx, vy, vz] per
        # particle, matching ParticleDynamics::host_state exactly.
        with open(os.path.join(args.out, f"state_{k:03d}.bin"), "wb") as handle:
            handle.write(np.int32(len(positions)).tobytes())
            interleaved = np.concatenate([positions, velocities], axis=1).astype(np.float32)
            handle.write(interleaved.tobytes())
        records.append((positions, velocities, t))
        summary.append((t, diag["negative_mass_fraction"],
                        np.linalg.norm(centroid - true_centroid),
                        np.linalg.norm(spread_sampled - true_spread)))
        print(f"{t:>7.2f} {diag['negative_mass_fraction']:>9.4f} {diag['num_particles']:>7} "
              f"{diag['nodes_occupied']:>9} {diag['halo_mass_fraction']:>7.4f} "
              f"{np.linalg.norm(centroid - true_centroid):>13.4f} "
              f"{np.linalg.norm(spread_sampled - true_spread):>11.4f} "
              f"{true_ke:>10.4f} {sampled_ke:>11.4f}")

    frac, median_gap = overlap_fraction(records[-1][0], radius, rng)
    print(f"\nVALIDITY AS A SIMULATION STATE (final snapshot)")
    print(f"  grains closer than one diameter to a neighbour: {frac:.3f}")
    print(f"  median nearest-neighbour distance / diameter:   {median_gap:.3f}")
    print("  => a packed state would have ~0 overlaps and a ratio near 1; anything well")
    print("     below that is a visualization/statistics decode, not a re-initializable state.")

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        picks = [0, len(records) // 3, 2 * len(records) // 3, len(records) - 1]
        fig, axes = plt.subplots(2, len(picks), figsize=(4 * len(picks), 8))
        for col, k in enumerate(picks):
            pos, _, t = records[k]
            axes[0, col].scatter(pos[:, 0], pos[:, 1], s=1.2, alpha=0.35, c="tab:blue", lw=0)
            axes[0, col].set(title=f"sampled, t={t:.2f}s", xlim=(lo[0], hi[0]),
                             ylim=(lo[1], hi[1]), aspect="equal")
            truth = grids[r, k, 0].reshape(shape).sum(axis=2)
            axes[1, col].imshow(truth.T, origin="lower", cmap="magma",
                                extent=[lo[0], hi[0], lo[1], hi[1]])
            axes[1, col].set(title=f"true field, t={t:.2f}s", aspect="equal")
        axes[0, 0].set_ylabel("y (m)  -- sampled particles, top view")
        axes[1, 0].set_ylabel("y (m)  -- ground-truth column density")
        fig.suptitle(f"Latent rollout decoded to particles, held-out rollout {r}")
        fig.tight_layout()
        fig.savefig(args.figure, dpi=110)
        print(f"\nwrote {args.figure} and {len(records)} snapshots to {args.out}/")
    except ImportError:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
