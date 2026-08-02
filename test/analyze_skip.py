"""
analyze_skip.py - Compare ECG data loss across firmware parameter settings.

Usage (one or more serial captures):
    python analyze_skip.py run_100ms.txt run_200ms.txt run_500ms.txt

For each capture it reports the connection outages and the amount of ECG data
lost, and prints a comparison table at the end.

Runs are rarely the same length, so the headline metric is loss PER MINUTE
rather than the raw counter. It also measures the actual publish period from
the log timestamps, which verifies that a parameter change really took effect
in the flashed firmware rather than only in the source.
"""
import sys
import re
import os

SAMPLE_RATE_HZ = 360.0

re_ts     = re.compile(r'^\w \((\d+)\)')
re_ecg    = re.compile(r'ECG:\s*(\d+) sampled,\s*(\d+) sent')
re_integ  = re.compile(r'dropped=(\d+).*?skipped=(\d+)')


def analyse(path):
    ts_all, ecg_ts, sampled, sent = [], [], [], []
    pubacks = 0
    downs, ups = [], []
    skipped = dropped = 0

    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            s = line.strip()
            m_ts = re_ts.match(s)
            if not m_ts:
                continue
            ts = int(m_ts.group(1))
            ts_all.append(ts)

            m = re_ecg.search(s)
            if m:
                ecg_ts.append(ts)
                sampled.append(int(m.group(1)))
                sent.append(int(m.group(2)))

            m = re_integ.search(s)
            if m:
                dropped = int(m.group(1))     # counters are cumulative,
                skipped = int(m.group(2))     # so the last one wins

            if 'PUBACK received' in s:
                pubacks += 1
            if 'MQTT_EVENT_DISCONNECTED' in s:
                downs.append(ts)
            elif 'MQTT_EVENT_CONNECTED' in s:
                ups.append(ts)

    if not ts_all:
        return None

    dur_s = (max(ts_all) - min(ts_all)) / 1000.0

    # measured publish period: median gap between consecutive ECG log lines
    gaps = sorted(b - a for a, b in zip(ecg_ts, ecg_ts[1:]) if 0 < b - a < 5000)
    period_ms = gaps[len(gaps)//2] if gaps else 0

    # total outage: pair each disconnect with the next reconnect
    outage_ms = 0
    for d in downs:
        later = [u for u in ups if u > d]
        if later:
            outage_ms += later[0] - d

    med_sent = sorted(sent)[len(sent)//2] if sent else 0

    return {
        'file':      os.path.basename(path),
        'dur_s':     dur_s,
        'period_ms': period_ms,
        'med_sent':  med_sent,
        'puback_s':  pubacks / dur_s if dur_s else 0,
        'downs':     len(downs),
        'outage_s':  outage_ms / 1000.0,
        'outage_pc': 100 * outage_ms / (dur_s * 1000) if dur_s else 0,
        'skipped':   skipped,
        'dropped':   dropped,
        'lost_s':    (skipped + dropped) / SAMPLE_RATE_HZ,
        'lost_pm':   (skipped + dropped) / SAMPLE_RATE_HZ / (dur_s/60) if dur_s else 0,
    }


def report(r):
    print(f"\n=== {r['file']} ===")
    print(f"  run duration        : {r['dur_s']:8.1f} s")
    print(f"  measured period     : {r['period_ms']:8d} ms   <- verifies the flash took")
    print(f"  points per publish  : {r['med_sent']:8d}")
    print(f"  PUBACK rate         : {r['puback_s']:8.1f} /s   <- message rate proxy")
    print(f"  disconnects         : {r['downs']:8d}")
    print(f"  total outage        : {r['outage_s']:8.1f} s  ({r['outage_pc']:.1f} % of run)")
    print(f"  skipped (no network): {r['skipped']:8d}")
    print(f"  dropped (queue full): {r['dropped']:8d}")
    print(f"  ECG lost            : {r['lost_s']:8.1f} s  ({r['lost_pm']:.1f} s lost per minute)")


def main():
    if len(sys.argv) < 2:
        print("usage: python analyze_skip.py <capture1.txt> [capture2.txt ...]")
        sys.exit(1)

    results = []
    for p in sys.argv[1:]:
        if not os.path.exists(p):
            print(f"skipping {p}: not found")
            continue
        r = analyse(p)
        if r is None:
            print(f"skipping {p}: no timestamped log lines found")
            continue
        results.append(r)
        report(r)

    if len(results) > 1:
        print("\n" + "=" * 78)
        print("COMPARISON  (lower loss/min is better)")
        print("=" * 78)
        print(f"{'file':<22}{'period':>8}{'PUBACK/s':>10}{'drops':>7}"
              f"{'outage%':>9}{'lost s/min':>12}")
        print("-" * 78)
        for r in results:
            print(f"{r['file']:<22}{r['period_ms']:>6} ms{r['puback_s']:>10.1f}"
                  f"{r['downs']:>7}{r['outage_pc']:>8.1f}%{r['lost_pm']:>12.1f}")
        print("-" * 78)
        best = min(results, key=lambda r: r['lost_pm'])
        print(f"lowest loss: {best['file']}  "
              f"({best['lost_pm']:.1f} s/min at {best['period_ms']} ms period)")


if __name__ == '__main__':
    main()