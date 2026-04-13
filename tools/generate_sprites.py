#!/usr/bin/env python3
"""
Isometric sprite generator for Dead Pixel Survival.
Generates all game sprites as isometric pixel art using Pillow.
Output: spritesheet.png + spritesheet.json in assets/sprites/
"""

import json
import math
import os
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

# Atlas dimensions
ATLAS_W = 1024
ATLAS_H = 512

# Tile sizes
ISO_TILE_W = 64
ISO_TILE_H = 32
ENTITY_SIZE = 64   # Characters/zombies
ITEM_SIZE = 32      # Items, env, UI, FX, solids

# Deterministic seed for reproducible results
random.seed(42)


# --- Isometric Diamond Helpers ---

def in_diamond(px, py, w=ISO_TILE_W, h=ISO_TILE_H):
    cx = (w - 1) / 2.0
    cy = (h - 1) / 2.0
    return abs(px - cx) / (w / 2.0) + abs(py - cy) / (h / 2.0) <= 1.0


def diamond_edge(px, py, w=ISO_TILE_W, h=ISO_TILE_H, thickness=1.5):
    cx = (w - 1) / 2.0
    cy = (h - 1) / 2.0
    d = abs(px - cx) / (w / 2.0) + abs(py - cy) / (h / 2.0)
    return 1.0 - thickness / min(w, h) <= d <= 1.0


def fill_diamond(img, ox, oy, base_color, noise_range=15, w=ISO_TILE_W, h=ISO_TILE_H):
    r0, g0, b0 = base_color
    for py in range(h):
        for px in range(w):
            if in_diamond(px, py, w, h):
                nr = random.randint(-noise_range, noise_range)
                r = max(0, min(255, r0 + nr))
                g = max(0, min(255, g0 + nr))
                b = max(0, min(255, b0 + nr))
                img.putpixel((ox + px, oy + py), (r, g, b, 255))


def fill_diamond_edge(img, ox, oy, edge_color, w=ISO_TILE_W, h=ISO_TILE_H):
    for py in range(h):
        for px in range(w):
            if diamond_edge(px, py, w, h):
                img.putpixel((ox + px, oy + py), edge_color)


# --- Tile Generation ---

def gen_tile_grass(img, ox, oy):
    fill_diamond(img, ox, oy, (58, 107, 45), noise_range=18)
    for _ in range(40):
        px = random.randint(0, ISO_TILE_W - 1)
        py = random.randint(0, ISO_TILE_H - 1)
        if in_diamond(px, py):
            shade = random.choice([(72, 128, 55, 255), (50, 95, 38, 255), (65, 115, 48, 255)])
            img.putpixel((ox + px, oy + py), shade)


def gen_tile_grass2(img, ox, oy):
    fill_diamond(img, ox, oy, (52, 98, 40), noise_range=20)
    for _ in range(30):
        px = random.randint(0, ISO_TILE_W - 1)
        py = random.randint(0, ISO_TILE_H - 1)
        if in_diamond(px, py):
            img.putpixel((ox + px, oy + py), (44, 85, 35, 255))


def gen_tile_dirt(img, ox, oy):
    fill_diamond(img, ox, oy, (120, 90, 60), noise_range=15)
    for _ in range(20):
        px = random.randint(2, ISO_TILE_W - 3)
        py = random.randint(2, ISO_TILE_H - 3)
        if in_diamond(px, py):
            img.putpixel((ox + px, oy + py), (95, 72, 50, 255))


def gen_tile_concrete(img, ox, oy):
    fill_diamond(img, ox, oy, (140, 140, 135), noise_range=10)
    for _ in range(3):
        sx = random.randint(16, 48)
        sy = random.randint(4, 28)
        for step in range(random.randint(4, 12)):
            cx = sx + step + random.randint(-1, 1)
            cy = sy + random.randint(-1, 1)
            if 0 <= cx < ISO_TILE_W and 0 <= cy < ISO_TILE_H and in_diamond(cx, cy):
                img.putpixel((ox + cx, oy + cy), (110, 110, 105, 255))


def gen_tile_carpet(img, ox, oy):
    fill_diamond(img, ox, oy, (130, 85, 70), noise_range=8)
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py) and (px + py) % 4 == 0:
                r, g, b, a = img.getpixel((ox + px, oy + py))
                img.putpixel((ox + px, oy + py), (max(0, r - 15), max(0, g - 15), max(0, b - 15), 255))


def gen_tile_gravel(img, ox, oy):
    fill_diamond(img, ox, oy, (125, 115, 100), noise_range=20)
    for _ in range(35):
        px = random.randint(2, ISO_TILE_W - 3)
        py = random.randint(2, ISO_TILE_H - 3)
        if in_diamond(px, py):
            shade = random.randint(80, 140)
            img.putpixel((ox + px, oy + py), (shade, shade - 5, shade - 10, 255))


def gen_tile_metal(img, ox, oy):
    fill_diamond(img, ox, oy, (90, 95, 100), noise_range=8)
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py) and py < 10 and abs(px - 32) < 8:
                r, g, b, a = img.getpixel((ox + px, oy + py))
                img.putpixel((ox + px, oy + py), (min(255, r + 20), min(255, g + 20), min(255, b + 25), 255))


