#!/usr/bin/env python3
"""Soak: the bot plays lives over many seeds, each in a fresh directory.

Prints one row per seed and a summary that a balance pass can trust: the
median day of death with its quartiles and a bootstrap range, lives ending
on day 1, deaths at home on night 1, causes of death by class, kills per day,
quiet hours and the first fight. Run from the repository root after a build:

    python tools/soak.py --exe build-ovh/dead_pixel_survival.exe --seeds 1-40
    python tools/soak.py --seeds 1-40 --profile careful --days 15 --csv out.csv
    python tools/soak.py --seeds 1-40 --mode stalker --tone hopeless
    python tools/soak.py --seeds 1-20 --tune zombie.attack.damage=6 --compare old.csv

Ten seeds are too few: a single seed moves by days between builds, so read a
change only when it shows on 40 or more (phase 14). Each run needs the UCRT64
bin directory on PATH (the game's DLLs). Runs that crash, time out or report
a stuck bot (FAIL) are marked and make the exit code 1.
"""

import argparse
import concurrent.futures
import csv
import os
import random
import re
import shutil
import statistics
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Death causes by class (the phase 14 targets' rows).
CAUSE_CLASSES = [
    ("people", re.compile(r"(^Killed by (?!zombies)|'s blow)")),
    ("thirst", re.compile(r"^Dehydrated")),
    ("hunger", re.compile(r"^Starved")),
    ("fire", re.compile(r"^Burned")),
    ("cold", re.compile(r"^(Froze|Died of cold)")),
    ("heat", re.compile(r"^(Heatstroke|Died of heat)")),
    ("infection", re.compile(r"(infection|^Turned|illness|^Died of fever)", re.I)),
    ("retired", re.compile(r"(retire|for good|crisis|^Gave up)", re.I)),
    ("out", re.compile(r"got out")),
    ("dead", re.compile(r"(zombie|Overrun|dead|Bled out|bitten)", re.I)),
]

SUMMARY_FIELDS = ["days", "kills", "kills_per_day", "containers", "boards", "food", "drink"]


def parse_seeds(text):
    seeds = []
    for part in text.split(","):
        if "-" in part:
            lo, hi = part.split("-", 1)
            seeds.extend(range(int(lo), int(hi) + 1))
        elif part:
            seeds.append(int(part))
    return seeds


def cause_class(cause, state):
    if cause in ("", "alive") or state == "playing":
        return "alive"
    for name, pattern in CAUSE_CLASSES:
        if pattern.search(cause):
            return name
    return "other"


def median(values):
    return statistics.median(values) if values else 0.0


def quartiles(values):
    if len(values) < 2:
        return (values[0], values[0]) if values else (0.0, 0.0)
    q = statistics.quantiles(values, n=4, method="inclusive")
    return q[0], q[2]


def bootstrap_median_range(values, rounds=2000, seed=14):
    """The middle 90 % of medians over resamples: how far the median of this
    many seeds can move by chance alone."""
    if len(values) < 3:
        return (median(values), median(values))
    rng = random.Random(seed)
    meds = sorted(statistics.median(rng.choices(values, k=len(values))) for _ in range(rounds))
    return meds[int(rounds * 0.05)], meds[int(rounds * 0.95) - 1]


def f(row, key, default=0.0):
    try:
        return float(row.get(key, default) or default)
    except ValueError:
        return default


def read_life(rows):
    """Everything a balance pass reads from one life's metrics rows."""
    life = {}
    first, last = rows[0], rows[-1]
    start = f(first, "day") * 24.0 + f(first, "hour")
    end = f(last, "day") * 24.0 + f(last, "hour")
    life["days"] = (end - start) / 24.0
    life["state"] = last["state"]
    life["cause"] = last.get("cause", "") or ("alive" if last["state"] == "playing" else last["state"])
    life["class"] = cause_class(life["cause"], last["state"])
    life["end_day"] = int(f(last, "day"))
    life["kills"] = int(f(last, "kills"))
    life["kills_per_day"] = life["kills"] / max(life["days"], 0.25)
    life["food"] = int(f(last, "food"))
    life["drink"] = int(f(last, "drink"))
    life["containers"] = int(f(last, "containers_searched"))
    life["boards"] = max(int(f(r, "boards_up")) for r in rows)
    died = last["state"] == "dead"
    life["day1_death"] = died and life["end_day"] <= 1
    # Died on the first night (day 1 18:00 to day 2 08:00) inside the home.
    life["night1_home"] = died and 42.0 <= end < 56.0 and last.get("at_home", "0") == "1"
    life["night1_death"] = died and 42.0 <= end < 56.0
    # Boards up at each dusk (19:00) the life saw.
    dusk = {}
    for r in rows:
        if int(f(r, "hour")) == 19:
            dusk.setdefault(int(f(r, "day")), int(f(r, "boards_up")))
    life["boards_dusk"] = dusk
    # Phase 14 columns (absent in older builds).
    ff = f(last, "first_fight", -1.0) if "first_fight" in last else None
    life["first_fight_day1"] = None if ff is None else (0.0 <= ff < 48.0)
    life["grabs"] = int(f(last, "grabs")) if "grabs" in last else None
    life["far_tiles"] = f(last, "far_tiles") if "far_tiles" in last else None
    # Quiet share by day, from the cumulative minute counts at each midnight.
    quiet = {}
    if "quiet_min" in last:
        marks = {}
        for r in rows:
            marks[int(f(r, "day"))] = (f(r, "quiet_min"), f(r, "sampled_min"))
        days = sorted(marks)
        prev = (0.0, 0.0)
        for d in days:
            q, s = marks[d]
            if s - prev[1] >= 300:  # at least five sampled hours that day
                quiet[d] = (q - prev[0]) / (s - prev[1])
            prev = (q, s)
    life["quiet"] = quiet
    # Sleep: hours sampled asleep each night (20:00 to 10:00), for nights
    # the life saw through to morning.
    # Rows come by game time awake but grow sparse asleep (the clock runs
    # fast), so the hours are summed between rows: an interval counts as
    # asleep when the row that ends it says so.
    nights = {}
    prev = None
    for r in rows:
        clock = f(r, "day") * 24.0 + f(r, "hour")
        if prev is not None and r.get("bot") == "asleep" and clock > prev:
            t = prev
            while t < clock:  # split the interval at each 10:00 so it lands in its night
                night = int((t - 10.0) // 24.0)  # night N runs 10:00 day N to 10:00 day N+1
                edge = (night + 1) * 24.0 + 10.0
                step = min(clock, edge) - t
                if 20.0 <= (t - 10.0) % 24.0 + 10.0 <= 34.0:
                    nights[night] = nights.get(night, 0.0) + step
                t += step
        prev = clock
    # A whole night: the life was there from its 20:00 to the 10:00 after.
    life["sleep_nights"] = [int(round(nights.get(n, 0.0)))
                            for n in range(int((start - 10.0) // 24.0), int((end - 10.0) // 24.0))
                            if n * 24.0 + 20.0 >= start]
    # A duel script (`--duel N`): the kills made after the metrics began and
    # the health lost on the way.
    life["duel_kills"] = int(f(last, "kills")) - int(f(first, "kills"))
    life["met"] = ff is not None and ff >= 0.0  # one of the dead came within 2 tiles
    life["hp_lost"] = max(0.0, f(first, "hp") - min(f(r, "hp") for r in rows))
    # Mind and supplies: drink carried at dawn of day 3, depression on day 7,
    # and the share of waking samples with panic above 75.
    life["drink_dawn3"] = next((int(f(r, "drink")) for r in rows if int(f(r, "day")) == 3 and f(r, "hour") >= 6.0),
                               None)
    life["depression_d7"] = next((f(r, "depression") for r in rows if int(f(r, "day")) == 7 and f(r, "hour") >= 12.0),
                                 None)
    waking = [r for r in rows if r.get("state") == "playing" and r.get("bot") != "asleep"]
    life["panic_share"] = (sum(1 for r in waking if f(r, "panic") > 75.0) / len(waking)) if waking else None
    # Containers holding anything at the life's start and end (phase 14).
    if "stocked_town" in last:
        life["stocked_start"] = (f(first, "stocked_home"), f(first, "boxes_home"),
                                 f(first, "stocked_town"), f(first, "boxes_town"))
        life["stocked_end"] = (f(last, "stocked_home"), f(last, "boxes_home"),
                               f(last, "stocked_town"), f(last, "boxes_town"))
    return life


def split_lives(rows):
    """One metrics file can hold several lives (`--lives N`): a life ends at
    its first row that is not `playing`; the next starts with the next
    `playing` row."""
    lives, current, ended = [], [], False
    last_clock = None
    for r in rows:
        playing = r.get("state") == "playing"
        clock = f(r, "day") * 24.0 + f(r, "hour")
        # A new life starts its clock again (day 1, morning): a row whose
        # clock runs backwards begins one even when no row caught the end.
        restarted = last_clock is not None and clock < last_clock - 1.0
        last_clock = clock
        if (ended and playing) or (restarted and current and not ended):
            lives.append(current)
            current, ended = [], False
        elif ended and restarted:
            lives.append(current)
            current, ended = [], False
        if ended:
            continue
        current.append(r)
        if not playing:
            ended = True
    if current:
        lives.append(current)
    return lives


def run_seed(exe, seed, script, workroot, timeout, keep, metrics_dir):
    work = os.path.join(workroot, "seed_%d" % seed)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    shutil.copytree(os.path.join(REPO, "assets"), os.path.join(work, "assets"))
    shutil.copytree(os.path.join(REPO, "tools"), os.path.join(work, "tools"))
    local_exe = os.path.join(work, os.path.basename(exe))
    shutil.copy2(exe, local_exe)

    cmd = [local_exe, "--script", script, "--hidden", "--world-seed", str(seed),
           "--metrics", "metrics.csv", "--shots", "shots"]
    result = {"seed": seed, "status": "ok", "days": 0.0, "cause": "", "class": "", "kills": 0, "food": 0,
              "drink": 0, "containers": 0, "boards": 0, "night1_home": False, "work": work}
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
        with open(metrics, newline="", encoding="utf-8") as fh:
            rows = list(csv.DictReader(fh))
        if metrics_dir:
            shutil.copy2(metrics, os.path.join(metrics_dir, "seed_%d.csv" % seed))
    lives = []
    for index, life_rows in enumerate(split_lives(rows) if rows else []):
        life = dict(result)
        life.update(read_life(life_rows))
        life["life"] = index + 1
        lives.append(life)
    if not lives:
        if result["status"] == "ok":
            result["status"] = "no metrics"
        result["life"] = 1
        lives.append(result)

    if not keep and result["status"] == "ok":
        shutil.rmtree(work, ignore_errors=True)
    return lives


def share(count, total):
    return "%d of %d (%.0f %%)" % (count, total, 100.0 * count / total) if total else "-"


def summarise(results):
    """The summary lines, and a flat dict of the numbers for --compare.
    Runs that did not finish clean (a stuck bot, a crash) are left out."""
    results = [r for r in results if r.get("status", "ok") == "ok"]
    n = len(results)
    if n == 0:
        return ["no clean runs"], {}
    days = sorted(r["days"] for r in results)
    lo, hi = quartiles(days)
    blo, bhi = bootstrap_median_range(days)
    out = {}
    out["lives"] = n
    out["median_days"] = median(days)
    out["q1_days"], out["q3_days"] = lo, hi
    out["mean_days"] = sum(days) / n
    out["day1_deaths"] = sum(1 for r in results if r.get("day1_death"))
    out["night1_deaths"] = sum(1 for r in results if r.get("night1_death"))
    out["night1_home"] = sum(1 for r in results if r.get("night1_home"))
    out["alive"] = sum(1 for r in results if r.get("class") == "alive")
    out["median_kills_per_day"] = median([r["kills_per_day"] for r in results if "kills_per_day" in r])
    out["median_boards"] = median([r["boards"] for r in results])
    out["median_searched"] = median([r["containers"] for r in results])
    classes = {}
    for r in results:
        classes[r.get("class", "")] = classes.get(r.get("class", ""), 0) + 1
    ended = n - out["alive"]
    for name, count in classes.items():
        out["cause_" + name] = count
    lines = []
    lines.append("lives %d: median days %.2f (quartiles %.2f to %.2f; a median of %d seeds moves %.2f to %.2f "
                 "by chance), mean %.2f" % (n, out["median_days"], lo, hi, n, blo, bhi, out["mean_days"]))
    lines.append("ended on day 1: %s; died on night 1: %s (at home %d); alive at the end: %d"
                 % (share(out["day1_deaths"], n), share(out["night1_deaths"], n), out["night1_home"], out["alive"]))
    lines.append("causes (of %d that ended): %s" % (ended, ", ".join(
        "%s %d (%.0f %%)" % (k, v, 100.0 * v / max(1, ended))
        for k, v in sorted(classes.items(), key=lambda kv: -kv[1]) if k != "alive")))
    lines.append("median kills per day %.1f, boards (most up) %.1f, searched %.1f"
                 % (out["median_kills_per_day"], out["median_boards"], out["median_searched"]))
    ff = [r["first_fight_day1"] for r in results if r.get("first_fight_day1") is not None]
    if ff:
        out["first_fight_day1"] = sum(ff) / len(ff)
        quiet13 = [v for r in results for d, v in r.get("quiet", {}).items() if 1 <= d <= 3]
        quiet814 = [v for r in results for d, v in r.get("quiet", {}).items() if 8 <= d <= 14]
        out["quiet_1_3"] = sum(quiet13) / len(quiet13) if quiet13 else float("nan")
        out["quiet_8_14"] = sum(quiet814) / len(quiet814) if quiet814 else float("nan")
        grabs = [r["grabs"] / max(r["days"], 0.25) for r in results if r.get("grabs") is not None]
        out["grabs_per_day"] = median(grabs)
        lines.append("first fight on day 1: %.0f %% of lives; quiet hours (none within 15 tiles) days 1-3: %.0f %%, "
                     "days 8-14: %s; grabs per day (median) %.1f"
                     % (100.0 * out["first_fight_day1"], 100.0 * out["quiet_1_3"],
                        "-" if not quiet814 else "%.0f %%" % (100.0 * out["quiet_8_14"]), out["grabs_per_day"]))
    sleeps = [h for r in results for h in r.get("sleep_nights", [])]
    if sleeps:
        out["sleep_6h_nights"] = sum(1 for h in sleeps if h >= 6) / len(sleeps)
        lines.append("nights slept 6 hours or more: %.0f %% of %d whole nights (median %.0f hours)"
                     % (100.0 * out["sleep_6h_nights"], len(sleeps), median(sleeps)))
    dusk = {}
    for r in results:
        for d, b in r.get("boards_dusk", {}).items():
            dusk.setdefault(d, []).append(b)
    if dusk:
        lines.append("boards up at dusk (median, lives seen): " + ", ".join(
            "day %d %.1f (%d)" % (d, median(v), len(v)) for d, v in sorted(dusk.items()) if d <= 7))
    drink3 = [r["drink_dawn3"] for r in results if r.get("drink_dawn3") is not None]
    dep7 = [r["depression_d7"] for r in results if r.get("depression_d7") is not None]
    panic = [r["panic_share"] for r in results if r.get("panic_share") is not None]
    reach = {d: sum(1 for r in results if r["days"] >= d - 1) for d in (7, 15)}
    out["reach_day7"], out["reach_day15"] = reach[7] / n, reach[15] / n
    if panic:
        out["panic_lives"] = sum(1 for p in panic if p > 0.05) / len(panic)
    if drink3:
        out["drink_dawn3"] = median(drink3)
    if dep7:
        out["depression_d7"] = median(dep7)
    lines.append("reached day 7: %s, day 15: %s; drink carried at dawn of day 3 (median): %s; depression on day 7 "
                 "(median): %s; lives with panic above 75 for over 5 %% of waking time: %s"
                 % (share(reach[7], n), share(reach[15], n), "%.0f (%d)" % (median(drink3), len(drink3)) if drink3
                    else "-", "%.0f (%d)" % (median(dep7), len(dep7)) if dep7 else "-",
                    "%.0f %%" % (100.0 * out["panic_lives"]) if panic else "-"))
    return lines, out


def summarise_lives(results):
    """With `--lives N`: the median days of each life in the world, and how
    many containers still hold anything as the lives go by."""
    by_life = {}
    for r in results:
        if r.get("status", "ok") == "ok":
            by_life.setdefault(r.get("life", 1), []).append(r)
    lines = []
    if len(by_life) < 2:
        return lines
    first = by_life.get(1, [])
    lines.append("by life in the world (median days, lives): " + ", ".join(
        "%d: %.2f (%d)" % (k, median([r["days"] for r in v]), len(v)) for k, v in sorted(by_life.items())))
    # Share of containers holding anything: in the district of that life's
    # home, and in the whole town; "start" is the first life's first row.
    def held(s, which):
        if not s:
            return None
        stocked, boxes = (s[0], s[1]) if which == "home" else (s[2], s[3])
        return stocked / boxes if boxes > 0 else None

    for which in ("home", "town"):
        starts = [x for x in (held(r.get("stocked_start"), which) for r in first) if x is not None]
        parts = ["start: %.0f %%" % (100.0 * median(starts))] if starts else []
        for k, v in sorted(by_life.items()):
            vals = [x for x in (held(r.get("stocked_end"), which) for r in v) if x is not None]
            if vals:
                parts.append("after %d: %.0f %%" % (k, 100.0 * median(vals)))
        if len(parts) > 1:
            lines.append("containers holding anything, %s (median share): %s"
                         % ("the life's home district" if which == "home" else "the town", ", ".join(parts)))
    return lines


def write_csv(path, results):
    cols = ["seed", "life", "status", "days", "end_day", "cause", "class", "kills", "kills_per_day", "food", "drink",
            "containers", "boards", "day1_death", "night1_death", "night1_home", "first_fight_day1", "grabs",
            "far_tiles", "quiet_d1", "quiet_d2", "quiet_d3", "quiet_d8_14", "sleep_nights", "drink_dawn3",
            "depression_d7", "panic_share", "stocked_start", "stocked_end"]
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(cols)
        for r in results:
            q = r.get("quiet", {})
            row = []
            for c in cols:
                if c == "quiet_d8_14":
                    v = [q[d] for d in q if 8 <= d <= 14]
                    row.append(" ".join("%.3f" % x for x in v))
                elif c.startswith("quiet_d"):
                    v = q.get(int(c[-1]))
                    row.append("" if v is None else "%.3f" % v)
                elif c == "sleep_nights":
                    row.append(" ".join(str(h) for h in r.get(c, [])))
                elif c.startswith("stocked_"):
                    row.append(" ".join("%g" % x for x in r[c]) if r.get(c) else "")
                elif r.get(c) is None:
                    row.append("")
                elif isinstance(r.get(c), float):
                    row.append("%.3f" % r[c])
                else:
                    row.append(r.get(c, ""))
            w.writerow(row)


def read_csv(path):
    results = []
    with open(path, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            r = {"seed": int(row["seed"]), "status": row["status"], "days": float(row["days"] or 0),
                 "end_day": int(float(row.get("end_day") or 0)), "cause": row["cause"], "class": row["class"],
                 "kills": int(float(row["kills"] or 0)), "kills_per_day": float(row["kills_per_day"] or 0),
                 "containers": int(float(row["containers"] or 0)), "boards": int(float(row["boards"] or 0)),
                 "day1_death": row["day1_death"] == "True", "night1_death": row["night1_death"] == "True",
                 "night1_home": row["night1_home"] == "True", "food": int(float(row.get("food") or 0)),
                 "drink": int(float(row.get("drink") or 0))}
            if row.get("first_fight_day1") not in (None, ""):
                r["first_fight_day1"] = row["first_fight_day1"] == "True"
            if row.get("grabs") not in (None, ""):
                r["grabs"] = int(float(row["grabs"]))
            q = {}
            for d in (1, 2, 3):
                if row.get("quiet_d%d" % d):
                    q[d] = float(row["quiet_d%d" % d])
            for i, v in enumerate((row.get("quiet_d8_14") or "").split()):
                q[8 + i] = float(v)  # the days themselves are not kept, only that they were 8 to 14
            r["quiet"] = q
            r["life"] = int(row.get("life") or 1)
            r["sleep_nights"] = [int(x) for x in (row.get("sleep_nights") or "").split()]
            for key in ("drink_dawn3", "depression_d7", "panic_share"):
                if row.get(key) not in (None, ""):
                    r[key] = float(row[key])
            for key in ("stocked_start", "stocked_end"):
                if row.get(key):
                    r[key] = tuple(float(x) for x in row[key].split())
            results.append(r)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", default=os.path.join(REPO, "build-ovh", "dead_pixel_survival.exe"))
    parser.add_argument("--seeds", default="1-40")
    parser.add_argument("--script", default="tools/harness/bot_week.txt",
                        help="harness script, relative to the repository (copied into each run)")
    parser.add_argument("--profile", choices=["careful", "normal", "bold"],
                        help="rewrite the script's bot line to this profile")
    parser.add_argument("--days", type=int, help="rewrite the script's bot line to play this many days")
    parser.add_argument("--mode", choices=["swarm", "stalker", "mutating", "society", "unknown", "hybrid"],
                        help="play the runs in this threat mode (a mode line before the script's newrun)")
    parser.add_argument("--tone", choices=["hopeless", "grim", "brutal"],
                        help="play the runs in this tone (a tone line before the script's newrun)")
    parser.add_argument("--tune", action="append", default=[], metavar="KEY=VALUE",
                        help="a tune line before the script's newrun (repeatable)")
    parser.add_argument("--lives", type=int, default=1,
                        help="play this many lives in one world per seed (the script's newrun..bot block repeated); "
                             "the summary is of the first lives, then a line per life")
    parser.add_argument("--jobs", type=int, default=6, help="runs at once")
    parser.add_argument("--timeout", type=int, default=5400, help="seconds per run")
    parser.add_argument("--work", default=None, help="where the run directories go (default: a temp dir)")
    parser.add_argument("--keep", action="store_true", help="keep run directories that passed")
    parser.add_argument("--csv", help="write one row per life here")
    parser.add_argument("--metrics-dir", help="copy each life's metrics.csv here as seed_N.csv")
    parser.add_argument("--compare", help="a CSV from an earlier soak: print the difference per measure")
    parser.add_argument("--report", help="read this CSV and print its summary (no runs)")
    parser.add_argument("--from-metrics", metavar="DIR",
                        help="read the seed_N.csv files a soak's --metrics-dir kept and summarise them (no runs; "
                             "with --csv, write the rows again with today's measures)")
    parser.add_argument("--duel", type=int, default=0, metavar="N",
                        help="the script is a duel against N of the dead: print the odds instead")
    parser.add_argument("--quiet-rows", action="store_true", help="print the summary only, not a row per seed")
    args = parser.parse_args()

    if args.report:
        results = read_csv(args.report)
        for line in summarise([r for r in results if r["life"] == 1])[0] + summarise_lives(results):
            print(line)
        return 0
    if args.from_metrics:
        results = []
        for name in sorted(os.listdir(args.from_metrics)):
            m = re.match(r"seed_(\d+)\.csv$", name)
            if not m:
                continue
            with open(os.path.join(args.from_metrics, name), newline="", encoding="utf-8") as fh:
                rows = list(csv.DictReader(fh))
            for index, life_rows in enumerate(split_lives(rows)):
                r = {"seed": int(m.group(1)), "status": "ok", "life": index + 1}
                r.update(read_life(life_rows))
                # A run that stopped with the bot still playing on its last
                # day did not finish clean (stuck, crashed or cut short).
                if r["state"] == "playing" and args.days and r["days"] < args.days - 1:
                    r["status"] = "unfinished"
                results.append(r)
        results.sort(key=lambda r: (r["seed"], r["life"]))
        for line in summarise([r for r in results if r["life"] == 1])[0] + summarise_lives(results):
            print(line)
        if args.csv:
            write_csv(args.csv, results)
            print("wrote %s" % args.csv)
        return 0

    exe = os.path.abspath(args.exe)
    if not os.path.exists(exe):
        print("no executable at %s (build first, or pass --exe)" % exe)
        return 2
    workroot = args.work or tempfile.mkdtemp(prefix="dead_pixel_soak_")
    os.makedirs(workroot, exist_ok=True)
    metrics_dir = os.path.abspath(args.metrics_dir) if args.metrics_dir else None
    if metrics_dir:
        os.makedirs(metrics_dir, exist_ok=True)

    script = args.script.replace("\\", "/")
    if args.profile or args.mode or args.tone or args.tune or args.days or args.lives > 1:
        with open(os.path.join(REPO, args.script), encoding="utf-8") as fh:
            lines = fh.read().splitlines()
        out = []
        for l in lines:
            parts = l.split()
            if parts and parts[0] == "bot":
                profile = args.profile or (parts[1] if len(parts) > 1 else "normal")
                days = args.days or (int(parts[2]) if len(parts) > 2 else 7)
                l = " ".join(["bot", profile, str(days)] + parts[3:])
            out.append(l)
        lines = out
        at = next((i for i, l in enumerate(lines) if l.strip() == "newrun"), 0)
        if args.lives > 1:
            bot_at = next((i for i, l in enumerate(lines) if l.split()[:1] == ["bot"]), None)
            if bot_at is None or bot_at < at:
                print("--lives needs a script with newrun before its bot line")
                return 2
            block = [l for l in lines[at:bot_at + 1] if l.strip() and not l.lstrip().startswith("#")]
            lines[bot_at + 1:bot_at + 1] = block * (args.lives - 1)
        pre = []
        if args.mode:
            pre.append("mode %s" % args.mode)
        if args.tone:
            pre.append("tone %s" % args.tone)
        for t in args.tune:
            key, value = t.split("=", 1)
            pre.append("tune %s %s" % (key, value))
        lines[at:at] = pre
        custom = os.path.join(workroot, "soak_script.txt")
        with open(custom, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
        script = custom

    seeds = parse_seeds(args.seeds)
    print("soak: %d seed(s), %d at once, %s, %s%s, runs in %s" % (
        len(seeds), args.jobs, "mode " + args.mode if args.mode else "default mode",
        "tone " + args.tone if args.tone else "default tone",
        "".join(", tune " + t for t in args.tune), workroot), flush=True)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = [pool.submit(run_seed, exe, s, script, workroot, args.timeout, args.keep, metrics_dir)
                   for s in seeds]
        for future in concurrent.futures.as_completed(futures):
            lives = future.result()
            results.extend(lives)
            r = lives[0]
            print("  seed %2d done: %s, %.1f days, %s%s" % (
                r["seed"], r["status"], r["days"], r["cause"],
                "" if len(lives) < 2 else " (%d lives, last %.1f days)" % (len(lives), lives[-1]["days"])), flush=True)

    results.sort(key=lambda r: (r["seed"], r.get("life", 1)))
    all_lives = results
    results = [r for r in all_lives if r.get("life", 1) == 1]
    if not args.quiet_rows:
        print()
        print("| Seed | Days | End | Kills | Food | Drink | Searched | Boards | Night 1 at home | Run |")
        print("|---:|---:|---|---:|---:|---:|---:|---:|---|---|")
        for r in results:
            print("| %d | %.1f | %s | %d | %d | %d | %d | %d | %s | %s |" % (
                r["seed"], r["days"], r["cause"], r["kills"], r["food"], r["drink"], r["containers"], r["boards"],
                "died" if r["night1_home"] else "-", r["status"]))
    ok = [r for r in results if r["status"] == "ok"]
    print()
    if args.duel:
        won = [r for r in results if r.get("state") != "dead" and r.get("duel_kills", 0) >= args.duel]
        lost_hp = sorted(r.get("hp_lost", 0.0) for r in won)
        dead = sum(1 for r in results if r.get("state") == "dead")

        def outcome(r):
            if r.get("state") == "dead":
                return "died"
            if r in won:
                return "won"
            return "fled or unfinished" if r.get("met") else "never met"

        for r in results:
            print("  seed %d: %s, %d killed, %.0f health lost" % (
                r["seed"], outcome(r), r.get("duel_kills", 0), r.get("hp_lost", 0.0)))
        met = [r for r in results if r.get("met") or r in won or r.get("state") == "dead"]
        print("duel against %d: of %d that met, won %s, died %d, fled or unfinished %d (never met: %d); "
              "health lost when won: median %.0f" % (
                  args.duel, len(met), share(len(won), len(met)), dead, len(met) - len(won) - dead,
                  len(results) - len(met), median(lost_hp)))
        if args.csv:
            with open(args.csv, "w", newline="", encoding="utf-8") as fh:
                w = csv.writer(fh)
                w.writerow(["seed", "status", "outcome", "killed", "hp_lost"])
                for r in results:
                    w.writerow([r["seed"], r["status"], outcome(r), r.get("duel_kills", 0),
                                "%.1f" % r.get("hp_lost", 0.0)])
            print("wrote %s" % args.csv)
        return 0 if len(ok) == len(results) else 1
    lines, numbers = summarise(results)
    for line in lines + summarise_lives(all_lives):
        print(line)
    print("clean runs %d of %d" % (len(ok), len(results)))
    if args.csv:
        write_csv(args.csv, all_lives)
        print("wrote %s" % args.csv)
    if args.compare:
        old = [r for r in read_csv(args.compare) if r["life"] == 1]
        _, before = summarise(old)
        print()
        print("compared with %s (%d lives):" % (args.compare, len(old)))
        for key in sorted(set(before) | set(numbers)):
            a, b = before.get(key), numbers.get(key)
            if isinstance(a, (int, float)) and isinstance(b, (int, float)):
                print("  %-22s %8.2f -> %8.2f  (%+.2f)" % (key, a, b, b - a))
            elif a is not None or b is not None:
                print("  %-22s %8s -> %8s" % (key, a, b))
        same = {r["seed"]: r["days"] for r in old}
        moved = [(r["seed"], same[r["seed"]], r["days"]) for r in results if r["seed"] in same
                 and abs(same[r["seed"]] - r["days"]) > 0.05]
        print("  seeds that moved: %d of %d" % (len(moved), len([r for r in results if r["seed"] in same])))
    return 0 if len(ok) == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
