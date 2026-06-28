#!/usr/bin/env python3
"""RL training: CNN policy learns to push particles to form a sphere at domain center.

Usage:
  python rl_train.py [--config config.yaml] [--launch-server] [--episodes 1000]

Protocol (WebSocket):
  TEXT "initialize"    → 5 BINARY messages: n, grid_size, num_triangles, radius, xy
  TEXT "get_occupancy" → BINARY: m*m int32 occupancy grid
  BINARY <2*m*m floats> → (no response) upload force grid
  TEXT "run"           → BINARY: (2*n + 7) floats = xy + metadata
"""

import argparse
import asyncio
import struct
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Normal
import yaml
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import websockets


# ── Config ────────────────────────────────────────────────────────────────────

class SimCfg:
    def __init__(self, path: str):
        with open(path) as f:
            raw = yaml.safe_load(f)

        def get(section, key, default):
            return raw.get(section, {}).get(key, default)

        self.force_grid_size  = get('simulation', 'force_grid_size', 16)
        self.max_force        = get('rl', 'max_force', 10.0)
        self.x_min            = get('domain', 'x_min', -1.0)
        self.x_max            = get('domain', 'x_max',  1.0)
        self.y_min            = get('domain', 'y_min', -1.0)
        self.y_max            = get('domain', 'y_max',  1.0)
        self.port             = get('drivers', 'port', 8081)
        self.steps_per_frame  = get('drivers', 'steps_per_frame', 10)
        self.cx = (self.x_min + self.x_max) / 2
        self.cy = (self.y_min + self.y_max) / 2


# ── Environment ───────────────────────────────────────────────────────────────

class ParticleEnv:
    """Async wrapper around the WebSocket simulation server."""

    def __init__(self, ws, cfg: SimCfg):
        self.ws  = ws
        self.cfg = cfg
        self.m   = cfg.force_grid_size
        self.n   = 0
        self.xs  = np.zeros(1, dtype=np.float32)
        self.ys  = np.zeros(1, dtype=np.float32)

    async def reset(self) -> np.ndarray:
        """Send 'initialize', read 5 handshake messages, return occupancy obs."""
        await self.ws.send("initialize")

        msg = await self.ws.recv()
        self.n = struct.unpack_from('<i', msg)[0]
        await self.ws.recv()   # grid_size (not needed here)
        await self.ws.recv()   # num_triangles
        await self.ws.recv()   # particle_radius

        msg = await self.ws.recv()   # n*2 floats: initial xy
        xy = np.frombuffer(msg, dtype=np.float32).reshape(self.n, 2)
        self.xs, self.ys = xy[:, 0].copy(), xy[:, 1].copy()

        return await self._get_occupancy()

    async def step(self, action: np.ndarray):
        """Upload forces, advance sim, return (obs, reward, done)."""
        # action shape: (2, m, m) — [0] = force_x, [1] = force_y, row-major
        force_bytes = action.astype(np.float32).tobytes()
        await self.ws.send(force_bytes)   # BINARY → force upload, no response

        await self.ws.send("run")
        msg = await self.ws.recv()
        floats = np.frombuffer(msg, dtype=np.float32)
        xy = floats[:self.n * 2].reshape(self.n, 2)
        self.xs, self.ys = xy[:, 0].copy(), xy[:, 1].copy()

        done = bool(
            np.any(self.xs < self.cfg.x_min * 1.1) or
            np.any(self.xs > self.cfg.x_max * 1.1) or
            np.any(self.ys < self.cfg.y_min * 1.1) or
            np.any(self.ys > self.cfg.y_max * 1.1)
        )

        obs = await self._get_occupancy()
        return obs, self._reward(), done

    async def _get_occupancy(self) -> np.ndarray:
        await self.ws.send("get_occupancy")
        msg = await self.ws.recv()
        m = self.m
        return np.frombuffer(msg, dtype=np.int32).reshape(m, m).astype(np.float32)

    def _reward(self) -> float:
        dists = np.sqrt((self.xs - self.cfg.cx) ** 2 + (self.ys - self.cfg.cy) ** 2)
        return float(-np.mean(dists) - 0.1 * np.std(dists))


# ── Model ─────────────────────────────────────────────────────────────────────