def gen_tile_water(img, ox, oy):
    fill_diamond(img, ox, oy, (35, 85, 145), noise_range=15)
    for _ in range(25):
        px = random.randint(4, ISO_TILE_W - 5)
        py = random.randint(2, ISO_TILE_H - 3)
        if in_diamond(px, py):
            img.putpixel((ox + px, oy + py), (80, 150, 210, 255))


def gen_tile_wall_top(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                if py < ISO_TILE_H // 2:
                    n = random.randint(-5, 5)
                    img.putpixel((ox + px, oy + py), (145 + n, 130 + n, 115 + n, 255))
                else:
                    if px < ISO_TILE_W // 2:
                        n = random.randint(-5, 5)
                        img.putpixel((ox + px, oy + py), (100 + n, 90 + n, 80 + n, 255))
                    else:
                        n = random.randint(-5, 5)
                        img.putpixel((ox + px, oy + py), (120 + n, 108 + n, 95 + n, 255))
    fill_diamond_edge(img, ox, oy, (70, 65, 58, 255))


def gen_tile_wall_face(img, ox, oy):
    fill_diamond(img, ox, oy, (110, 100, 88), noise_range=8)
    for line_y in [8, 16, 24]:
        for px in range(ISO_TILE_W):
            if in_diamond(px, line_y):
                img.putpixel((ox + px, oy + line_y), (85, 78, 68, 255))
    fill_diamond_edge(img, ox, oy, (65, 60, 52, 255))


def gen_tile_door_closed(img, ox, oy):
    fill_diamond(img, ox, oy, (100, 70, 45), noise_range=10)
    for py in range(4, ISO_TILE_H - 4):
        for px in range(20, 44):
            if in_diamond(px, py):
                n = random.randint(-3, 3)
                img.putpixel((ox + px, oy + py), (85 + n, 55 + n, 30 + n, 255))
    if in_diamond(38, 16):
        img.putpixel((ox + 38, oy + 16), (180, 170, 80, 255))
    fill_diamond_edge(img, ox, oy, (60, 42, 25, 255))


def gen_tile_door_open(img, ox, oy):
    fill_diamond(img, ox, oy, (55, 55, 50), noise_range=5)
    for py in range(4, ISO_TILE_H - 4):
        if in_diamond(10, py):
            img.putpixel((ox + 10, oy + py), (100, 70, 45, 255))
            img.putpixel((ox + 11, oy + py), (90, 62, 38, 255))


def gen_tile_solid_white(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                img.putpixel((ox + px, oy + py), (255, 255, 255, 255))


def gen_tile_solid_dark(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                img.putpixel((ox + px, oy + py), (20, 20, 20, 255))


def gen_tile_window(img, ox, oy):
    fill_diamond(img, ox, oy, (110, 100, 88), noise_range=5)
    for py in range(8, 24):
        for px in range(22, 42):
            if in_diamond(px, py):
                img.putpixel((ox + px, oy + py), (130, 180, 220, 180))
    fill_diamond_edge(img, ox, oy, (65, 60, 52, 255))


def gen_tile_fence(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                if px in (16, 32, 48) and py > 4:
                    img.putpixel((ox + px, oy + py), (120, 95, 60, 255))
                elif py in (10, 20) and 14 <= px <= 50:
                    img.putpixel((ox + px, oy + py), (110, 85, 50, 255))


# --- Character Generation ---

def draw_iso_character(img, ox, oy, body_color, head_color, leg_color,
                       is_dead=False, is_hurt=False, is_crouch=False,
                       walk_frame=0, facing='right'):
    size = ENTITY_SIZE
    cx = size // 2

    if is_dead:
        for py in range(28, 36):
            for px in range(16, 48):
                n = random.randint(-5, 5)
                r, g, b = body_color
                img.putpixel((ox + px, oy + py), (r + n, g + n, b + n, 255))
        for py in range(30, 36):
            for px in range(12, 18):
                r, g, b = head_color
                img.putpixel((ox + px, oy + py), (r, g, b, 255))
        return

    body_h = 14 if not is_crouch else 10
    body_top = 22 if not is_crouch else 30
    head_top = body_top - 10 if not is_crouch else body_top - 7
    leg_top = body_top + body_h
    leg_h = 14 if not is_crouch else 8

    leg_shift = 0
    body_bob = 0
    if walk_frame > 0:
        phase = [0, 2, 0, -2][walk_frame % 4]
        leg_shift = phase
        body_bob = abs(phase) // 2

    left_leg_x = cx - 5 + leg_shift
    right_leg_x = cx + 2 - leg_shift
    for py in range(leg_top - body_bob, leg_top + leg_h - body_bob):
        for dx in range(4):
            for lx in [left_leg_x + dx, right_leg_x + dx]:
                if 0 <= lx < size and 0 <= py < size:
                    r, g, b = leg_color
                    n = random.randint(-5, 5)
                    img.putpixel((ox + lx, oy + py), (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)), 255))

    for py in range(body_top - body_bob, body_top + body_h - body_bob):
        for px in range(cx - 6, cx + 7):
            if 0 <= py < size:
                r, g, b = body_color
                n = random.randint(-8, 8)
                shade = -15 if px > cx else 0
                img.putpixel((ox + px, oy + py), (
                    max(0, min(255, r + n + shade)),
                    max(0, min(255, g + n + shade)),
                    max(0, min(255, b + n + shade)), 255))

    # --- Arms ---
    arm_swing = 0
    if walk_frame > 0:
        arm_swing = [0, 3, 0, -3][walk_frame % 4]

    arm_top = body_top + 2 - body_bob
    arm_len = 10 if not is_crouch else 7
    left_arm_x = cx - 8
    right_arm_x = cx + 7

    for i in range(arm_len):
        ay = arm_top + i + (arm_swing if i > arm_len // 2 else 0)
        # Left arm
        if 0 <= left_arm_x < size and 0 <= ay < size:
            r, g, b = body_color
            n = random.randint(-5, 5)
            img.putpixel((ox + left_arm_x, oy + ay), (max(0, min(255, r + n - 10)), max(0, min(255, g + n - 10)), max(0, min(255, b + n - 10)), 255))
            img.putpixel((ox + left_arm_x + 1, oy + ay), (max(0, min(255, r + n - 10)), max(0, min(255, g + n - 10)), max(0, min(255, b + n - 10)), 255))
        # Right arm
        if 0 <= right_arm_x < size and 0 <= ay < size:
            r, g, b = body_color
            n = random.randint(-5, 5)
            img.putpixel((ox + right_arm_x, oy + ay), (max(0, min(255, r + n - 15)), max(0, min(255, g + n - 15)), max(0, min(255, b + n - 15)), 255))
            img.putpixel((ox + right_arm_x - 1, oy + ay), (max(0, min(255, r + n - 15)), max(0, min(255, g + n - 15)), max(0, min(255, b + n - 15)), 255))

    # --- Hands (skin-colored circles at arm ends) ---
    hand_y = arm_top + arm_len - 1 + (arm_swing if arm_len > 3 else 0)
    for hdx in range(-1, 2):
        for hdy in range(-1, 2):
            if hdx * hdx + hdy * hdy <= 1:
                hpx_l = left_arm_x + hdx
                hpx_r = right_arm_x + hdx
                hpy = hand_y + hdy
                if 0 <= hpx_l < size and 0 <= hpy < size:
                    r, g, b = head_color
                    n = random.randint(-5, 5)
                    img.putpixel((ox + hpx_l, oy + hpy), (max(0, min(255, r + n - 20)), max(0, min(255, g + n - 20)), max(0, min(255, b + n - 20)), 255))
                if 0 <= hpx_r < size and 0 <= hpy < size:
                    r, g, b = head_color
                    n = random.randint(-5, 5)
                    img.putpixel((ox + hpx_r, oy + hpy), (max(0, min(255, r + n - 20)), max(0, min(255, g + n - 20)), max(0, min(255, b + n - 20)), 255))

    # --- Head ---
    head_cy = head_top + 4 - body_bob
    for py in range(head_top - body_bob, head_top + 9 - body_bob):
        for px in range(cx - 4, cx + 5):
            dy = py - head_cy
            dxx = px - cx
            if dxx * dxx / 20.0 + dy * dy / 18.0 <= 1.0:
                if 0 <= py < size:
                    r, g, b = head_color
                    n = random.randint(-3, 3)
                    img.putpixel((ox + px, oy + py), (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)), 255))

    eye_y = head_cy - 1 - body_bob
    if 0 <= eye_y < size:
        img.putpixel((ox + cx - 2, oy + eye_y), (30, 30, 30, 255))
        img.putpixel((ox + cx + 1, oy + eye_y), (30, 30, 30, 255))

    if is_hurt:
        for py in range(head_top - body_bob, leg_top + leg_h - body_bob):
            for px in range(cx - 7, cx + 8):
                if 0 <= px < size and 0 <= py < size:
                    pixel = img.getpixel((ox + px, oy + py))
                    if pixel[3] > 0:
                        img.putpixel((ox + px, oy + py), (
                            min(255, pixel[0] + 60), max(0, pixel[1] - 20),
                            max(0, pixel[2] - 20), pixel[3]))


def gen_player_sprites(img, atlas_sprites, row_y):
    body = (60, 65, 75)
    head = (210, 180, 150)
    legs = (50, 50, 55)

    x = 0
    frames_walk_right = []
    for f in range(4):
        draw_iso_character(img, x, row_y, body, head, legs, walk_frame=f, facing='right')
        frames_walk_right.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
        x += ENTITY_SIZE
    atlas_sprites["player_walk_right"] = {
        "x": 0, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE,
        "frames": frames_walk_right
    }

    frames_walk_down = []
    sx = x
    for f in range(4):
        draw_iso_character(img, x, row_y, body, head, legs, walk_frame=f, facing='down')
        frames_walk_down.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
        x += ENTITY_SIZE
    atlas_sprites["player_walk_down"] = {
        "x": sx, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE,
        "frames": frames_walk_down
    }

    frames_walk_up = []
    sx = x
    for f in range(4):
        draw_iso_character(img, x, row_y, body, head, legs, walk_frame=f, facing='up')
        frames_walk_up.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
        x += ENTITY_SIZE
    atlas_sprites["player_walk_up"] = {
        "x": sx, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE,
        "frames": frames_walk_up
    }

    draw_iso_character(img, x, row_y, body, head, legs)
    atlas_sprites["player_idle"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE

    draw_iso_character(img, x, row_y, body, head, legs, is_hurt=True)
    atlas_sprites["player_hurt"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE

    draw_iso_character(img, x, row_y, body, head, legs, is_dead=True)
    atlas_sprites["player_dead"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE

    draw_iso_character(img, x, row_y, body, head, legs, is_crouch=True)
    atlas_sprites["player_crouch"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE


def gen_zombie_sprites(img, atlas_sprites, row_y):
    body = (75, 95, 65)
    head = (130, 160, 110)
    legs = (60, 55, 50)

    x = 0
    frames = []
    for f in range(4):
        draw_iso_character(img, x, row_y, body, head, legs, walk_frame=f)
        frames.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
        x += ENTITY_SIZE
    atlas_sprites["zombie_walk_right"] = {
        "x": 0, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE, "frames": frames
    }

    frames = []
    sx = x
    for f in range(4):
        draw_iso_character(img, x, row_y, body, head, legs, walk_frame=f)
        frames.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
        x += ENTITY_SIZE
    atlas_sprites["zombie_walk_down"] = {
        "x": sx, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE, "frames": frames
    }

    frames = []
    sx = x
    for f in range(4):
        draw_iso_character(img, x, row_y, body, head, legs, walk_frame=f)
        frames.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
        x += ENTITY_SIZE
    atlas_sprites["zombie_walk_up"] = {
        "x": sx, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE, "frames": frames
    }

    draw_iso_character(img, x, row_y, body, head, legs)
    atlas_sprites["zombie_idle"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE

    draw_iso_character(img, x, row_y, body, head, legs)
    for py in range(row_y + 26, row_y + 32):
        for px in range(x + 38, x + 48):
            if px < ATLAS_W and py < ATLAS_H:
                img.putpixel((px, py), (75, 95, 65, 255))
    atlas_sprites["zombie_attack"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE

    draw_iso_character(img, x, row_y, body, head, legs, is_dead=True)
    atlas_sprites["zombie_dead"] = {"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE}
    x += ENTITY_SIZE


# --- Item Generation ---

def draw_small_rect(img, ox, oy, x1, y1, x2, y2, color):
    for py in range(y1, y2):
        for px in range(x1, x2):
            n = random.randint(-3, 3)
            r, g, b = color
            img.putpixel((ox + px, oy + py), (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)), 255))


def gen_item_sprite(img, ox, oy, name):
    if name == "item_canned_food":
        draw_small_rect(img, ox, oy, 10, 8, 22, 24, (160, 50, 40))
        draw_small_rect(img, ox, oy, 10, 8, 22, 11, (180, 180, 180))
        draw_small_rect(img, ox, oy, 12, 14, 20, 18, (200, 180, 60))
    elif name == "item_apple":
        for py in range(8, 24):
            for px in range(10, 22):
                dy = py - 16
                dx = px - 16
                if dx * dx + dy * dy < 50:
                    n = random.randint(-8, 8)
                    img.putpixel((ox + px, oy + py), (max(0, 180 + n), max(0, 30 + n), max(0, 30 + n), 255))
        img.putpixel((ox + 16, oy + 8), (80, 50, 20, 255))
        img.putpixel((ox + 16, oy + 7), (80, 50, 20, 255))
    elif name == "item_bread":
        draw_small_rect(img, ox, oy, 8, 14, 24, 22, (190, 160, 90))
        draw_small_rect(img, ox, oy, 9, 12, 23, 15, (210, 180, 110))
    elif name == "item_water_bottle":
        draw_small_rect(img, ox, oy, 12, 6, 20, 26, (60, 120, 200))
        draw_small_rect(img, ox, oy, 13, 4, 19, 7, (180, 180, 180))
        for py in range(10, 20):
            img.putpixel((ox + 14, oy + py), (100, 160, 230, 255))
    elif name == "item_bandage":
        draw_small_rect(img, ox, oy, 10, 10, 22, 22, (230, 225, 215))
        draw_small_rect(img, ox, oy, 14, 10, 18, 22, (200, 50, 40))
        draw_small_rect(img, ox, oy, 10, 14, 22, 18, (200, 50, 40))
    elif name == "item_knife":
        for i in range(14):
            img.putpixel((ox + 16 + i, oy + 15 - i // 2), (180, 185, 190, 255))
            img.putpixel((ox + 16 + i, oy + 16 - i // 2), (160, 165, 170, 255))
        draw_small_rect(img, ox, oy, 10, 16, 17, 20, (90, 55, 30))
    elif name == "item_bat":
        for i in range(20):
            img.putpixel((ox + 6 + i, oy + 22 - i), (130, 95, 55, 255))
            img.putpixel((ox + 7 + i, oy + 22 - i), (120, 85, 45, 255))
        draw_small_rect(img, ox, oy, 24, 2, 28, 8, (130, 95, 55))
    elif name == "item_pipe":
        for i in range(22):
            img.putpixel((ox + 5 + i, oy + 20 - i), (120, 120, 125, 255))
            img.putpixel((ox + 6 + i, oy + 20 - i), (100, 100, 105, 255))
    elif name == "item_plank":
        draw_small_rect(img, ox, oy, 6, 10, 26, 16, (160, 130, 80))
        draw_small_rect(img, ox, oy, 6, 16, 26, 22, (140, 110, 65))
    elif name == "item_axe":
        for i in range(18):
            img.putpixel((ox + 8 + i, oy + 22 - i), (120, 85, 45, 255))
        draw_small_rect(img, ox, oy, 24, 2, 30, 10, (150, 155, 160))
    elif name == "item_sleeping_bag":
        draw_small_rect(img, ox, oy, 6, 12, 26, 22, (60, 80, 130))
        draw_small_rect(img, ox, oy, 6, 12, 10, 22, (50, 70, 115))
    elif name == "item_campfire":
        for angle in range(0, 360, 45):
            px = int(16 + 8 * math.cos(math.radians(angle)))
            py = int(16 + 6 * math.sin(math.radians(angle)))
            draw_small_rect(img, ox, oy, px - 1, py - 1, px + 2, py + 2, (100, 95, 85))
        for offset in [(15, 14), (16, 12), (17, 14), (16, 16)]:
            img.putpixel((ox + offset[0], oy + offset[1]), (220, 150, 40, 255))
        img.putpixel((ox + 16, oy + 13), (240, 80, 30, 255))
    elif name == "item_noise_trap":
        draw_small_rect(img, ox, oy, 10, 12, 22, 22, (140, 140, 145))
        draw_small_rect(img, ox, oy, 12, 8, 20, 13, (180, 175, 50))
    elif name == "item_barricade":
        for i in range(24):
            y1 = 4 + i
            if y1 < 28:
                img.putpixel((ox + 4 + i, oy + y1), (140, 110, 65, 255))
                img.putpixel((ox + 27 - i, oy + y1), (130, 100, 55, 255))
    elif name == "item_ammo":
        draw_small_rect(img, ox, oy, 10, 12, 22, 22, (150, 130, 50))
        for px in range(12, 20, 3):
            draw_small_rect(img, ox, oy, px, 8, px + 2, 13, (180, 160, 80))
    elif name == "item_medkit":
        draw_small_rect(img, ox, oy, 8, 10, 24, 24, (220, 220, 220))
        draw_small_rect(img, ox, oy, 14, 12, 18, 22, (200, 40, 40))
        draw_small_rect(img, ox, oy, 10, 15, 22, 19, (200, 40, 40))
    elif name == "item_wood":
        draw_small_rect(img, ox, oy, 8, 8, 14, 26, (145, 115, 70))
        draw_small_rect(img, ox, oy, 18, 10, 24, 24, (135, 105, 60))
    elif name == "item_scrap":
        for _ in range(8):
            px = random.randint(8, 24)
            py = random.randint(8, 24)
            draw_small_rect(img, ox, oy, px, py, px + 4, py + 3, (130, 130, 135))
    elif name == "item_rope":
        for angle in range(0, 360, 15):
            px = int(16 + 7 * math.cos(math.radians(angle)))
            py = int(16 + 5 * math.sin(math.radians(angle)))
            img.putpixel((ox + px, oy + py), (160, 140, 90, 255))
    elif name == "item_fuel":
        draw_small_rect(img, ox, oy, 10, 8, 22, 24, (180, 50, 45))
        draw_small_rect(img, ox, oy, 14, 4, 18, 9, (120, 120, 125))
    elif name == "item_glass":
        draw_small_rect(img, ox, oy, 10, 10, 22, 24, (180, 210, 230))
        for py in range(12, 22):
            img.putpixel((ox + 12, oy + py), (200, 230, 245, 200))
    else:
        draw_small_rect(img, ox, oy, 8, 8, 24, 24, (150, 150, 150))


# --- Environment Sprites ---

def gen_env_sprite(img, ox, oy, name):
    if name == "env_grave":
        draw_small_rect(img, ox, oy, 12, 8, 20, 12, (140, 140, 135))
        draw_small_rect(img, ox, oy, 10, 12, 22, 24, (130, 130, 125))
        draw_small_rect(img, ox, oy, 14, 14, 18, 20, (110, 110, 105))
    elif name == "env_grave2":
        draw_small_rect(img, ox, oy, 11, 10, 21, 24, (120, 120, 115))
        draw_small_rect(img, ox, oy, 13, 6, 19, 11, (130, 130, 125))
    elif name == "env_blood_small":
        for _ in range(12):
            px = random.randint(10, 22)
            py = random.randint(12, 24)
            img.putpixel((ox + px, oy + py), (150, 20, 20, 200))
    elif name == "env_blood_large":
        for _ in range(40):
            px = random.randint(6, 26)
            py = random.randint(8, 26)
            a = random.randint(140, 220)
            img.putpixel((ox + px, oy + py), (155 + random.randint(-20, 20), 15, 15, a))
    elif name == "env_crate":
        draw_small_rect(img, ox, oy, 8, 10, 24, 24, (150, 120, 70))
        for y in [14, 18, 22]:
            for px in range(8, 24):
                img.putpixel((ox + px, oy + y), (120, 95, 55, 255))
    elif name == "env_shelf":
        draw_small_rect(img, ox, oy, 6, 8, 26, 24, (130, 105, 65))
        draw_small_rect(img, ox, oy, 6, 14, 26, 16, (110, 88, 50))
    elif name == "env_bed":
        draw_small_rect(img, ox, oy, 6, 10, 26, 24, (70, 80, 130))
        draw_small_rect(img, ox, oy, 6, 10, 12, 16, (200, 195, 185))
    elif name == "env_campfire_lit":
        gen_item_sprite(img, ox, oy, "item_campfire")
        for _ in range(12):
            px = random.randint(13, 19)
            py = random.randint(10, 16)
            c = random.choice([(255, 200, 50), (255, 140, 30), (255, 80, 20)])
            img.putpixel((ox + px, oy + py), (*c, 255))
    elif name == "env_barricade":
        gen_item_sprite(img, ox, oy, "item_barricade")
    elif name == "env_noise_trap":
        gen_item_sprite(img, ox, oy, "item_noise_trap")
    elif name == "env_campfire":
        gen_item_sprite(img, ox, oy, "item_campfire")
    elif name == "env_sleeping_area":
        draw_small_rect(img, ox, oy, 6, 12, 26, 24, (55, 70, 120))
        draw_small_rect(img, ox, oy, 6, 12, 10, 18, (190, 185, 175))
    else:
        draw_small_rect(img, ox, oy, 8, 8, 24, 24, (100, 100, 100))


# --- UI Icons ---

def gen_ui_sprite(img, ox, oy, name):
    if name == "ui_heart":
        for py in range(8, 24):
            for px in range(8, 24):
                dx = px - 16
                dy = py - 16
                in_heart = (dx * dx + (dy - 2) * (dy - 2) - 36) ** 3 - dx * dx * (dy - 2) ** 3 < 0
                if in_heart:
                    img.putpixel((ox + px, oy + py), (200, 50, 50, 255))
    elif name == "ui_stamina":
        points = [(18, 6), (14, 14), (18, 14), (14, 26), (20, 16), (16, 16), (20, 6)]
        draw = ImageDraw.Draw(img)
        poly = [(ox + p[0], oy + p[1]) for p in points]
        draw.polygon(poly, fill=(50, 200, 80, 255))
    elif name == "ui_hunger":
        for py in range(6, 26):
            img.putpixel((ox + 16, oy + py), (200, 180, 60, 255))
        for px in [12, 14, 16, 18, 20]:
            for py in range(6, 12):
                img.putpixel((ox + px, oy + py), (200, 180, 60, 255))
    elif name == "ui_thirst":
        for py in range(8, 24):
            width = max(0, min(6, (py - 8) // 2))
            for px in range(16 - width, 16 + width + 1):
                img.putpixel((ox + px, oy + py), (60, 140, 220, 255))
    elif name == "ui_sleep":
        for sx, sy in [(10, 10), (16, 14), (22, 18)]:
            for dx in range(6):
                img.putpixel((ox + sx + dx, oy + sy), (150, 130, 200, 255))
                img.putpixel((ox + sx + dx, oy + sy + 4), (150, 130, 200, 255))
            img.putpixel((ox + sx + 5, oy + sy + 1), (150, 130, 200, 255))
            img.putpixel((ox + sx, oy + sy + 3), (150, 130, 200, 255))
    elif name == "ui_clock":
        for angle in range(0, 360, 10):
            px = int(16 + 8 * math.cos(math.radians(angle)))
            py = int(16 + 8 * math.sin(math.radians(angle)))
            img.putpixel((ox + px, oy + py), (200, 200, 180, 255))
        for px, py in [(16, 12), (16, 14), (20, 16), (18, 16)]:
            img.putpixel((ox + px, oy + py), (200, 200, 180, 255))
    elif name == "ui_skull":
        for py in range(8, 20):
            for px in range(10, 22):
                dy = py - 14
                dx = px - 16
                if dx * dx / 40.0 + dy * dy / 35.0 <= 1.0:
                    img.putpixel((ox + px, oy + py), (220, 215, 200, 255))
        for px, py in [(13, 13), (14, 13), (18, 13), (19, 13)]:
            img.putpixel((ox + px, oy + py), (40, 40, 40, 255))
        for px in range(13, 20):
            img.putpixel((ox + px, oy + 20), (200, 195, 180, 255))
    else:
        draw_small_rect(img, ox, oy, 8, 8, 24, 24, (200, 200, 200))


# --- FX Sprites ---

def gen_fx_sprite(img, ox, oy, name, frame=0):
    if name == "fx_slash":
        for angle in range(-30 + frame * 10, 30 + frame * 10, 5):
            r = 4 + frame * 2
            px = int(16 + r * math.cos(math.radians(angle)))
            py = int(16 + r * math.sin(math.radians(angle)))
            if 0 <= px < 32 and 0 <= py < 32:
                alpha = max(60, 255 - frame * 50)
                img.putpixel((ox + px, oy + py), (255, 255, 220, alpha))
    elif name == "fx_impact":
        for angle in range(0, 360, 30):
            r = 3 + frame * 4
            px = int(16 + r * math.cos(math.radians(angle)))
            py = int(16 + r * math.sin(math.radians(angle)))
            if 0 <= px < 32 and 0 <= py < 32:
                img.putpixel((ox + px, oy + py), (255, 230, 150, 200))
    elif name == "fx_blood":
        count = 8 + frame * 6
        for _ in range(count):
            spread = 4 + frame * 4
            px = 16 + random.randint(-spread, spread)
            py = 16 + random.randint(-spread, spread)
            if 0 <= px < 32 and 0 <= py < 32:
                img.putpixel((ox + px, oy + py), (180 + random.randint(-30, 30), 15, 15, 220))
    elif name == "fx_footstep":
        count = 3 + frame * 3
        for _ in range(count):
            px = 16 + random.randint(-3 - frame * 2, 3 + frame * 2)
            py = 20 + random.randint(-2 - frame, 2 + frame)
            if 0 <= px < 32 and 0 <= py < 32:
                g = 140 + random.randint(-20, 20)
                img.putpixel((ox + px, oy + py), (g, g - 10, g - 20, max(10, 150 - frame * 40)))


# --- Solid/Utility Sprites ---

def gen_solid(img, ox, oy, color):
    for py in range(32):
        for px in range(32):
            img.putpixel((ox + px, oy + py), (*color, 255))


# --- Main Atlas Assembly ---

def generate_atlas():
    img = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    sprites = {}

    # Row 0: Tiles (y=0, h=32, each 64x32)
    tile_funcs = [
        ("tile_grass", gen_tile_grass),
        ("tile_grass2", gen_tile_grass2),
        ("tile_dirt", gen_tile_dirt),
        ("tile_concrete", gen_tile_concrete),
        ("tile_carpet", gen_tile_carpet),
        ("tile_gravel", gen_tile_gravel),
        ("tile_metal", gen_tile_metal),
        ("tile_water", gen_tile_water),
        ("tile_wall_top", gen_tile_wall_top),
        ("tile_wall_face", gen_tile_wall_face),
        ("tile_door_closed", gen_tile_door_closed),
        ("tile_door_open", gen_tile_door_open),
        ("tile_solid_white", gen_tile_solid_white),
        ("tile_solid_dark", gen_tile_solid_dark),
        ("tile_window", gen_tile_window),
        ("tile_fence", gen_tile_fence),
    ]

    for i, (name, func) in enumerate(tile_funcs):
        x = i * ISO_TILE_W
        func(img, x, 0)
        sprites[name] = {"x": x, "y": 0, "w": ISO_TILE_W, "h": ISO_TILE_H}

    # Row 1: Player (y=32, h=64)
    gen_player_sprites(img, sprites, 32)

    # Row 2: Zombies (y=96, h=64)
    gen_zombie_sprites(img, sprites, 96)

    # Row 3: Items (y=160, h=32)
    items_row = [
        "item_canned_food", "item_apple", "item_bread", "item_water_bottle",
        "item_bandage", "item_knife", "item_bat", "item_pipe",
        "item_plank", "item_axe", "item_sleeping_bag", "item_campfire",
        "item_noise_trap", "item_barricade", "item_ammo", "item_medkit",
    ]
    for i, name in enumerate(items_row):
        x = i * ITEM_SIZE
        gen_item_sprite(img, x, 160, name)
        sprites[name] = {"x": x, "y": 160, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Row 4: Environment + extra items (y=192, h=32)
    env_row = [
        "env_grave", "env_grave2", "env_blood_small", "env_blood_large",
        "env_crate", "env_shelf", "env_bed", "env_campfire_lit",
        "env_barricade", "env_noise_trap", "env_campfire", "env_sleeping_area",
        "item_wood", "item_scrap", "item_rope", "item_fuel",
    ]
    for i, name in enumerate(env_row):
        x = i * ITEM_SIZE
        if name.startswith("env_"):
            gen_env_sprite(img, x, 192, name)
        else:
            gen_item_sprite(img, x, 192, name)
        sprites[name] = {"x": x, "y": 192, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Row 5: UI icons + misc (y=224, h=32)
    ui_row = [
        "ui_heart", "ui_stamina", "ui_hunger", "ui_thirst",
        "ui_sleep", "ui_clock", "ui_skull", "item_glass",
    ]
    for i, name in enumerate(ui_row):
        x = i * ITEM_SIZE
        if name.startswith("ui_"):
            gen_ui_sprite(img, x, 224, name)
        else:
            gen_item_sprite(img, x, 224, name)
        sprites[name] = {"x": x, "y": 224, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Row 6: FX (y=256, h=32)
    fx_defs = [
        ("fx_slash", 4),
        ("fx_impact", 2),
        ("fx_blood", 2),
        ("fx_footstep", 2),
    ]
    fx_x = 0
    for name, frame_count in fx_defs:
        frames_list = []
        start_x = fx_x
        for f in range(frame_count):
            gen_fx_sprite(img, fx_x, 256, name, f)
            frames_list.append({"x": fx_x, "y": 256, "w": ITEM_SIZE, "h": ITEM_SIZE})
            fx_x += ITEM_SIZE
        entry = {"x": start_x, "y": 256, "w": ITEM_SIZE, "h": ITEM_SIZE}
        if frame_count > 1:
            entry["frames"] = frames_list
        sprites[name] = entry

    # Row 7: Solids + tints (y=288, h=32)
    solids = [
        ("solid_white", (255, 255, 255)),
        ("solid_black", (0, 0, 0)),
        ("solid_red", (200, 40, 40)),
        ("solid_green", (40, 180, 60)),
        ("solid_blue", (40, 80, 200)),
        ("solid_yellow", (220, 200, 50)),
        ("solid_orange", (220, 140, 40)),
        ("solid_purple", (140, 60, 180)),
        ("tint_night", (10, 15, 40)),
        ("tint_dusk", (60, 40, 50)),
        ("tint_dark", (5, 5, 10)),
    ]
    for i, (name, color) in enumerate(solids):
        x = i * ITEM_SIZE
        gen_solid(img, x, 288, color)
        sprites[name] = {"x": x, "y": 288, "w": ITEM_SIZE, "h": ITEM_SIZE}

    return img, sprites


def generate_tileset():
    """Generate the isometric tileset image (placeholder.png) for tile map rendering.

    Layout: 4 columns x 4 rows of 64x32 cells = 256x128 image.
    GID mapping (firstGid=1, so localId = gid - 1):
      localId 0 (GID 1): empty/unused
      localId 1 (GID 2 = kGidFloor): concrete floor
      localId 2 (GID 3 = kGidGravel): gravel
      localId 3 (GID 4): dirt
      localId 4 (GID 5 = kGidWater): water
      localId 5 (GID 6): carpet
      localId 6 (GID 7 = kGidWall): wall
      localId 7 (GID 8): metal
      localId 8 (GID 9 = kGidGrass): grass
      localId 9-15: variants/unused
    """
    ts_cols = 4
    ts_rows = 4
    ts_w = ts_cols * ISO_TILE_W   # 256
    ts_h = ts_rows * ISO_TILE_H   # 128
    img = Image.new("RGBA", (ts_w, ts_h), (0, 0, 0, 0))

    tile_generators = {
        1: gen_tile_concrete,   # GID 2 = kGidFloor
        2: gen_tile_gravel,     # GID 3 = kGidGravel
        3: gen_tile_dirt,       # GID 4
        4: gen_tile_water,      # GID 5 = kGidWater
        5: gen_tile_carpet,     # GID 6
        6: gen_tile_wall_top,   # GID 7 = kGidWall
        7: gen_tile_metal,      # GID 8
        8: gen_tile_grass,      # GID 9 = kGidGrass
        9: gen_tile_grass2,     # GID 10
        10: gen_tile_fence,     # GID 11
        11: gen_tile_door_closed,  # GID 12
        12: gen_tile_door_open,    # GID 13
        13: gen_tile_window,       # GID 14
        14: gen_tile_solid_white,  # GID 15
        15: gen_tile_solid_dark,   # GID 16
    }

    for local_id, gen_func in tile_generators.items():
        col = local_id % ts_cols
        row = local_id // ts_cols
        ox = col * ISO_TILE_W
        oy = row * ISO_TILE_H
        gen_func(img, ox, oy)

    return img


def main():
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    sprites_dir = project_root / "assets" / "sprites"
    sprites_dir.mkdir(parents=True, exist_ok=True)

    # Generate spritesheet atlas
    print("Generating isometric sprite atlas...")
    img, sprites = generate_atlas()

    png_path = sprites_dir / "spritesheet.png"
    json_path = sprites_dir / "spritesheet.json"

    img.save(str(png_path))
    print(f"  Saved {png_path} ({img.size[0]}x{img.size[1]})")

    atlas_meta = {
        "imageWidth": ATLAS_W,
        "imageHeight": ATLAS_H,
        "sprites": sprites,
    }

    with open(str(json_path), "w") as f:
        json.dump(atlas_meta, f, indent=2)
    print(f"  Saved {json_path} ({len(sprites)} sprites)")

    # Generate tileset for map rendering
    print("Generating isometric tileset...")
    tileset_img = generate_tileset()
    tileset_path = sprites_dir / "placeholder.png"
    tileset_img.save(str(tileset_path))
    print(f"  Saved {tileset_path} ({tileset_img.size[0]}x{tileset_img.size[1]})")

    print(f"Done! {len(sprites)} sprites + tileset generated.")


if __name__ == "__main__":
    main()
