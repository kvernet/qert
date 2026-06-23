# Paper figures only.
#
# Four figures:
#   1. Entropy growth curves (N=8-18 grid, all mappings)
#   2. Alpha vs N summary
#   3. Observable vs entropy (MPS χ left, statevector L3 right)
#   4. Cache miss curves vs depth (N=8-18 grid)

import math
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def generate_paper_figures(
    statevector_runs: list[dict] | None,
    mps_runs: list[dict] | None,
    entropy_results: list[dict] | None,
    cache_results: list[dict] | None,
    output_dir: str | None = None,
) -> None:
    """Generate all figures for the paper."""
    path = Path(output_dir) if output_dir else None
    if path:
        path.mkdir(parents=True, exist_ok=True)

    if entropy_results:
        _fig_entropy_curves(entropy_results,
                            str(path / "fig1_entropy.png") if path else None)
        _fig_alpha_summary(entropy_results,
                           str(path / "fig2_alpha.png") if path else None)

    if cache_results:
        _fig_cache_curves(cache_results,
                          str(path / "fig4_cache.png") if path else None)

    if statevector_runs and mps_runs:
        _fig_observable_vs_entropy(
            statevector_runs, mps_runs,
            str(path / "fig3_observable_vs_entropy.png") if path else None)

    if not path:
        plt.show()


# ============================================================================
# Figure 1: Entropy growth curves
# ============================================================================

