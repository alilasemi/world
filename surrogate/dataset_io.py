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

    def usable(self) -> np.ndarray:
        """Grids for the usable rollouts, WITHOUT copying when they all are.

        `grids[ok]` is boolean fancy indexing, so it always allocates a full second array
        even when the mask selects everything. At 16 GB that is what put two 29 GB Python
        processes in front of the OOM killer. Every dataset generated so far has had zero
        diverged rollouts, so the copy bought nothing; this returns the array itself in
        that case and only pays for a copy when some rollout really has to be dropped.
        """
        mask = self.ok
        return self.grids if mask.all() else self.grids[mask]

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


def load(directory: str = "dataset", mmap: bool = True) -> Dataset:
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
        offset = handle.tell()
    actual = (os.path.getsize(grid_path) - offset) // 4
    if actual != expected:
        raise ValueError(f"{grid_path}: expected {expected} floats after the header, "
                         f"found {actual}")
    shape = (num_rollouts, num_checkpoints, channels, nx, ny, nz)
    if mmap:
        # Memory-MAPPED, not read. grids.bin is 16 GB for a 64^2 x 16 latent, and
        # np.fromfile would put all of it in anonymous memory, which the kernel cannot
        # evict -- so a second consumer, or one incautious copy, reaches the OOM killer.
        # A memmap is file-backed page cache: it counts against nothing that must be kept
        # resident, and slicing it materializes only the slice that is asked for.
        grids = np.memmap(grid_path, dtype="<f4", mode="r", offset=offset, shape=shape)
    else:
        with open(grid_path, "rb") as handle:
            handle.seek(offset)
            grids = np.fromfile(handle, dtype="<f4", count=expected).reshape(shape)

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
