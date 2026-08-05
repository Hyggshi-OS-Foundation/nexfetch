#!/usr/bin/env python3
"""
Plots a run-by-run line chart comparing Nexfetch and Fastfetch timings,
based on the JSON file exported by hyperfine. Dark-theme style with an
optional logo in the top-right corner.

Usage:
    hyperfine --warmup 5 --min-runs 50 \
        --export-json bench_results.json \
        'nexfetch' 'fastfetch'

    python3 plot_benchmark.py bench_results.json --config "All plugins enabled"
    python3 plot_benchmark.py bench_results.json --config "Default configuration" --logo logo.png
"""

import argparse
import json
import statistics as stats

import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from matplotlib.offsetbox import OffsetImage, AnnotationBbox


def load_results(path):
    with open(path, "r") as f:
        data = json.load(f)
    return data["results"]


def add_logo(fig, logo_path, target_px=16):
    """Places a logo image in the top-right corner of the figure,
    scaled to a fixed pixel size (default 16x16) regardless of the
    source image's native resolution."""
    try:
        img = mpimg.imread(logo_path)
    except Exception as e:
        print(f"Warning: could not load logo '{logo_path}': {e}")
        return

    img_px = max(img.shape[0], img.shape[1])
    # zoom=1.0 renders the image at its native pixel size (at 100 dpi
    # reference); scale down/up so the longest side == target_px.
    zoom = target_px / img_px

    imagebox = OffsetImage(img, zoom=zoom)
    ab = AnnotationBbox(
        imagebox,
        (0.985, 0.985),
        xycoords="figure fraction",
        frameon=False,
        box_alignment=(1, 1),
        pad=0,
    )
    fig.add_artist(ab)


def main():
    parser = argparse.ArgumentParser(
        description="Plot a dark-theme benchmark comparison line chart from a hyperfine JSON export."
    )
    parser.add_argument("json_file", help="JSON file exported by hyperfine")
    parser.add_argument(
        "--config",
        default=None,
        help=(
            "Description of the benchmark configuration, printed on the chart "
            'to avoid misinterpretation (e.g. "All plugins enabled", '
            '"Default configuration"). Always pass this argument.'
        ),
    )
    parser.add_argument(
        "--out",
        default="benchmark_chart.png",
        help="Output image filename (default: benchmark_chart.png)",
    )
    parser.add_argument(
        "--logo",
        default=None,
        help="Path to a logo image (PNG with transparency recommended) to place in the top-right corner.",
    )
    parser.add_argument(
        "--logo-size",
        type=int,
        default=16,
        help="Logo size in pixels, longest side (default: 16).",
    )
    parser.add_argument(
        "--title",
        default="Nexfetch vs Fastfetch — Per-run execution time",
        help="Chart title (default: 'Nexfetch vs Fastfetch — Per-run execution time')",
    )
    args = parser.parse_args()

    results = load_results(args.json_file)

    # ---- Dark theme ----
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

    fig, ax = plt.subplots(figsize=(11, 6.5))

    # Soft pastel palette similar to the reference style
    colors = ["#a5c4f7", "#4f7fd6", "#f5b79a", "#d9622b", "#9fe08a", "#c9a5f7"]

    for i, r in enumerate(results):
        cmd = r["command"]
        times_ms = [t * 1000 for t in r["times"]]  # seconds -> ms
        n = len(times_ms)
        mean = stats.mean(times_ms)
        median = stats.median(times_ms)

        color = colors[i % len(colors)]
        ax.plot(
            range(1, n + 1),
            times_ms,
            marker="o",
            markersize=6,
            linewidth=2,
            alpha=0.95,
            color=color,
            label=f"{cmd}  (mean={mean:.2f}ms, median={median:.2f}ms, n={n})",
        )

    ax.set_xlabel("Run #")
    ax.set_ylabel("Time (ms)")

    if args.config:
        subtitle = f"Configuration: {args.config}"
        subtitle_color = "#a5a5a5"
    else:
        subtitle = "⚠ Configuration: UNSPECIFIED — do not use for general comparison"
        subtitle_color = "#e05c5c"

    fig.text(0.06, 0.955, args.title, fontsize=16, fontweight="bold", color="#ffffff")
    fig.text(0.06, 0.925, subtitle, fontsize=10.5, color=subtitle_color)

    ax.legend(
        loc="upper center",
        bbox_to_anchor=(0.5, 1.12),
        ncol=min(len(results), 3),
        frameon=False,
        fontsize=9.5,
    )
    ax.grid(True, alpha=0.15)
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)

    if args.logo:
        add_logo(fig, args.logo, target_px=args.logo_size)

    fig.tight_layout(rect=[0, 0, 1, 0.87])
    fig.savefig(args.out, dpi=150)
    print(f"Chart saved: {args.out}")

    # Print summary table
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