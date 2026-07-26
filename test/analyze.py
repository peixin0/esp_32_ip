"""
analyze.py - Batch analysis of ECG sampling jitter across multiple runs.

Compares two experimental groups, each measured several times, and reports
whether the difference is repeatable (not a one-off fluke).

Usage (glob patterns - quote them on Windows PowerShell):
    python analyze.py "jitter_base_*.txt" "jitter_iso_*.txt"

Or list files explicitly:
    python analyze.py --base jitter_base_1.txt jitter_base_2.txt --iso jitter_iso_1.txt jitter_iso_2.txt

It prints, for each run, the per-run std / mean|dev| / max, then the
across-run mean +/- std for each group, and saves:
    jitter_boxplot.png   - box plot of per-run std for both groups. If the two
                           boxes do not overlap, the effect is repeatable.
"""
import sys
import glob
import numpy as np
import matplotlib.pyplot as plt

IDEAL_US    = 2778.0   # 360 Hz ideal period (use 5556.0 for 180 Hz)
WARMUP_SKIP = 100      # drop first N samples (boot transient) from each run


def load_periods(path, skip=0):
    periods = []
    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            s = line.strip()
            if s.lstrip('-').isdigit():
                periods.append(int(s))
    arr = np.array(periods, dtype=float)
    if skip > 0 and len(arr) > skip:
        arr = arr[skip:]
    return arr


def run_metrics(p):
    """Return the key metrics for a single run."""
    dev = np.abs(p - IDEAL_US)
    late = int((dev > IDEAL_US).sum())
    return {
        'n':      len(p),
        'std':    p.std(),
        'meandev': dev.mean(),
        'max':    p.max(),
        'p99':    np.percentile(p, 99),
        'misses': late,
    }


def parse_args(argv):
    """Support either two glob patterns, or --base ... --iso ... lists."""
    if '--base' in argv and '--iso' in argv:
        bi, ii = argv.index('--base'), argv.index('--iso')
        if bi < ii:
            base_files = argv[bi+1:ii]
            iso_files  = argv[ii+1:]
        else:
            iso_files  = argv[ii+1:bi]
            base_files = argv[bi+1:]
    else:
        if len(argv) < 2:
            print('usage: python analyze.py "jitter_base_*.txt" "jitter_iso_*.txt"')
            sys.exit(1)
        base_files = sorted(glob.glob(argv[0]))
        iso_files  = sorted(glob.glob(argv[1]))
    return base_files, iso_files


def summarise(group_name, files):
    if not files:
        print(f"ERROR: no files found for {group_name}")
        sys.exit(1)
    print(f"\n########## {group_name} ({len(files)} runs) ##########")
    stds, meandevs, maxes, misses = [], [], [], []
    for f in files:
        p = load_periods(f, WARMUP_SKIP)
        if len(p) == 0:
            print(f"  {f}: no numeric data, skipped")
            continue
        m = run_metrics(p)
        stds.append(m['std']); meandevs.append(m['meandev'])
        maxes.append(m['max']); misses.append(m['misses'])
        print(f"  {f:28s} n={m['n']:4d}  std={m['std']:7.1f}  "
              f"mean|dev|={m['meandev']:6.1f}  max={m['max']:8.1f}  misses={m['misses']}")
    stds = np.array(stds)
    print(f"  ---- across {len(stds)} runs ----")
    print(f"  std      : {stds.mean():7.1f} +/- {stds.std():5.1f} us")
    print(f"  mean|dev|: {np.mean(meandevs):7.1f} +/- {np.std(meandevs):5.1f} us")
    print(f"  max      : {np.mean(maxes):7.1f} +/- {np.std(maxes):5.1f} us  (worst-case, noisy)")
    print(f"  misses   : {np.mean(misses):7.2f} +/- {np.std(misses):5.2f}  per run")
    return stds


def main():
    base_files, iso_files = parse_args(sys.argv[1:])
    std_base = summarise("BASELINE (unpinned)", base_files)
    std_iso  = summarise("ISOLATED (pinned core 1)", iso_files)

    print("\n########## overall comparison ##########")
    imp = 100 * (std_base.mean() - std_iso.mean()) / std_base.mean()
    print(f"  baseline std: {std_base.mean():.1f} +/- {std_base.std():.1f} us")
    print(f"  isolated std: {std_iso.mean():.1f} +/- {std_iso.std():.1f} us")
    print(f"  mean improvement: {imp:.1f}%")
    # do the ranges overlap? (simple, honest repeatability check)
    b_lo = std_base.mean() - std_base.std()
    i_hi = std_iso.mean() + std_iso.std()
    if i_hi < b_lo:
        print("  -> ranges (mean +/- 1 std) do NOT overlap: effect is repeatable.")
    else:
        print("  -> ranges overlap; collect more runs before claiming an effect.")

    # ---------- box plot of per-run std ----------
    fig, ax = plt.subplots(figsize=(6, 5))
    bp = ax.boxplot([std_base, std_iso], tick_labels=['Baseline\n(unpinned)', 'Isolated\n(core 1)'],
                    patch_artist=True, widths=0.5)
    for patch, c in zip(bp['boxes'], ['#c0392b', '#2a6fb0']):
        patch.set_facecolor(c); patch.set_alpha(0.7)
    # overlay individual run points (jittered) so reviewers see the raw runs
    for i, data in enumerate([std_base, std_iso], start=1):
        x = np.random.normal(i, 0.04, size=len(data))
        ax.scatter(x, data, color='k', zorder=3, s=25)
    ax.set_ylabel('per-run jitter std dev (us)')
    ax.set_title('Sampling jitter across repeated runs\n(lower = more stable; non-overlap = repeatable)')
    ax.grid(True, axis='y', alpha=0.3)
    fig.tight_layout(); fig.savefig('jitter_boxplot.png', dpi=150)
    print("\nsaved -> jitter_boxplot.png")


if __name__ == '__main__':
    main()