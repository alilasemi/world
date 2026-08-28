"""Measure the irreducible scatter of the coarse field, from build/bin/chaos output.

This is the denominator every model error in this repository is reported against, and it must
be re-measured for every configuration and horizon. It is NOT the pairwise difference between
two realizations. Writing a realization as s = mu + n with independent zero-mean n,

    ||s_a - s_b|| = sqrt(2) ||n||        while a model predicting mu incurs only ||n||,

so the pairwise value overstates the floor by sqrt(2) and would make a model look nearly
optimal when it is not. The scatter about the ensemble MEAN is the right quantity, and
chaos/final_fields.bin exists so it can be measured directly rather than inferred through that
factor. Both are printed here, together with their ratio, as a check on the relation.

  ./build/bin/chaos chaos/ensemble_heavy.yaml 16
  surrogate/.venv/bin/python surrogate/measure_floor.py --fields chaos/final_fields.bin
"""

from __future__ import annotations

import argparse
import itertools

import numpy as np


def load(path: str) -> np.ndarray:
    """(members, nodes) terminal mass fields; member 0 is the unperturbed reference."""
    with open(path, "rb") as handle:
        members, nodes = np.frombuffer(handle.read(8), dtype=np.int32)
        data = np.frombuffer(handle.read(), dtype=np.float32)
    if data.size != members * nodes:
        raise ValueError(f"{path}: expected {members * nodes} floats, found {data.size}")
    return data.reshape(int(members), int(nodes)).astype(np.float64)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fields", default="chaos/final_fields.bin")
    args = parser.parse_args()

    fields = load(args.fields)
    members = len(fields)
    mean = fields.mean(axis=0)

    # Scatter about the ensemble mean: the error a model predicting the conditional mean incurs.
    deviations = np.linalg.norm(fields - mean, axis=1) / np.linalg.norm(mean)
    floor = float(np.sqrt((deviations ** 2).mean()))

    # Pairwise difference, for the sqrt(2) check.
    pairs = [np.linalg.norm(fields[a] - fields[b]) / np.linalg.norm(mean)
             for a, b in itertools.combinations(range(members), 2)]
    pairwise = float(np.sqrt(np.mean(np.square(pairs))))

    print(f"{args.fields}: {members} realizations, {fields.shape[1]} nodes")
    print(f"  scatter about the ensemble mean   {floor:.4f}   <- the floor")
    print(f"  pairwise difference               {pairwise:.4f}")
    print(f"  ratio                             {pairwise / floor:.4f}   "
          f"against sqrt(2) = {np.sqrt(2):.4f}")
    print(f"  per-realization deviation         min {deviations.min():.4f} "
          f"max {deviations.max():.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