class ActorCritic(nn.Module):
    def __init__(self, m: int, max_force: float):
        super().__init__()
        self.max_force = max_force

        self.backbone = nn.Sequential(
            nn.Conv2d(1, 16, 3, padding=1), nn.ReLU(),
            nn.Conv2d(16, 32, 3, padding=1), nn.ReLU(),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU(),
        )
        # Actor: outputs per-cell force mean for x and y
        self.actor_mean = nn.Conv2d(32, 2, 1)
        # Learnable log-std shared across all grid cells
        self.log_std = nn.Parameter(torch.full((2, 1, 1), -0.5))

        # Critic: pools spatial features → scalar value estimate
        self.critic_pool = nn.AdaptiveAvgPool2d((4, 4))
        self.critic_fc   = nn.Linear(32 * 16, 1)

    def forward(self, obs: torch.Tensor):
        """obs: (B, 1, m, m) → (mean, log_std, value)"""
        feat  = self.backbone(obs)
        mean  = torch.tanh(self.actor_mean(feat)) * self.max_force  # (B, 2, m, m)
        log_std = self.log_std.clamp(-2.0, 2.0).expand_as(mean)
        pooled  = self.critic_pool(feat).flatten(1)
        value   = self.critic_fc(pooled).squeeze(-1)   # (B,)
        return mean, log_std, value

    def act(self, obs: torch.Tensor):
        mean, log_std, value = self(obs)
        dist   = Normal(mean, log_std.exp())
        action = dist.sample()
        log_prob = dist.log_prob(action).sum(dim=(1, 2, 3))
        return action, log_prob, value

    def evaluate(self, obs: torch.Tensor, action: torch.Tensor):
        mean, log_std, value = self(obs)
        dist     = Normal(mean, log_std.exp())
        log_prob = dist.log_prob(action).sum(dim=(1, 2, 3))
        entropy  = dist.entropy().sum(dim=(1, 2, 3))
        return log_prob, entropy, value


# ── PPO helpers ───────────────────────────────────────────────────────────────

def compute_gae(rewards, values, dones, last_value, gamma=0.99, lam=0.95):
    T   = len(rewards)
    adv = np.zeros(T, dtype=np.float32)
    gae = 0.0
    for t in reversed(range(T)):
        next_val      = last_value if t == T - 1 else values[t + 1]
        not_done      = 1.0 - dones[t]
        delta         = rewards[t] + gamma * next_val * not_done - values[t]
        gae           = delta + gamma * lam * not_done * gae
        adv[t]        = gae
    returns = adv + np.array(values, dtype=np.float32)
    return adv, returns


def ppo_update(model, optimizer, obs_buf, act_buf, logp_buf, adv_buf, ret_buf,
               clip_eps, k_epochs, batch_size, entropy_coef, device):
    obs_t  = torch.tensor(obs_buf,  dtype=torch.float32, device=device).unsqueeze(1)
    act_t  = torch.tensor(act_buf,  dtype=torch.float32, device=device)
    logp_t = torch.tensor(logp_buf, dtype=torch.float32, device=device)
    adv_t  = torch.tensor(adv_buf,  dtype=torch.float32, device=device)
    ret_t  = torch.tensor(ret_buf,  dtype=torch.float32, device=device)

    adv_t = (adv_t - adv_t.mean()) / (adv_t.std() + 1e-8)

    T = obs_t.shape[0]
    total_p = total_v = 0.0
    n_upd = 0

    for _ in range(k_epochs):
        for start in range(0, T, batch_size):
            mb = torch.randperm(T, device=device)[start:start + batch_size]
            new_logp, entropy, new_val = model.evaluate(obs_t[mb], act_t[mb])
            ratio  = (new_logp - logp_t[mb]).exp()
            surr   = torch.min(
                ratio * adv_t[mb],
                ratio.clamp(1 - clip_eps, 1 + clip_eps) * adv_t[mb],
            )
            p_loss = -surr.mean()
            v_loss = 0.5 * (new_val - ret_t[mb]).pow(2).mean()
            loss   = p_loss - entropy_coef * entropy.mean() + v_loss

            optimizer.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 0.5)
            optimizer.step()

            total_p += p_loss.item()
            total_v += v_loss.item()
            n_upd   += 1

    return total_p / n_upd, total_v / n_upd


# ── Training loop ─────────────────────────────────────────────────────────────