def _fig_entropy_curves(results: list[dict], output_path: str | None = None) -> None:
    """Grid of entropy vs depth panels, one per N."""
    by_n = {}
    for r in results:
        by_n.setdefault(r["num_qubits"], []).append(r)

    n_list = sorted(by_n.keys())
    ncols = (len(n_list) + 1) // 2
    nrows = 2 if len(n_list) > 1 else 1

    fig, axes = plt.subplots(nrows, ncols, figsize=(4.5 * ncols, 4.0 * nrows),
                             squeeze=False)
    colors = {"lexicographic": "#1f77b4", "gray": "#ff7f0e",
              "locality_aware": "#2ca02c"}

    for idx, n in enumerate(n_list):
        ax = axes.flatten()[idx]
        for r in by_n[n]:
            ax.errorbar(r["depths"], r["mean_entropy_curve"],
                        yerr=r["std_entropy_curve"],
                        marker="o", markersize=3, capsize=2, linewidth=1.0,
                        color=colors.get(r["mapping"], "#333"),
                        label=fr"{r['mapping']} ($\alpha$={r['alpha_d90']:.3f})",
                        alpha=0.85)

        ax.axvline(x=n / 2, color="red", linestyle="--", alpha=0.5, linewidth=0.8)
        ax.axhline(y=(n // 2) * math.log(2), color="gray", linestyle=":",
                   alpha=0.4, linewidth=0.8)
        ax.set_xlabel("Circuit depth", fontsize=9)
        ax.set_ylabel("Half-chain entropy (nats)", fontsize=9)
        ax.set_title(f"N = {n}", fontsize=10, fontweight="bold")
        ax.legend(fontsize=6, loc="lower right")
        ax.grid(True, alpha=0.3)
        ax.set_xlim(left=0)

    for idx in range(len(n_list), len(axes.flatten())):
        axes.flatten()[idx].set_visible(False)

    num_seeds = results[0]["num_seeds"]
    fig.text(0.5, 0.01,
             f"Mean entropy across {num_seeds} instances. "
             r"Error bars: $\pm 1\sigma$. "
             "Red dashed: Page time (N/2). Gray dotted: max entropy k·ln(2).",
             ha="center", fontsize=9, fontstyle="italic")

    fig.suptitle("Entanglement entropy growth in 1D brickwall circuits",
                 fontsize=14, fontweight="bold", y=1.01)
    plt.tight_layout(rect=[0, 0.04, 1, 0.96])
    _save_or_show(fig, output_path)


# ============================================================================
# Figure 2: Alpha vs N
# ============================================================================

def _fig_alpha_summary(results: list[dict], output_path: str | None = None) -> None:
    """Scaling coefficient α vs system size N (lexicographic only)."""
    by_n = {}
    for r in results:
        if r["mapping"] == "lexicographic":
            by_n[r["num_qubits"]] = r["alpha_d90"]

    n_list = sorted(by_n.keys())
    alphas = [by_n[n] for n in n_list]
    mean_a, std_a = np.mean(alphas), np.std(alphas)

    fig, ax = plt.subplots(figsize=(8, 4.5))
    ax.plot(n_list, alphas, marker="o", markersize=8, linewidth=1.5,
            color="#1f77b4",
            label=fr"Measured $\alpha$ (mean = {mean_a:.3f} $\pm$ {std_a:.3f})")
    ax.axhline(y=1.0, color="red", linestyle="--", linewidth=1.2, alpha=0.7,
               label=r"Page prediction ($\alpha$ = 1.0)")
    ax.axhspan(mean_a - std_a, mean_a + std_a, alpha=0.12, color="#1f77b4",
               label=fr"Mean $\alpha$ band ($\pm 1\sigma$)")

    ax.set_xlabel("System size N", fontsize=12)
    ax.set_ylabel(r"Scaling coefficient $\alpha = D_{90} / (N/2)$", fontsize=12)
    ax.set_title("Entanglement saturation scaling coefficient",
                 fontsize=13, fontweight="bold")
    ax.legend(fontsize=10, loc="lower right")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(n_list[0] - 0.5, n_list[-1] + 0.5)
    plt.tight_layout()
    _save_or_show(fig, output_path)


# ============================================================================
# Figure 3: Observable vs entropy (MPS χ vs statevector L3)
# ============================================================================

def _fig_observable_vs_entropy(
    statevector_runs: list[dict],
    mps_runs: list[dict],
    output_path: str | None = None,
) -> None:
    """Side-by-side: MPS χ vs entropy (left), statevector L3 vs entropy (right)."""
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    all_n = sorted(set(
        r["metadata"]["num_qubits"] for r in
        (list(mps_runs) + list(statevector_runs))
    ))
    cmap = plt.cm.viridis
    colors = {n: cmap(i / max(len(all_n) - 1, 1)) for i, n in enumerate(all_n)}
    n_bins = 15

    # --- Left: MPS χ_max vs entropy ---
    ax = axes[0]
    by_n = {}
    for r in mps_runs:
        n = r["metadata"]["num_qubits"]
        by_n.setdefault(n, {"ent": [], "chi": []})
        d = r["data"]
        for chi, ent in zip(d["chi_max"], d["entropy"]):
            by_n[n]["ent"].append(ent)
            by_n[n]["chi"].append(chi)

    for n in sorted(by_n):
        ent_arr = np.array(by_n[n]["ent"])
        chi_arr = np.array(by_n[n]["chi"])
        bins = np.linspace(0, ent_arr.max() * 1.05, n_bins + 1)
        centers = (bins[:-1] + bins[1:]) / 2
        means, stds = np.full(n_bins, np.nan), np.full(n_bins, np.nan)
        for i in range(n_bins):
            mask = (ent_arr >= bins[i]) & (ent_arr < bins[i + 1])
            if mask.sum() > 10:
                means[i] = np.mean(chi_arr[mask])
                stds[i] = np.std(chi_arr[mask])
        valid = ~np.isnan(means)
        if valid.sum() > 0:
            ax.plot(centers[valid], means[valid], color=colors[n],
                    linewidth=1.2, label=f"N={n}")
            ax.fill_between(centers[valid], means[valid] - stds[valid],
                            means[valid] + stds[valid], color=colors[n], alpha=0.1)

    ax.set_xlabel("Half-chain entropy (nats)", fontsize=10)
    ax.set_ylabel("Max bond dimension χ", fontsize=10)
    ax.set_title("MPS: χ vs entropy", fontsize=11, fontweight="bold")
    ax.legend(fontsize=7, ncol=2)
    ax.grid(True, alpha=0.3)

    # --- Right: Statevector L3 misses vs entropy ---
    ax = axes[1]
    by_n = {}
    for r in statevector_runs:
        n = r["metadata"]["num_qubits"]
        by_n.setdefault(n, {"ent": [], "l3": []})
        ent_by_depth = {}
        for row in r["data"]:
            depth = int(row["depth"])
            ent = row["half_chain_entropy"]
            if not np.isnan(ent):
                ent_by_depth[depth] = float(ent)
        l3_by_depth = {}
        for row in r["data"]:
            depth = int(row["depth"])
            l3_by_depth.setdefault(depth, []).append(row["l3_misses_delta"])
        for depth in ent_by_depth:
            if depth in l3_by_depth:
                by_n[n]["ent"].append(ent_by_depth[depth])
                by_n[n]["l3"].append(np.mean(l3_by_depth[depth]))

    for n in sorted(by_n):
        ent_arr = np.array(by_n[n]["ent"])
        l3_arr = np.array(by_n[n]["l3"])
        bins = np.linspace(0, ent_arr.max() * 1.05, n_bins + 1)
        centers = (bins[:-1] + bins[1:]) / 2
        means, stds = np.full(n_bins, np.nan), np.full(n_bins, np.nan)
        for i in range(n_bins):
            mask = (ent_arr >= bins[i]) & (ent_arr < bins[i + 1])
            if mask.sum() > 10:
                means[i] = np.mean(l3_arr[mask])
                stds[i] = np.std(l3_arr[mask])
        valid = ~np.isnan(means)
        if valid.sum() > 0:
            ax.plot(centers[valid], means[valid], color=colors[n],
                    linewidth=1.2, label=f"N={n}")
            ax.fill_between(centers[valid], means[valid] - stds[valid],
                            means[valid] + stds[valid], color=colors[n], alpha=0.1)

    ax.set_xlabel("Half-chain entropy (nats)", fontsize=10)
    ax.set_ylabel("Mean L3 misses / gate", fontsize=10)
    ax.set_title("Statevector: L3 misses vs entropy", fontsize=11, fontweight="bold")
    ax.legend(fontsize=7, ncol=2)
    ax.grid(True, alpha=0.3)

    fig.suptitle("Same entropy, different observables",
                 fontsize=13, fontweight="bold", y=1.02)
    plt.tight_layout()
    _save_or_show(fig, output_path)


# ============================================================================
# Figure 4: Cache miss curves vs depth
# ============================================================================

def _fig_cache_curves(results: list[dict], output_path: str | None = None) -> None:
    """Grid of L3 cache miss vs depth panels, one per N."""
    by_n = {}
    for r in results:
        by_n.setdefault(r["num_qubits"], []).append(r)

    n_list = sorted(by_n.keys())
    ncols = (len(n_list) + 1) // 2
    nrows = 2 if len(n_list) > 1 else 1

    fig, axes = plt.subplots(nrows, ncols, figsize=(4.5 * ncols, 4.0 * nrows),
                             squeeze=False)
    colors = {"lexicographic": "#1f77b4", "gray": "#ff7f0e",
              "locality_aware": "#2ca02c"}

    for idx, n in enumerate(n_list):
        ax = axes.flatten()[idx]
        for r in by_n[n]:
            ax.errorbar(r["depths"], r["mean_l3_curve"],
                        yerr=r["std_l3_curve"],
                        marker="o", markersize=3, capsize=2, linewidth=1.0,
                        color=colors.get(r["mapping"], "#333"),
                        label=fr"{r['mapping']} ($\alpha$={r['alpha_cache']:.3f})",
                        alpha=0.85)
        ax.axvline(x=n / 2, color="red", linestyle="--", alpha=0.5, linewidth=0.8)
        ax.set_xlabel("Circuit depth", fontsize=9)
        ax.set_ylabel("L3 misses / gate", fontsize=9)
        ax.set_title(f"N = {n}", fontsize=10, fontweight="bold")
        ax.legend(fontsize=6, loc="upper right")
        ax.grid(True, alpha=0.3)

    for idx in range(len(n_list), len(axes.flatten())):
        axes.flatten()[idx].set_visible(False)

    fig.suptitle("L3 Cache Misses vs. Circuit Depth",
                 fontsize=14, fontweight="bold", y=1.01)
    plt.tight_layout(rect=[0, 0.04, 1, 0.96])
    _save_or_show(fig, output_path)


# ============================================================================
# Helpers
# ============================================================================

def _save_or_show(fig: plt.Figure, output_path: str | None) -> None:
    """Save figure to file or display it."""
    if output_path:
        fig.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Saved: {output_path}")
        plt.close(fig)