#!/usr/bin/env python3
"""Soak report: turns soak CSVs into a Markdown page of small text tables.

Each CSV is one column of the table (a profile, a mode, a tone, before and
after); the rows are the phase 14 measures with their targets. Run from the
repository root:

    python tools/soak_report.py --out docs/overhaul/balance/report_2026-11-15.md \
        normal=docs/overhaul/balance/final_normal.csv careful=final_careful.csv

A LABEL=PATH pair per column; the first column is the one the targets are
read against. Duel results and anything the CSVs cannot hold are added by
hand under the table (--note "text", repeatable).
"""

import argparse
import datetime
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import soak  # noqa: E402

# (key in summarise's numbers, label, target text, format)
ROWS = [
    ("lives", "Lives (clean runs)", "40", "%d"),
    ("median_days", "Median days survived",
     "normal 2-4, careful 3-6, bold 1.5-3 (phase file: 5-8, 7-12, 3-6; see DECISIONS)", "%.2f"),
    ("q1_days", "First quartile", "", "%.2f"),
    ("q3_days", "Third quartile", "", "%.2f"),
    ("mean_days", "Mean days", "", "%.2f"),
    ("day1_share", "Lives ending on day 1", "under 5 %", "%.0f %%"),
    ("night1_share", "Died on night 1", "", "%.0f %%"),
    ("reach_day7", "Reached day 7", "careful 5-20 %", "%.0f %%"),
    ("reach_day15", "Reached day 15", "tenth life (phase file: careful 10-25 %)", "%.0f %%"),
    ("cause_dead", "Cause: the dead", "45-60 %", "%.0f %%"),
    ("cause_infection", "Cause: infection and illness", "10-20 %", "%.0f %%"),
    ("cause_thirst", "Cause: thirst", "5-12 %", "%.0f %%"),
    ("cause_hunger", "Cause: hunger", "3-10 %", "%.0f %%"),
    ("cause_cold", "Cause: cold", "2-8 % with heat", "%.0f %%"),
    ("cause_heat", "Cause: heat", "", "%.0f %%"),
    ("cause_fire", "Cause: fire", "under 5 %", "%.0f %%"),
    ("cause_people", "Cause: other people", "2-8 % (Society 20-35 %)", "%.0f %%"),
    ("cause_retired", "Retired (crisis)", "1-5 %", "%.0f %%"),
    ("median_kills_per_day", "Kills per day (median)", "1-4", "%.1f"),
    ("first_fight_day1", "First fight on day 1", "60-90 %", "%.0f %%"),
    ("quiet_1_3", "Quiet hours, days 1-3", "60-80 %", "%.0f %%"),
    ("quiet_8_14", "Quiet hours, days 8-14", "35-55 %", "%.0f %%"),
    ("grabs_per_day", "Grabs per day (median)", "", "%.1f"),
    ("median_boards", "Boards up (most, median)", "", "%.1f"),
    ("median_searched", "Containers searched (median)", "", "%.1f"),
    ("drink_dawn3", "Drinks carried at dawn of day 3", "4-7", "%.0f"),
    ("sleep_6h_nights", "Nights slept 6 hours or more", "over 70 %", "%.0f %%"),
    ("depression_d7", "Depression on day 7 (median)", "35-55 alone", "%.0f"),
    ("panic_lives", "Lives panicked over 5 % of waking time", "under 15 %", "%.0f %%"),
]

PERCENT_OF_LIVES = {"day1_share": "day1_deaths", "night1_share": "night1_deaths"}


def column(path):
    results = soak.read_csv(path)
    first = [r for r in results if r["life"] == 1]
    lines, numbers = soak.summarise(first)
    n = numbers.get("lives", 0)
    ended = n - numbers.get("alive", 0)
    for share_key, count_key in PERCENT_OF_LIVES.items():
        if n:
            numbers[share_key] = numbers.get(count_key, 0) / n
    for key in list(numbers):
        if key.startswith("cause_"):
            numbers[key] = numbers[key] / max(1, ended)
    for row in ROWS:
        if row[0].startswith("cause_") and row[0] not in numbers and n:
            numbers[row[0]] = 0.0
    return numbers, lines + soak.summarise_lives(results)


def cell(numbers, key, fmt):
    v = numbers.get(key)
    if v is None or v != v:  # missing or NaN
        return "-"
    return fmt % (100.0 * v) if "%%" in fmt and key != "lives" else fmt % v


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("columns", nargs="+", metavar="LABEL=CSV")
    parser.add_argument("--out", help="write the Markdown here (default: print it)")
    parser.add_argument("--title", default="Balance report")
    parser.add_argument("--note", action="append", default=[], help="a line under the table (repeatable)")
    args = parser.parse_args()

    cols = []
    for spec in args.columns:
        label, path = spec.split("=", 1)
        cols.append((label, path) + column(path))

    out = ["# %s" % args.title, "",
           "Written by `tools/soak_report.py` on %s from %s." % (
               datetime.date.today().isoformat(), ", ".join("`%s`" % os.path.basename(c[1]) for c in cols)),
           "Grim Human tone, Overwhelming Swarm, 24-minute days and the first life of a world unless a column "
           "says otherwise.", ""]
    out.append("| Measure | Target | " + " | ".join(c[0] for c in cols) + " |")
    out.append("|---|---|" + "---:|" * len(cols))
    for key, label, target, fmt in ROWS:
        out.append("| %s | %s | %s |" % (label, target, " | ".join(cell(c[2], key, fmt) for c in cols)))
    out.append("")
    for label, path, _, lines in cols:
        out.append("**%s** (`%s`):" % (label, os.path.basename(path)))
        out.append("")
        out.extend("    " + l for l in lines)
        out.append("")
    if args.note:
        out.extend(args.note)
        out.append("")
    text = "\n".join(out)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            fh.write(text)
        print("wrote %s" % args.out)
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
