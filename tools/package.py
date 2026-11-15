"""Build the packaged (player) game and zip it, ready to copy to another PC.

    python tools/package.py            # dist/DeadPixelSurvival-<version>.zip
    python tools/package.py --demo     # dist/DeadPixelSurvival-<version>-demo.zip

The packaged build hides the dev keys (F1, F3, K) and keeps saves in
%APPDATA%/DeadPixelSurvival unless a portable.txt sits beside the exe. The
demo adds demo.txt: a life ends at dawn on day 7 with a card, then the
retirement screen. The DLLs the exe needs (SDL2, GLEW, the MinGW runtime)
are found by walking its imports with objdump, so the folder runs on a PC
without MSYS2. Run from an MSYS2 UCRT64 shell or with its bin on PATH.

Made locally only: this script does not upload anything.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = "dead_pixel_survival.exe"

README = """Dead Pixel Survival {version}{demo_note}

Run dead_pixel_survival.exe. No installation; nothing else is needed.

Saves and settings are kept in %APPDATA%\\DeadPixelSurvival. To keep them
in this folder instead (a USB stick), make an empty file named portable.txt
beside the exe.

Keys (all of them can be changed under Settings > Keys):
  WASD move, Shift run, C crouch, E use (hold E to push), Space shove,
  left click attack, right click take down, G throw, F use item, R board
  or reload, X pry, Q flashlight, 1-8 belt, Tab pack and body, N make,
  B build, J notebook, M map, V get in or out of a car, Esc pause.
  F2 shows the controls in the game.
{demo_text}
This is a game about getting through the days. It saves at dawn, when
you come home, and when you quit to the title.
"""

DEMO_TEXT = """
The demo: a life ends at dawn on day 7. The town remembers it.
"""


def version():
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(\s*dead_pixel_survival\s+VERSION\s+([0-9.]+)", text)
    return match.group(1) if match else "0.0.0"


def run(cmd):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], check=True, cwd=ROOT)


def imports(binary, objdump):
    out = subprocess.run([objdump, "-p", str(binary)], check=True, capture_output=True, text=True).stdout
    return [m.group(1) for m in re.finditer(r"DLL Name:\s*(\S+)", out)]


def gather_dlls(exe, objdump):
    """Every non-system DLL the exe needs, found on PATH (MSYS2's bin first)."""
    windows = Path(os.environ.get("SystemRoot", r"C:\Windows")).resolve()
    search = [Path(p) for p in os.environ.get("PATH", "").split(os.pathsep) if p]
    found = {}
    todo = [exe]
    while todo:
        for name in imports(todo.pop(), objdump):
            key = name.lower()
            if key in found:
                continue
            if key.startswith(("api-ms-win-", "ext-ms-")):
                found[key] = None  # an API set: Windows maps it, never shipped
                continue
            path = None
            for folder in search:
                candidate = folder / name
                if candidate.is_file():
                    path = candidate.resolve()
                    break
            if path is None or windows in path.parents:
                found[key] = None  # a system DLL (or an API set): Windows has it
                continue
            found[key] = path
            todo.append(path)
    return sorted(p for p in found.values() if p is not None)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--demo", action="store_true", help="the demo: a life ends at dawn on day 7")
    parser.add_argument("--build-dir", default="build-package", help="where to build (default build-package)")
    parser.add_argument("--out", default="dist", help="where the folder and zip go (default dist)")
    parser.add_argument("--skip-build", action="store_true", help="package what is already built")
    parser.add_argument("--jobs", type=int, default=6, help="parallel compiles (default 6; more can run out of memory)")
    args = parser.parse_args()

    build = ROOT / args.build_dir
    if not args.skip_build:
        run(["cmake", "-S", ROOT, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
             "-DDEAD_PIXEL_PACKAGED=ON", "-DDEAD_PIXEL_BUILD_TESTS=OFF"])
        run(["cmake", "--build", build, "--target", "dead_pixel_survival", "-j", args.jobs])
    exe = build / EXE
    if not exe.is_file():
        sys.exit(f"no {exe}: build it first (or drop --skip-build)")

    objdump = shutil.which("objdump")
    if objdump is None:
        sys.exit("objdump not found: run from an MSYS2 UCRT64 shell, or put its bin on PATH")

    ver = version()
    name = f"DeadPixelSurvival-{ver}" + ("-demo" if args.demo else "")
    out = ROOT / args.out
    stage = out / name
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    shutil.copy2(exe, stage / EXE)
    shutil.copytree(ROOT / "assets", stage / "assets")
    dlls = gather_dlls(exe, objdump)
    for dll in dlls:
        shutil.copy2(dll, stage / dll.name)
    shutil.copy2(ROOT / "LICENSE", stage / "LICENSE.txt")
    (stage / "README.txt").write_text(
        README.format(version=ver, demo_note=" (demo)" if args.demo else "",
                      demo_text=DEMO_TEXT if args.demo else ""),
        encoding="utf-8", newline="\r\n")
    if args.demo:
        (stage / "demo.txt").write_text("A life ends at dawn on day 7.\n", encoding="utf-8")

    archive = out / f"{name}.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                zf.write(path, Path(name) / path.relative_to(stage))
    size = archive.stat().st_size / (1024 * 1024)
    print(f"{len(dlls)} DLLs: " + ", ".join(d.name for d in dlls))
    print(f"wrote {stage} and {archive} ({size:.1f} MB)")


if __name__ == "__main__":
    main()
