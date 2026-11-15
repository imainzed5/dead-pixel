"""The game's icon: a boarded door, drawn in code at the game's pixel scale.

Writes assets/icons/icon_32.png (the window icon), icon_256.png (the same
pixels at 8x) and dead_pixel.ico (16, 32, 48 and 256 px, for the exe).

    python tools/generate_icon.py
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "icons"

CLEAR = (0, 0, 0, 0)
FRAME = (38, 30, 26, 255)       # the door frame, near black
DOOR = (112, 74, 46, 255)       # the door's planks
DOOR_DARK = (88, 56, 34, 255)   # the gaps between them
DOOR_LIGHT = (132, 90, 58, 255)
KNOB = (214, 170, 82, 255)      # the panel amber
BOARD = (182, 138, 88, 255)     # boards nailed across
BOARD_EDGE = (74, 50, 32, 255)
NAIL = (222, 220, 212, 255)
STEP = (70, 70, 74, 255)


def draw_32():
    img = Image.new("RGBA", (32, 32), CLEAR)
    px = img.load()

    # The frame and the step under it.
    for y in range(1, 30):
        for x in range(6, 26):
            px[x, y] = FRAME
    for x in range(4, 28):
        px[x, 30] = STEP
        px[x, 29] = STEP

    # The door: four planks with dark seams and a lit left edge.
    for y in range(3, 29):
        for x in range(8, 24):
            seam = (x - 8) % 4 == 3
            px[x, y] = DOOR_DARK if seam else (DOOR_LIGHT if (x - 8) % 4 == 0 else DOOR)
    for (x, y) in ((21, 16), (21, 17), (20, 16)):
        px[x, y] = KNOB

    # Two boards across, tilted opposite ways, nailed at both ends.
    def board(y_left, y_right):
        for x in range(5, 27):
            t = (x - 5) / 21.0
            yc = round(y_left + (y_right - y_left) * t)
            px[x, yc - 2] = BOARD_EDGE
            px[x, yc - 1] = BOARD
            px[x, yc] = BOARD
            px[x, yc + 1] = BOARD
            px[x, yc + 2] = BOARD_EDGE
        for x in (7, 24):
            t = (x - 5) / 21.0
            yc = round(y_left + (y_right - y_left) * t)
            px[x, yc] = NAIL

    board(8, 13)
    board(23, 18)
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    small = draw_32()
    small.save(OUT / "icon_32.png")
    big = small.resize((256, 256), Image.NEAREST)
    big.save(OUT / "icon_256.png")
    sizes = [(16, 16), (32, 32), (48, 48), (256, 256)]
    images = {
        16: small.resize((16, 16), Image.NEAREST),
        32: small,
        48: small.resize((48, 48), Image.NEAREST),
    }
    big.save(OUT / "dead_pixel.ico", sizes=sizes, append_images=[images[16], images[32], images[48]])
    print("wrote", OUT / "icon_32.png", OUT / "icon_256.png", OUT / "dead_pixel.ico")


if __name__ == "__main__":
    main()
