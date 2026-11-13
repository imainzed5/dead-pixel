#!/usr/bin/env python3
"""Soak: the bot plays a week for seeds 1 to 10, each in a fresh directory.

Prints one row per seed (days survived, cause, kills, food and water carried
at the end, containers searched) and a summary. This is how later phases read
balance. Run from the repository root after a build:

    python tools/soak.py --exe build/dead_pixel_survival.exe
    python tools/soak.py --exe build/dead_pixel_survival.exe --seeds 1-3 --jobs 3 --profile careful

Each run needs the UCRT64 bin directory on PATH (the game's DLLs). Runs that
crash, time out or report a stuck bot (FAIL) are marked and make the exit
code 1.
"""

import argparse
import concurrent.futures
import csv
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def parse_seeds(text):
    seeds = []
    for part in text.split(","):
        if "-" in part:
            lo, hi = part.split("-", 1)
            seeds.extend(range(int(lo), int(hi) + 1))
        elif part:
            seeds.append(int(part))
    return seeds


def run_seed(exe, seed, script, workroot, timeout, keep):
    work = os.path.join(workroot, "seed_%d" % seed)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    shutil.copytree(os.path.join(REPO, "assets"), os.path.join(work, "assets"))
    shutil.copytree(os.path.join(REPO, "tools"), os.path.join(work, "tools"))
    local_exe = os.path.join(work, os.path.basename(exe))
    shutil.copy2(exe, local_exe)

    cmd = [local_exe, "--script", script, "--hidden", "--world-seed", str(seed),
           "--metrics", "metrics.csv", "--shots", "shots"]
    result = {"seed": seed, "status": "ok", "days": 0.0, "cause": "", "kills": 0, "food": 0,
              "drink": 0, "containers": 0, "work": work}
    try:
        proc = subprocess.run(cmd, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=timeout, text=True, errors="replace")
        output = proc.stdout
        with open(os.path.join(work, "out.log"), "w", encoding="utf-8") as log:
            log.write(output)
        if "FAIL" in output:
            result["status"] = "FAIL: " + next(l for l in output.splitlines() if "FAIL" in l).strip()
        elif proc.returncode != 0:
            result["status"] = "exit %d" % proc.returncode
    except subprocess.TimeoutExpired:
        result["status"] = "timeout"

    rows = []
    metrics = os.path.join(work, "metrics.csv")
    if os.path.exists(metrics):
        with open(metrics, newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
    if rows:
        first, last = rows[0], rows[-1]
        start = float(first["day"]) * 24.0 + float(first["hour"])
        end = float(last["day"]) * 24.0 + float(last["hour"])
        result["days"] = (end - start) / 24.0
        result["cause"] = last.get("cause", "") or ("alive" if last["state"] == "playing" else last["state"])
        result["kills"] = int(float(last["kills"]))
        result["food"] = int(float(last.get("food", 0) or 0))
        result["drink"] = int(float(last.get("drink", 0) or 0))
        result["containers"] = int(float(last.get("containers_searched", 0) or 0))
    elif result["status"] == "ok":
        result["status"] = "no metrics"

    if not keep and result["status"] == "ok":
        shutil.rmtree(work, ignore_errors=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", default=os.path.join(REPO, "build", "dead_pixel_survival.exe"))
    parser.add_argument("--seeds", default="1-10")
    parser.add_argument("--script", default="tools/harness/bot_week.txt",
                        help="harness script, relative to the repository (copied into each run)")
    parser.add_argument("--profile", choices=["careful", "normal", "bold"],
                        help="rewrite the script's bot line to this profile")
    parser.add_argument("--jobs", type=int, default=2, help="runs at once")
    parser.add_argument("--timeout", type=int, default=3600, help="seconds per run")
    parser.add_argument("--work", default=None, help="where the run directories go (default: a temp dir)")
    parser.add_argument("--keep", action="store_true", help="keep run directories that passed")
    args = parser.parse_args()

    exe = os.path.abspath(args.exe)
    if not os.path.exists(exe):
        print("no executable at %s (build first, or pass --exe)" % exe)
        return 2
    workroot = args.work or tempfile.mkdtemp(prefix="dead_pixel_soak_")
    os.makedirs(workroot, exist_ok=True)

    script = args.script.replace("\\", "/")
    if args.profile:
        with open(os.path.join(REPO, args.script), encoding="utf-8") as f:
            lines = f.read().splitlines()
        lines = [("bot %s %s" % (args.profile, l.split()[2] if len(l.split()) > 2 else "7"))
                 if l.strip().startswith("bot ") else l for l in lines]
        custom = os.path.join(workroot, "soak_script.txt")
        with open(custom, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        script = custom

    seeds = parse_seeds(args.seeds)
    print("soak: %d seed(s), %d at once, runs in %s" % (len(seeds), args.jobs, workroot))
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = [pool.submit(run_seed, exe, s, script, workroot, args.timeout, args.keep) for s in seeds]
        for future in concurrent.futures.as_completed(futures):
            r = future.result()
            results.append(r)
            print("  seed %2d done: %s" % (r["seed"], r["status"]), flush=True)

    results.sort(key=lambda r: r["seed"])
    print()
    print("| Seed | Days | End | Kills | Food | Drink | Searched | Run |")
    print("|---:|---:|---|---:|---:|---:|---:|---|")
    for r in results:
        print("| %d | %.1f | %s | %d | %d | %d | %d | %s |" % (
            r["seed"], r["days"], r["cause"], r["kills"], r["food"], r["drink"], r["containers"], r["status"]))
    ok = [r for r in results if r["status"] == "ok"]
    if results:
        days = sorted(r["days"] for r in results)
        median = days[len(days) // 2] if len(days) % 2 else (days[len(days) // 2 - 1] + days[len(days) // 2]) / 2
        alive = sum(1 for r in results if r["cause"] == "alive")
        print()
        print("median days %.1f, mean %.1f, alive at the end %d of %d, clean runs %d of %d" % (
            median, sum(days) / len(days), alive, len(results), len(ok), len(results)))
    return 0 if len(ok) == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
