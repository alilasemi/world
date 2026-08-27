"""Reader for the dataset written by build/bin/dataset.

The binary is self-describing (magic + shape header), so nothing here needs to
be kept in sync with the config that generated it -- which matters because a
silently-mismatched shape would reinterpret the same bytes as a different field
and produce plausible-looking garbage.

Layout of dataset/grids.bin:

    char   magic[8]          "RTPGRD01"
    int32  num_rollouts
    int32  num_checkpoints
    int32  channels          1 + kDim  (mass, then momentum per axis)
    int32  nx, ny, nz
    int32  num_parameters
    float32 checkpoint_times[num_checkpoints]
    float32 grids[num_rollouts][num_checkpoints][channels][nx][ny][nz]

Channel 0 is mass; channels 1..3 are momentum density (rho*u). Diverged
rollouts are zero-filled -- always filter on the manifest's `status` column
rather than assuming every row is usable.
"""

from __future__ import annotations

import csv
import os
from dataclasses import dataclass

import numpy as np

MAGIC = b"RTPGRD01"
MASS = 0  # channel index


@dataclass
class Dataset:
    grids: np.ndarray            # (rollouts, checkpoints, channels, nx, ny, nz) float32
    checkpoint_times: np.ndarray  # (checkpoints,) float32
    parameters: np.ndarray       # (rollouts, num_parameters) float64
    parameter_names: list[str]
    status: list[str]            # per rollout: "ok" | "diverged"

    @property
    def ok(self) -> np.ndarray:
        """Boolean mask of usable rollouts."""
        return np.array([s == "ok" for s in self.status])

    def mass(self) -> np.ndarray:
        """Mass channel only: (rollouts, checkpoints, nx, ny, nz)."""
        return self.grids[:, :, MASS]

    def momentum(self) -> np.ndarray:
        """Momentum channels: (rollouts, checkpoints, 3, nx, ny, nz)."""
        return self.grids[:, :, MASS + 1:]

    def velocity(self, floor: float = 1e-8) -> np.ndarray:
        """Mass-weighted velocity = momentum / mass, zero where there is no mass.

        The guard is not optional: empty nodes have exactly zero mass, so a bare
        division yields 0/0. (The deleted occupancy_velocity_kernel carried the
        same guard for the same reason.)
        """
        mass = self.mass()[:, :, None]
        return np.divide(self.momentum(), mass, out=np.zeros_like(self.momentum()),
                         where=mass > floor)


def load(directory: str = "dataset") -> Dataset:
    grid_path = os.path.join(directory, "grids.bin")
    manifest_path = os.path.join(directory, "manifest.csv")

    with open(grid_path, "rb") as handle:
        magic = handle.read(8)
        if magic != MAGIC:
            raise ValueError(f"{grid_path}: bad magic {magic!r}, expected {MAGIC!r}")
        header = np.frombuffer(handle.read(7 * 4), dtype="<i4")
        num_rollouts, num_checkpoints, channels, nx, ny, nz, num_parameters = header.tolist()
        checkpoint_times = np.frombuffer(handle.read(num_checkpoints * 4), dtype="<f4")
        expected = num_rollouts * num_checkpoints * channels * nx * ny * nz
        grids = np.fromfile(handle, dtype="<f4", count=expected)
        if grids.size != expected:
            raise ValueError(f"{grid_path}: expected {expected} floats, got {grids.size}")
        grids = grids.reshape(num_rollouts, num_checkpoints, channels, nx, ny, nz)

    names: list[str] = []
    parameters: list[list[float]] = []
    status: list[str] = []
    with open(manifest_path, newline="") as handle:
        reader = csv.reader(handle)
        header_row = next(reader)
        # rollout, <parameters...>, status, final_time, overflow_cells, deposited_mass
        names = header_row[1:1 + num_parameters]
        status_index = header_row.index("status")
        for row in reader:
            parameters.append([float(value) for value in row[1:1 + num_parameters]])
            status.append(row[status_index])

    if len(status) != num_rollouts:
        raise ValueError(f"manifest has {len(status)} rows but grids has {num_rollouts}")

    return Dataset(grids=grids, checkpoint_times=np.array(checkpoint_times),
                   parameters=np.array(parameters), parameter_names=names, status=status)


if __name__ == "__main__":
    data = load()
    print(f"grids           {data.grids.shape}  {data.grids.dtype}")
    print(f"checkpoints     {data.checkpoint_times}")
    print(f"parameters      {data.parameters.shape}  {data.parameter_names}")
    print(f"usable rollouts {data.ok.sum()}/{len(data.status)}")
