"""
analyze_latency.py - Analyse the internal-latency and RTT dumps, and the
broker disconnection events recorded in the same serial capture.

Usage:
    python analyze_latency.py latency_run.txt

Pass the raw serial capture unedited: the script locates the two dump
sections by their markers and ignores interleaved log lines.

Latency distributions are long-tailed, so medians and high percentiles are
reported rather than means. Samples above the stall threshold are summarised
separately, because they describe broker back-pressure rather than normal
operating latency and would otherwise dominate every statistic.

IMPORTANT: the two thresholds below must exceed the normal operating value
of their metric. Normal internal latency tracks ECG_CYCLE_MS, so raise
STALL_INTERNAL_US whenever that period is increased.
"""
import sys
import re
import numpy as np
import matplotlib.pyplot as plt

# --- thresholds: a sample above these is a stall, not ordinary jitter ---
STALL_INTERNAL_US = 3_000_000   # normal internal latency ~= ECG_CYCLE_MS
STALL_RTT_US      =   500_000   # normal RTT ~= 40 ms

# a stall burst may contain a few normal replies; tolerate this many
# consecutive normal samples before declaring the burst over
BURST_GAP_TOLERANCE = 3


# ----------------------------------------------------------------- parsing
def parse_capture(path):
    """Return (internal, rtt, events, skipped) from a raw serial capture."""
    internal, rtt, current = [], [], None
    events   = []      # (timestamp_ms, 'up'|'down')
    skipped  = []      # (timestamp_ms, count)
    dropped  = []

    re_ts   = re.compile(r'^\w \((\d+)\)')
    re_skip = re.compile(r'dropped=(\d+).*?skipped=(\d+)')

    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            s = line.strip()

            if 'INTERNAL LATENCY DUMP START' in s:
                current = internal; continue
            if 'RTT DUMP START' in s:
                current = rtt; continue
            if 'DUMP END' in s:
                current = None; continue

            if current is not None and s.isdigit():
                current.append(int(s))
                continue

            m_ts = re_ts.match(s)
            ts = int(m_ts.group(1)) if m_ts else None

            if 'MQTT_EVENT_DISCONNECTED' in s and ts is not None:
                events.append((ts, 'down'))
            elif 'MQTT_EVENT_CONNECTED' in s and ts is not None:
                events.append((ts, 'up'))

            m_sk = re_skip.search(s)
            if m_sk and ts is not None:
                dropped.append((ts, int(m_sk.group(1))))
                skipped.append((ts, int(m_sk.group(2))))

    return (np.array(internal, float), np.array(rtt, float),
            events, skipped, dropped)


# -------------------------------------------------------------- statistics
def summarise(name, a, stall_us):
    if len(a) == 0:
        print(f"\n=== {name}: no data ===")
        return None

    stalls = a[a >= stall_us]
    normal = a[a <  stall_us]
    if len(normal) == 0:
        print(f"\n=== {name}: ALL {len(a)} samples exceed the stall threshold "
              f"({stall_us/1000:.0f} ms). Raise the threshold. ===")
        return None

    print(f"\n=== {name} ===")
    print(f"  samples          : {len(a)}")
    print(f"  normal operation : {len(normal)}  ({100*len(normal)/len(a):.1f} %)")
    print(f"    median         = {np.median(normal)/1000:8.1f} ms")
    print(f"    min / max      = {normal.min()/1000:8.1f} / {normal.max()/1000:.1f} ms")
    print(f"    95th / 99th    = {np.percentile(normal,95)/1000:8.1f} / "
          f"{np.percentile(normal,99)/1000:.1f} ms")
    if len(stalls):
        print(f"  stalled (>{stall_us/1000:.0f} ms): {len(stalls)}"
              f"  ({100*len(stalls)/len(a):.1f} %), longest {stalls.max()/1e6:.1f} s")
    else:
        print(f"  stalled          : none")
    return normal


