#!/usr/bin/env python3
"""
Plots benchmark comparison charts (Nexfetch vs Fastfetch, etc.) from a
hyperfine JSON export. Dark-theme style with 5 selectable chart styles
and an optional logo in the top-right corner.

Usage:
    hyperfine --warmup 10 --runs 500 \
        --export-json bench_results.json \
        './nexfetch --fast' 'fastfetch' './nexfetch-ghvbb'

    # Style 1: per-run line chart (raw timings, one point per run)
    python3 plot_benchmark.py bench_results.json --style line \
        --config "Default configuration" --logo ./logos/logo.png --logo-size 48

    # Style 2: bar chart of mean/median (like a leaderboard)
    python3 plot_benchmark.py bench_results.json --style bar --metric mean \
        --config "Default configuration" --logo ./logos/logo.png --logo-size 48

    # Style 3: horizontal bar chart (leaderboard, sorted)
    python3 plot_benchmark.py bench_results.json --style hbar --metric median \
        --config "Default configuration"

    # Style 4: multi-line chart with inline labels (no legend box)
    python3 plot_benchmark.py bench_results.json --style multiline \
        --config "Default configuration"

    # Style 5: box plot (shows distribution / spread of timings)
    python3 plot_benchmark.py bench_results.json --style box \
        --config "Default configuration"
"""

import argparse
import json
import statistics as stats

import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from matplotlib.offsetbox import OffsetImage, AnnotationBbox

PALETTE = ["#e0714f", "#2fb37c", "#7c5cf0", "#4f9be8", "#f0c14f", "#e85c8a"]


def load_results(path):
    with open(path, "r") as f:
        data = json.load(f)
    return data["results"]


def apply_dark_theme():
    plt.rcParams.update({
        "figure.facecolor": "#0a0a0a",
        "axes.facecolor": "#0a0a0a",
        "savefig.facecolor": "#0a0a0a",
        "text.color": "#f5f5f5",
        "axes.edgecolor": "#3a3a3a",
        "axes.labelcolor": "#e5e5e5",
        "xtick.color": "#b5b5b5",
        "ytick.color": "#b5b5b5",
        "font.size": 11,
    })


def add_logo(fig, logo_path, target_px=16):
    try:
        img = mpimg.imread(logo_path)
    except Exception as e:
        print(f"Warning: could not load logo '{logo_path}': {e}")
        return
    img_px = max(img.shape[0], img.shape[1])
    zoom = target_px / img_px
    imagebox = OffsetImage(img, zoom=zoom)
    ab = AnnotationBbox(
        imagebox, (0.985, 0.985), xycoords="figure fraction",
        frameon=False, box_alignment=(1, 1), pad=0,
    )
    fig.add_artist(ab)


def draw_header(fig, title, config):
    if config:
        subtitle, subtitle_color = f"Configuration: {config}", "#a5a5a5"
    else:
        subtitle = "⚠ Configuration: UNSPECIFIED — do not use for general comparison"
        subtitle_color = "#e05c5c"
    fig.text(0.06, 0.955, title, fontsize=16, fontweight="bold", color="#ffffff")
    fig.text(0.06, 0.918, subtitle, fontsize=10.5, color=subtitle_color)


def get_metric(times_ms, metric):
    return stats.mean(times_ms) if metric == "mean" else stats.median(times_ms)


# ---------------------------------------------------------------- styles ---

def style_line(fig, ax, results):
    """Style 1: raw per-run timings, one line per command, legend on top."""
    for i, r in enumerate(results):
        times_ms = [t * 1000 for t in r["times"]]
        n = len(times_ms)
        mean, median = stats.mean(times_ms), stats.median(times_ms)
        color = PALETTE[i % len(PALETTE)]
        ax.plot(range(1, n + 1), times_ms, marker="o", markersize=6,
                linewidth=2, alpha=0.95, color=color,
                label=f"{r['command']}  (mean={mean:.2f}ms, median={median:.2f}ms, n={n})")
    ax.set_xlabel("Run #")
    ax.set_ylabel("Time (ms)")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, 1.12),
              ncol=2 if len(results) > 2 else len(results),
              frameon=False, fontsize=9.5, columnspacing=1.5)
    ax.grid(True, alpha=0.15)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)


def style_bar(fig, ax, results, metric="mean"):
    """Style 2: vertical bar chart, like a leaderboard (image 1 reference)."""
    names, values, colors = [], [], []
    for i, r in enumerate(results):
        times_ms = [t * 1000 for t in r["times"]]
        names.append(r["command"])
        values.append(get_metric(times_ms, metric))
        colors.append(PALETTE[i % len(PALETTE)])

    order = sorted(range(len(values)), key=lambda i: values[i], reverse=True)
    names = [names[i] for i in order]
    values = [values[i] for i in order]
    colors = [colors[i] for i in order]

    bars = ax.bar(names, values, color=colors, width=0.6, zorder=3)
    for b, v in zip(bars, values):
        ax.text(b.get_x() + b.get_width() / 2, v + max(values) * 0.02,
                f"{v:.2f}ms", ha="center", va="bottom", fontsize=10,
                fontweight="bold", color="#ffffff")
    ax.set_ylabel(f"Time ({metric}, ms)")
    ax.grid(True, axis="y", alpha=0.15, zorder=0)
    ax.set_ylim(0, max(values) * 1.2)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    plt.setp(ax.get_xticklabels(), rotation=15, ha="right")


