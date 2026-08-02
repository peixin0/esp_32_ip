"""
analyze_stack_monitor.py - Analyse FreeRTOS task stack High-Water-Mark logs.

Works with the raw serial capture produced by your simple cap.py (which just
dumps everything to a .txt file with no filtering). This script does the
filtering/parsing itself.

Expects lines like (as printed by your task_stack_monitor):
    I (10775) TASK_MON: task_ecg words left 7620, byte left 30480

IMPORTANT ESP-IDF QUIRK:
    In vanilla FreeRTOS, uxTaskGetStackHighWaterMark() returns a count of
    "words" (1 word = 4 bytes), so you'd multiply by 4 to get bytes.
    ESP-IDF's FreeRTOS port is different: StackType_t is byte-addressed,
    so the raw return value IS ALREADY BYTES. The "byte left" field in the
    firmware log (raw_value * 4) is therefore wrong/inflated on ESP32 --
    this script uses the "words left" field instead, since despite its
    name, that's the field holding the correct byte count on this platform.
    (Proof: a task with an 8192-byte stack cannot have 30480 bytes free --
    only the un-multiplied value is physically possible.)

Usage:
    python analyze_stack_monitor.py jitter.txt --allocated 8192

Options:
    --allocated  the stack size (bytes) you allocated for each task in
                 xTaskCreate (default 8192, matches your current 4096*2).
    --csv        output CSV path (default stack_monitor_analysis.csv)
    --plot       output PNG path (default stack_monitor_plot.png)
"""
import argparse
import csv
import re
from collections import defaultdict

# "bytes?" matches both "byte left" and "bytes left" so a future spelling
# fix in your firmware won't break this script again.
LOG_PATTERN = re.compile(
    r"I \((?P<ts>\d+)\)\s*TASK_MON:\s*(?P<task>\S+)\s+words left\s+(?P<words>\d+),\s*bytes? left\s+(?P<bytes>\d+)"
)


def parse_file(path):
    """Read the raw capture file, return {task_name: [(uptime_ms, words, bytes), ...]}"""
    data = defaultdict(list)
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = LOG_PATTERN.search(line)
            if not m:
                continue
            ts = int(m.group("ts"))
            task = m.group("task")
            # On ESP-IDF, uxTaskGetStackHighWaterMark() already returns bytes,
            # despite the firmware log calling it "words left". The firmware's
            # "byte left" field (words*4) is the wrong, inflated one -- ignore it.
            free_bytes = int(m.group("words"))
            data[task].append((ts, free_bytes))
    return data


def analyze(data, allocated_bytes, stable_window=3):
    """For each task: worst-case (minimum) free stack, actual usage, recommended
    size with 25% margin, and whether the minimum has converged (stopped
    dropping over the last few samples)."""
    print("=" * 60)
    print("STACK ANALYSIS SUMMARY")
    print("=" * 60)

    results = {}
    for task, samples in data.items():
        if not samples:
            continue
        bytes_series = [b for _, b in samples]
        min_free = min(bytes_series)
        used = allocated_bytes - min_free
        recommended = int(used * 1.25)

        tail = bytes_series[-stable_window:]
        converged = len(tail) >= stable_window and min(tail) == min_free
        status = "CONVERGED" if converged else "NOT YET CONVERGED - capture longer!"

        print(f"\nTask: {task}")
        print(f"  Samples collected      : {len(samples)}")
        print(f"  Allocated stack        : {allocated_bytes} bytes")
        print(f"  Min free (worst HWM)   : {min_free} bytes")
        print(f"  Actually used          : {used} bytes")
        print(f"  Recommended stack size : {recommended} bytes  (+25% margin)")
        print(f"  Status                 : {status}")

        results[task] = {
            "min_free_bytes": min_free,
            "used_bytes": used,
            "recommended_bytes": recommended,
            "converged": converged,
        }
    return results


def save_csv(data, out_csv):
    with open(out_csv, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["uptime_ms", "task", "free_bytes"])
        for task, samples in data.items():
            for ts, free_bytes in samples:
                w.writerow([ts, task, free_bytes])
    print(f"\nRaw samples saved to {out_csv}")


def plot(data, out_png):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed, skipping plot (pip install matplotlib)")
        return

    plt.figure(figsize=(9, 5))
    for task, samples in data.items():
        if not samples:
            continue
        xs = [s[0] / 1000 for s in samples]   # ms -> s (ESP32 uptime)
        ys = [s[1] for s in samples]
        plt.plot(xs, ys, marker="o", markersize=3, label=task)

    plt.xlabel("ESP32 uptime (s)")
    plt.ylabel("Free stack remaining (bytes)")
    plt.title("FreeRTOS Task Stack High-Water-Mark over time")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150)
    print(f"Plot saved to {out_png}")


def main():
    parser = argparse.ArgumentParser(description="Analyze ESP32 stack monitor log")
    parser.add_argument("logfile", help="raw serial capture .txt file (from your cap.py)")
    parser.add_argument("--allocated", type=int, default=8192,
                         help="allocated stack size in bytes (default 8192)")
    parser.add_argument("--csv", default="stack_monitor_analysis.csv")
    parser.add_argument("--plot", default="stack_monitor_plot.png")
    args = parser.parse_args()

    data = parse_file(args.logfile)
    if not data:
        print("No TASK_MON lines matched. Check the log actually contains "
              "'TASK_MON:' and 'words left' text, and that the file path is correct.")
        return

    analyze(data, args.allocated)
    save_csv(data, args.csv)
    plot(data, args.plot)


if __name__ == "__main__":
    main()