def stall_bursts(rtt):
    """Group stalled replies into bursts, tolerating brief normal gaps.

    During an outage the queued messages are all acknowledged at once, so
    their recorded waits form a descending run whose largest value equals the
    outage duration. A few normal replies can appear inside that run, so a
    short gap does not end the burst.
    """
    bursts, run, gap = [], [], 0
    for v in rtt:
        if v >= STALL_RTT_US:
            run.append(v); gap = 0
        elif run:
            gap += 1
            if gap > BURST_GAP_TOLERANCE:
                bursts.append(run); run, gap = [], 0
    if run:
        bursts.append(run)

    if not bursts:
        return
    print(f"\n=== stall bursts (merged): {len(bursts)} ===")
    for i, b in enumerate(bursts, 1):
        print(f"  burst {i}: {len(b):3d} messages held, outage ~{max(b)/1e6:.1f} s")


def connection_report(events, skipped, dropped):
    downs = [t for t, k in events if k == 'down']
    ups   = [t for t, k in events if k == 'up']
    print(f"\n=== broker connection ===")
    print(f"  disconnect events: {len(downs)}")
    if not downs:
        print("  connection stable throughout the run.")
    else:
        total = 0
        for d in downs:
            later = [u for u in ups if u > d]
            if later:
                dur = later[0] - d
                total += dur
                print(f"    down at {d/1000:7.1f} s, back at {later[0]/1000:7.1f} s"
                      f"  ({dur/1000:.1f} s)")
            else:
                print(f"    down at {d/1000:7.1f} s, never reconnected in this capture")
        if ups and downs:
            span = max(max(ups), max(downs)) - min(downs)
            print(f"  total outage      : {total/1000:.1f} s")
            if span > 0:
                print(f"  outage fraction   : {100*total/span:.1f} % of the observed window")

    if skipped:
        final_skip = skipped[-1][1]
        final_drop = dropped[-1][1] if dropped else 0
        print(f"  final skipped (no network): {final_skip}"
              f"   -> {final_skip/360:.1f} s of ECG at 360 Hz")
        print(f"  final dropped (queue full): {final_drop}")


# ------------------------------------------------------------------- plots
def make_plots(internal_n, rtt_n, rtt_all):
    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    if internal_n is not None:
        axes[0].hist(internal_n/1000, bins=60, color='#2a6fb0', alpha=0.85)
        axes[0].set_title(f'Internal latency\nmedian {np.median(internal_n)/1000:.0f} ms')
        axes[0].set_xlabel('latency (ms)'); axes[0].set_ylabel('count')

    if rtt_n is not None:
        axes[1].hist(rtt_n/1000, bins=60, color='#2a9d5c', alpha=0.85)
        axes[1].set_title(f'RTT, normal operation\nmedian {np.median(rtt_n)/1000:.0f} ms')
        axes[1].set_xlabel('RTT (ms)')

    if len(rtt_all):
        axes[2].plot(rtt_all/1000, lw=0.7, color='#c0392b')
        axes[2].set_yscale('log')
        axes[2].axhline(STALL_RTT_US/1000, color='k', ls='--', lw=1, label='stall threshold')
        axes[2].set_title('RTT over the run (log scale)')
        axes[2].set_xlabel('message index'); axes[2].set_ylabel('RTT (ms, log)')
        axes[2].legend()

    fig.tight_layout(); fig.savefig('latency_summary.png', dpi=150)
    print("\nsaved -> latency_summary.png")


def main():
    if len(sys.argv) < 2:
        print("usage: python analyze_latency.py <serial_capture.txt>")
        sys.exit(1)

    internal, rtt, events, skipped, dropped = parse_capture(sys.argv[1])
    if len(internal) == 0 and len(rtt) == 0:
        print("ERROR: no dump sections found in the capture.")
        sys.exit(1)

    int_n = summarise("INTERNAL LATENCY (sample -> publish)", internal, STALL_INTERNAL_US)
    rtt_n = summarise("RTT (publish -> PUBACK)", rtt, STALL_RTT_US)
    stall_bursts(rtt)
    connection_report(events, skipped, dropped)

    if int_n is not None and rtt_n is not None:
        print(f"\n=== end-to-end, normal operation (medians) ===")
        print(f"  internal {np.median(int_n)/1000:.0f} ms + RTT {np.median(rtt_n)/1000:.0f} ms"
              f"  = {(np.median(int_n)+np.median(rtt_n))/1000:.0f} ms")

    make_plots(int_n, rtt_n, rtt)


if __name__ == '__main__':
    main()