def style_hbar(fig, ax, results, metric="mean"):
    """Style 3: horizontal bar chart leaderboard, sorted ascending (fastest on top)."""
    names, values, colors = [], [], []
    for i, r in enumerate(results):
        times_ms = [t * 1000 for t in r["times"]]
        names.append(r["command"])
        values.append(get_metric(times_ms, metric))
        colors.append(PALETTE[i % len(PALETTE)])

    order = sorted(range(len(values)), key=lambda i: values[i])
    names = [names[i] for i in order]
    values = [values[i] for i in order]
    colors = [colors[i] for i in order]

    bars = ax.barh(names, values, color=colors, height=0.55, zorder=3)
    for b, v in zip(bars, values):
        ax.text(v + max(values) * 0.015, b.get_y() + b.get_height() / 2,
                f"{v:.2f}ms", va="center", fontsize=10, fontweight="bold",
                color="#ffffff")
    ax.set_xlabel(f"Time ({metric}, ms)")
    ax.grid(True, axis="x", alpha=0.15, zorder=0)
    ax.set_xlim(0, max(values) * 1.2)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)


def style_multiline(fig, ax, results):
    """Style 4: multi-line chart with inline end-of-line labels, no legend box
    (image 2 reference)."""
    for i, r in enumerate(results):
        times_ms = [t * 1000 for t in r["times"]]
        n = len(times_ms)
        color = PALETTE[i % len(PALETTE)]
        ax.plot(range(1, n + 1), times_ms, linewidth=1.6, alpha=0.9, color=color)
        ax.annotate(r["command"], xy=(n, times_ms[-1]), xytext=(6, 0),
                    textcoords="offset points", va="center", fontsize=9,
                    fontweight="bold", color=color)
    ax.set_xlabel("Run #")
    ax.set_ylabel("Time (ms)")
    ax.grid(True, alpha=0.15)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)


def style_box(fig, ax, results):
    """Style 5: box plot showing distribution / spread of run times."""
    data = [[t * 1000 for t in r["times"]] for r in results]
    labels = [r["command"] for r in results]
    bp = ax.boxplot(data, tick_labels=labels, patch_artist=True, widths=0.5,
                     medianprops=dict(color="#ffffff", linewidth=1.5),
                     whiskerprops=dict(color="#b5b5b5"),
                     capprops=dict(color="#b5b5b5"),
                     flierprops=dict(markeredgecolor="#888888", markersize=3))
    for i, box in enumerate(bp["boxes"]):
        box.set_facecolor(PALETTE[i % len(PALETTE)])
        box.set_alpha(0.85)
        box.set_edgecolor("none")
    ax.set_ylabel("Time (ms)")
    ax.grid(True, axis="y", alpha=0.15)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    plt.setp(ax.get_xticklabels(), rotation=15, ha="right")


STYLES = {
    "line": style_line,
    "bar": style_bar,
    "hbar": style_hbar,
    "multiline": style_multiline,
    "box": style_box,
}


# ------------------------------------------------------------------- main --

def main():
    parser = argparse.ArgumentParser(
        description="Plot a dark-theme benchmark chart (5 styles) from a hyperfine JSON export."
    )
    parser.add_argument("json_file", help="JSON file exported by hyperfine")
    parser.add_argument("--style", choices=STYLES.keys(), default="line",
                         help="Chart style: line, bar, hbar, multiline, box (default: line)")
    parser.add_argument("--metric", choices=["mean", "median"], default="mean",
                         help="Metric used for bar/hbar styles (default: mean)")
    parser.add_argument("--config", default=None,
                         help='Benchmark configuration label, e.g. "All plugins enabled".')
    parser.add_argument("--out", default=None,
                         help="Output filename (default: benchmark_<style>.png)")
    parser.add_argument("--logo", default=None, help="Path to a logo image.")
    parser.add_argument("--logo-size", type=int, default=16,
                         help="Logo size in pixels, longest side (default: 16).")
    parser.add_argument("--title", default="Nexfetch vs Fastfetch — Benchmark",
                         help="Chart title.")
    args = parser.parse_args()

    results = load_results(args.json_file)

    apply_dark_theme()
    fig, ax = plt.subplots(figsize=(12.8, 7.2))  # 16:9 -> 1920x1080 @150dpi

    draw_fn = STYLES[args.style]
    if args.style in ("bar", "hbar"):
        draw_fn(fig, ax, results, metric=args.metric)
    else:
        draw_fn(fig, ax, results)

    draw_header(fig, args.title, args.config)
    if args.logo:
        add_logo(fig, args.logo, target_px=args.logo_size)

    fig.tight_layout(rect=[0, 0, 1, 0.87])
    out = args.out or f"benchmark_{args.style}.png"
    fig.savefig(out, dpi=150)
    print(f"Chart saved: {out}")

    print("\n--- Summary ---")
    for r in results:
        times_ms = [t * 1000 for t in r["times"]]
        print(
            f"{r['command']:20s} mean={stats.mean(times_ms):8.2f}ms  "
            f"median={stats.median(times_ms):8.2f}ms  "
            f"stdev={stats.pstdev(times_ms):7.2f}ms  "
            f"min={min(times_ms):7.2f}ms  max={max(times_ms):7.2f}ms"
        )


if __name__ == "__main__":
    main()