async def train(args, cfg: SimCfg):
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    print(f"Using device: {device}")
    print(f"force_grid_size: {cfg.force_grid_size}  max_force: {cfg.max_force}")

    m     = cfg.force_grid_size
    model = ActorCritic(m, cfg.max_force).to(device)
    opt   = optim.Adam(model.parameters(), lr=args.lr)

    reward_history = []

    print(f"Connecting to {args.server} ...")
    async with websockets.connect(args.server, max_size=None) as ws:
        env = ParticleEnv(ws, cfg)
        obs = await env.reset()

        ep        = 0
        ep_reward = 0.0
        ep_step   = 0

        obs_buf, act_buf, logp_buf, val_buf, rew_buf, done_buf = [], [], [], [], [], []

        while ep < args.episodes:
            # ── Collect rollout ────────────────────────────────────────────
            while len(obs_buf) < args.rollout_steps and ep < args.episodes:
                obs_t = torch.tensor(obs[None, None], dtype=torch.float32, device=device)
                with torch.no_grad():
                    action, log_prob, value = model.act(obs_t)

                action_np  = action.squeeze(0).cpu().numpy()   # (2, m, m)

                next_obs, reward, done = await env.step(action_np)

                obs_buf.append(obs)
                act_buf.append(action_np)
                logp_buf.append(log_prob.item())
                val_buf.append(value.item())
                rew_buf.append(reward)
                done_buf.append(float(done))

                ep_reward += reward
                ep_step   += 1
                obs        = next_obs

                episode_over = done or ep_step >= args.episode_steps
                if episode_over:
                    mean_dist = float(np.mean(
                        np.sqrt((env.xs - cfg.cx)**2 + (env.ys - cfg.cy)**2)))
                    print(f"Ep {ep+1:4d} | steps {ep_step:3d} | "
                          f"reward {ep_reward:8.3f} | mean_dist {mean_dist:.4f}")
                    reward_history.append(ep_reward)
                    ep_reward = 0.0
                    ep_step   = 0
                    ep       += 1

                    if ep < args.episodes:
                        obs = await env.reset()

                    if ep % 10 == 0:
                        _plot(reward_history)
                    if ep % 50 == 0 and ep > 0:
                        path = f"actor_critic_ep{ep}.pth"
                        torch.save(model.state_dict(), path)
                        print(f"  → checkpoint: {path}")

            if not obs_buf:
                break

            # Bootstrap terminal value
            with torch.no_grad():
                obs_t = torch.tensor(obs[None, None], dtype=torch.float32, device=device)
                _, _, last_val = model(obs_t)
            last_val_np = last_val.item() * (1.0 - done_buf[-1])

            # ── PPO update ─────────────────────────────────────────────────
            adv, ret = compute_gae(rew_buf, val_buf, done_buf, last_val_np,
                                   gamma=args.gamma, lam=args.lam)
            p_loss, v_loss = ppo_update(
                model, opt,
                np.array(obs_buf, dtype=np.float32),
                np.array(act_buf, dtype=np.float32),
                np.array(logp_buf, dtype=np.float32),
                adv, ret,
                clip_eps=args.clip_eps,
                k_epochs=args.k_epochs,
                batch_size=args.batch_size,
                entropy_coef=args.entropy_coef,
                device=device,
            )
            print(f"  [ppo] p_loss={p_loss:.4f}  v_loss={v_loss:.4f}")

            obs_buf.clear(); act_buf.clear(); logp_buf.clear()
            val_buf.clear(); rew_buf.clear(); done_buf.clear()

    _plot(reward_history)
    torch.save(model.state_dict(), "actor_critic_final.pth")
    print("Done. Final checkpoint: actor_critic_final.pth")


def _plot(history):
    if not history:
        return
    plt.figure(figsize=(8, 4))
    plt.plot(history)
    plt.xlabel("Episode")
    plt.ylabel("Total reward")
    plt.title("PPO — particle sphere reward")
    plt.tight_layout()
    plt.savefig("reward_curve.png", dpi=120)
    plt.close()


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="PPO training: particles → sphere")
    ap.add_argument("--config",        default="config.yaml")
    ap.add_argument("--server",        default=None,
                    help="WebSocket URL; defaults to ws://localhost:<port from config>")
    ap.add_argument("--launch-server", action="store_true",
                    help="Start ./build/bin/world <config> as a subprocess")
    ap.add_argument("--episodes",      type=int,   default=1000)
    ap.add_argument("--episode-steps", type=int,   default=200)
    ap.add_argument("--rollout-steps", type=int,   default=256)
    ap.add_argument("--lr",            type=float, default=3e-4)
    ap.add_argument("--k-epochs",      type=int,   default=4)
    ap.add_argument("--batch-size",    type=int,   default=64)
    ap.add_argument("--clip-eps",      type=float, default=0.2)
    ap.add_argument("--entropy-coef",  type=float, default=0.01)
    ap.add_argument("--gamma",         type=float, default=0.99)
    ap.add_argument("--lam",           type=float, default=0.95)
    args = ap.parse_args()

    cfg = SimCfg(args.config)
    if args.server is None:
        args.server = f"ws://localhost:{cfg.port}"

    proc = None
    if args.launch_server:
        cmd = ["./build/bin/world", args.config]
        print(f"Launching: {' '.join(cmd)}")
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.5)

    try:
        asyncio.run(train(args, cfg))
    finally:
        if proc is not None:
            proc.terminate()


if __name__ == "__main__":
    main()
