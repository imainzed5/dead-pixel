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
ATLAS_H = 1024

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
        draw_small_rect(img, ox, oy, 8, 4, 24, 28, (120, 90, 50))
        for y in [8, 14, 20]:
            draw_small_rect(img, ox, oy, 6, y, 26, y + 2, (100, 75, 40))
        # Add random clutter on shelves
        for y in [8, 14, 20]:
            if random.random() > 0.3:
                cx = random.randint(10, 20)
                cc = random.choice([(180, 50, 40), (60, 120, 200), (180, 180, 180), (220, 220, 220)])
                draw_small_rect(img, ox, oy, cx, y - 4, cx + 4, y, cc)
    elif name == "env_bed":
        # Wooden frame
        draw_small_rect(img, ox, oy, 4, 8, 28, 28, (110, 85, 45))
        # Mattress
        draw_small_rect(img, ox, oy, 6, 12, 26, 26, (220, 220, 215))
        # Blanket
        draw_small_rect(img, ox, oy, 6, 16, 26, 26, (80, 100, 160))
        # Pillow
        draw_small_rect(img, ox, oy, 8, 10, 24, 14, (240, 240, 240))
    elif name == "env_chair":
        # Seat
        draw_small_rect(img, ox, oy, 10, 16, 22, 20, (130, 105, 65))
        # Backrest
        draw_small_rect(img, ox, oy, 10, 10, 22, 16, (110, 88, 50))
        # Legs
        draw_small_rect(img, ox, oy, 10, 20, 12, 26, (90, 70, 40))
        draw_small_rect(img, ox, oy, 20, 20, 22, 26, (90, 70, 40))
    elif name == "env_table":
        # Table top
        draw_small_rect(img, ox, oy, 6, 12, 26, 18, (140, 115, 75))
        # Legs
        draw_small_rect(img, ox, oy, 8, 18, 10, 26, (100, 80, 50))
        draw_small_rect(img, ox, oy, 22, 18, 24, 26, (100, 80, 50))
    elif name == "env_cabinet":
        # Frame
        draw_small_rect(img, ox, oy, 8, 6, 24, 26, (120, 95, 55))
        # Doors
        draw_small_rect(img, ox, oy, 10, 8, 15, 24, (100, 75, 40))
        draw_small_rect(img, ox, oy, 17, 8, 22, 24, (100, 75, 40))
        # Handles
        draw_small_rect(img, ox, oy, 14, 14, 15, 18, (200, 200, 200))
        draw_small_rect(img, ox, oy, 17, 14, 18, 18, (200, 200, 200))
    elif name == "env_couch":
        # Frame/Base
        draw_small_rect(img, ox, oy, 4, 14, 28, 22, (90, 120, 90))
        # Backrest
        draw_small_rect(img, ox, oy, 6, 8, 26, 14, (70, 100, 70))
        # Armrests
        draw_small_rect(img, ox, oy, 4, 12, 8, 20, (80, 110, 80))
        draw_small_rect(img, ox, oy, 24, 12, 28, 20, (80, 110, 80))
        # Cushions
        draw_small_rect(img, ox, oy, 8, 14, 16, 20, (100, 130, 100))
        draw_small_rect(img, ox, oy, 16, 14, 24, 20, (100, 130, 100))
    elif name == "env_lamp":
        # Base
        draw_small_rect(img, ox, oy, 14, 24, 18, 26, (80, 80, 80))
        # Pole
        draw_small_rect(img, ox, oy, 15, 10, 17, 24, (100, 100, 100))
        # Shade
        draw_small_rect(img, ox, oy, 12, 6, 20, 12, (240, 230, 200))
        # Light glow (if not off)
        img.putpixel((ox + 16, oy + 11), (255, 255, 200, 255))
        img.putpixel((ox + 15, oy + 11), (255, 255, 200, 255))
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
        "env_chair", "env_table", "env_cabinet", "env_couch", "env_lamp",
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

    # Rows 8+: structures (walls, doorways, roofs).
    add_structure_sprites(img, sprites)
    add_legacy_props(img, sprites)

    return img, sprites


# ---------------------------------------------------------------------------
# Exact 2:1 diamonds, ground textures and structures (walls, doorways, roofs).
#
# Everything below uses its own random.Random instances, so the older atlas
# content above stays byte-identical when this file changes.
# ---------------------------------------------------------------------------

WALL_H = 56       # full wall height in pixels at zoom 1 (a little taller than a person)
WALL_LOW_H = 8    # cut-away stub drawn when a wall would hide the player
TILE_VARIANTS = 4 # rows of variants per ground tile in the tileset


def diamond_span(y):
    """Inclusive x range of row y (0..31) in an exact 64x32 diamond.

    Rows are 4k+2 pixels wide, so diamonds placed at (+-32, +-16) offsets
    tile with no gaps and no overlap.
    """
    k = y if y < 16 else 31 - y
    return 31 - 2 * k, 32 + 2 * k


def diamond_bottom(x):
    """Lowest diamond row that contains column x (1..62)."""
    xm = x if x <= 31 else 63 - x
    k = (32 - xm) // 2
    return 31 - k


def tile_world(px, py):
    """Local world coordinates (0..32) of diamond pixel (px, py)."""
    wx = py + (px - 31.5) / 2.0
    wy = py - (px - 31.5) / 2.0
    return wx, wy


def clamp8(v):
    return max(0, min(255, int(round(v))))


def mix(a, b, t):
    return tuple(clamp8(a[i] + (b[i] - a[i]) * t) for i in range(3))


def shade(c, d):
    return (clamp8(c[0] + d), clamp8(c[1] + d), clamp8(c[2] + d))


def put(img, x, y, rgb, a=255):
    img.putpixel((x, y), (rgb[0], rgb[1], rgb[2], a))


def for_diamond(fn):
    for py in range(32):
        x0, x1 = diamond_span(py)
        for px in range(x0, x1 + 1):
            fn(px, py)


# --- Ground textures -------------------------------------------------------
# Each takes (img, ox, oy, rng) and paints one 64x32 diamond. Patterns are laid
# out in tile-local world coordinates so they continue across tile borders.

def tex_grass(img, ox, oy, rng, base=(64, 112, 48)):
    def px_fn(px, py):
        n = rng.randint(-10, 10)
        put(img, ox + px, oy + py, shade(base, n))
    for_diamond(px_fn)
    # Blades: short vertical strokes, lighter on top.
    for _ in range(70):
        py = rng.randint(2, 29)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        light = rng.random() < 0.55
        col = shade(base, 22 if light else -18)
        put(img, ox + px, oy + py, col)
        if py > 0:
            a0, a1 = diamond_span(py - 1)
            if a0 <= px <= a1:
                put(img, ox + px, oy + py - 1, shade(col, 10 if light else -6))
    # A few flowers/weeds.
    for _ in range(rng.randint(0, 2)):
        py = rng.randint(4, 27)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0 + 2, x1 - 2) if x1 - x0 > 4 else x0
        put(img, ox + px, oy + py, rng.choice([(196, 188, 92), (180, 150, 190), (210, 210, 200)]))


def tex_grass_dark(img, ox, oy, rng):
    tex_grass(img, ox, oy, rng, base=(58, 104, 45))


def tex_dirt(img, ox, oy, rng):
    base = (116, 88, 62)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-9, 9)))
    for_diamond(px_fn)
    for _ in range(26):
        py = rng.randint(1, 30)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        put(img, ox + px, oy + py, shade(base, rng.choice([-24, -16, 14])))


def tex_gravel(img, ox, oy, rng):
    base = (122, 116, 104)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-14, 14)))
    for_diamond(px_fn)
    for _ in range(40):
        py = rng.randint(1, 30)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        v = rng.randint(-34, 26)
        put(img, ox + px, oy + py, shade(base, v))


def tex_asphalt(img, ox, oy, rng):
    base = (70, 72, 76)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-6, 6)))
    for_diamond(px_fn)
    for _ in range(22):
        py = rng.randint(1, 30)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        put(img, ox + px, oy + py, shade(base, rng.choice([-12, 10, 16])))
    # Occasional crack.
    if rng.random() < 0.35:
        py = rng.randint(6, 24)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0 + 4, max(x0 + 4, x1 - 10))
        for step in range(rng.randint(5, 11)):
            cx = px + step
            cy = py + (step // 3) * rng.choice([-1, 0, 1])
            if 0 <= cy < 32:
                a0, a1 = diamond_span(cy)
                if a0 <= cx <= a1:
                    put(img, ox + cx, oy + cy, shade(base, -22))


def tex_sidewalk(img, ox, oy, rng):
    base = (152, 150, 142)
    slab = 16.0
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-5, 5))
        fx = wx % slab
        fy = wy % slab
        if fx < 0.75 or fy < 0.75:
            c = shade(base, -26)
        elif fx < 1.5 or fy < 1.5:
            c = shade(base, 8)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_concrete(img, ox, oy, rng):
    base = (138, 138, 132)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-5, 5))
        if wx < 0.6 or wy < 0.6:
            c = shade(base, -14)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)
    for _ in range(rng.randint(0, 3)):
        py = rng.randint(4, 27)
        x0, x1 = diamond_span(py)
        put(img, ox + rng.randint(x0, x1), oy + py, shade(base, -18))


def tex_tile_floor(img, ox, oy, rng):
    light = (196, 192, 178)
    dark = (150, 146, 136)
    cell = 8.0
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        checker = (int(wx // cell) + int(wy // cell)) % 2
        c = light if checker == 0 else dark
        c = shade(c, rng.randint(-3, 3))
        if wx % cell < 0.6 or wy % cell < 0.6:
            c = shade(c, -18)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_wood_floor(img, ox, oy, rng):
    plank_w = 6.4  # five planks per tile, running along world x
    tones = [(128, 88, 56), (138, 96, 60), (120, 82, 52), (134, 92, 58), (124, 86, 54)]
    rng.shuffle(tones)
    joints = [rng.uniform(4.0, 28.0) for _ in tones]
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        i = min(len(tones) - 1, max(0, int(wy // plank_w)))
        c = shade(tones[i], rng.randint(-4, 4))
        if wy % plank_w < 0.7:
            c = shade(c, -30)
        elif abs(wx - joints[i]) < 0.6:
            c = shade(c, -24)
        elif (int(wx * 1.7 + i * 5) % 9) == 0:
            c = shade(c, -8)  # grain
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_carpet(img, ox, oy, rng):
    base = (112, 62, 58)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-5, 5))
        if (int(wx) + int(wy)) % 4 == 0:
            c = shade(c, -10)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_metal_floor(img, ox, oy, rng):
    base = (102, 108, 114)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-3, 3))
        # Diamond plate: short raised dashes on a grid.
        gx = wx % 6.0
        gy = wy % 6.0
        if 1.0 < gx < 3.4 and 1.4 < gy < 2.4:
            c = shade(base, 22)
        elif 1.0 < gx < 3.4 and 2.4 <= gy < 3.0:
            c = shade(base, -16)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_water(img, ox, oy, rng):
    base = (40, 84, 128)
    phase = rng.uniform(0, 6.28)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        wave = math.sin((wx + wy) * 0.55 + phase) + 0.5 * math.sin(wx * 0.9 - phase)
        c = shade(base, int(wave * 7) + rng.randint(-3, 3))
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)
    for _ in range(6):
        py = rng.randint(4, 27)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0 + 2, max(x0 + 2, x1 - 4))
        for d in range(rng.randint(2, 4)):
            if px + d <= x1:
                put(img, ox + px + d, oy + py, (120, 168, 200))


def tex_wall_base(img, ox, oy, rng):
    # Ground under wall blocks; covered by the block, kept for the minimap.
    base = (88, 82, 74)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-4, 4)))
    for_diamond(px_fn)


def tex_solid(color):
    def fn(img, ox, oy, rng):
        def px_fn(px, py):
            put(img, ox + px, oy + py, color)
        for_diamond(px_fn)
    return fn


# GID -> texture. GIDs 11-14 were unused by the map and now hold new floors.
GROUND_TEXTURES = {
    2: tex_concrete,          # kGidFloor
    3: tex_gravel,            # kGidGravel
    4: tex_dirt,              # kGidDirt
    5: tex_water,             # kGidWater
    6: tex_carpet,            # kGidCarpet
    7: tex_wall_base,         # kGidWall (under wall blocks)
    8: tex_metal_floor,       # kGidMetal
    9: tex_grass,             # kGidGrass
    10: tex_grass_dark,       # kGidGrass2
    11: tex_wood_floor,       # kGidWood
    12: tex_tile_floor,       # kGidTileFloor
    13: tex_asphalt,          # kGidAsphalt
    14: tex_sidewalk,         # kGidSidewalk
    15: tex_solid((255, 255, 255)),
    16: tex_solid((20, 20, 20)),
}


def generate_tileset():
    """Ground tileset (placeholder.png): 16 columns (GID 1..16) x 4 variant rows.

    GID g, variant v lives at column g-1, row v, i.e. local id v*16 + g-1.
    The renderer picks a variant per tile from a hash of its position.
    """
    cols = 16
    img = Image.new("RGBA", (cols * ISO_TILE_W, TILE_VARIANTS * ISO_TILE_H), (0, 0, 0, 0))
    for gid, fn in GROUND_TEXTURES.items():
        for v in range(TILE_VARIANTS):
            rng = random.Random(gid * 1009 + v * 7919)
            fn(img, (gid - 1) * ISO_TILE_W, v * ISO_TILE_H, rng)
    return img


# --- Structures ------------------------------------------------------------

WALL_MATERIALS = {
    # name: (left face, right face, top cap, accent)
    "plaster": ((184, 168, 140), (146, 132, 108), (92, 86, 80), (120, 108, 90)),
    "brick": ((156, 82, 62), (122, 64, 50), (88, 82, 78), (176, 160, 140)),
    "metal": ((128, 138, 146), (98, 106, 114), (150, 156, 160), (84, 90, 96)),
}

ROOF_COLORS = {
    "residential": (98, 66, 58),
    "commercial": (92, 92, 96),
    "industrial": (128, 136, 142),
}


def face_color(material, left, u, h, height, rng):
    """Color of a wall face pixel. u runs along the face (0..31), h is the
    height above the ground (0 at the bottom)."""
    lc, rc, cap, accent = WALL_MATERIALS[material]
    c = lc if left else rc
    c = shade(c, rng.randint(-3, 3))
    if material == "plaster":
        # Horizontal siding boards with a dark baseboard and light top trim.
        if h < 3:
            return shade(c, -34)
        if h >= height - 2:
            return shade(c, 16)
        if h % 7 == 0:
            return shade(c, -16)
        if h % 7 == 1:
            return shade(c, 6)
        return c
    if material == "brick":
        course = h // 4
        if h < 2:
            return shade(c, -30)
        if h % 4 == 0:
            return accent if left else shade(accent, -26)
        offset = 4 if course % 2 else 0
        if (u + offset) % 8 == 0:
            return accent if left else shade(accent, -26)
        return shade(c, (course * 7 % 5) - 2)
    # metal: vertical corrugation
    if h < 2:
        return shade(c, -30)
    if u % 4 == 0:
        return shade(c, 16)
    if u % 4 == 2:
        return shade(c, -14)
    return c


DOOR_WOOD = (122, 84, 50)


def door_panel(u, h, left):
    """A closed wooden door filling the doorway (u 10..21, h 0..43)."""
    c = DOOR_WOOD
    if u % 4 == 1:
        c = shade(c, -16)  # gaps between vertical planks
    elif u % 4 == 3:
        c = shade(c, 8)
    if h in (10, 11, 32, 33):
        c = shade(DOOR_WOOD, -24)  # cross rails
    if u in (19, 20) and 20 <= h <= 22:
        c = (204, 172, 86)  # handle
    if not left:
        c = shade(c, -18)
    return c


def door_remains(u, h, left):
    """Splintered boards left on the hinges; None where it is broken through."""
    jag = (u * 7 + h * 3) % 5
    keep = u <= 11 + (1 if jag == 0 else 0)          # hinge-side stiles
    keep = keep or h <= 3 + (u * 5) % 4               # stump of the bottom rail
    keep = keep or (31 <= h <= 33 and u <= 14 + jag)  # a rail snapped off mid-way
    if not keep:
        return None
    c = DOOR_WOOD if jag else shade(DOOR_WOOD, 26)    # pale fresh splinters
    if u % 4 == 1:
        c = shade(c, -16)
    if not left:
        c = shade(c, -18)
    return c


def make_block(material, height, feature=None, seed=0):
    """A wall block: top cap diamond plus the two faces the camera sees.

    feature: None, "window_l", "window_r", "door_l", "door_r", "doorclosed_l",
    "doorclosed_r", "doorbroken_l", "doorbroken_r". The suffix says which face
    carries it (l = left/south face, r = right/east face). "door" is an open
    doorway; "doorclosed" holds a wooden door; "doorbroken" is what is left of
    one after the dead got through.
    """
    rng = random.Random(seed)
    lc, rc, cap, accent = WALL_MATERIALS[material]
    img = Image.new("RGBA", (ISO_TILE_W, ISO_TILE_H + height), (0, 0, 0, 0))

    for px in range(1, 63):
        left = px <= 31
        u = px - 1 if left else 62 - px
        yb = diamond_bottom(px)
        for h in range(height):
            y = yb + height - h
            c = face_color(material, left, u, h, height, rng)
            alpha = 255
            if feature is not None and feature.endswith("_l" if left else "_r"):
                if feature.startswith("window"):
                    if 9 <= u <= 22 and 20 <= h <= 42 and height >= 40:
                        if u in (9, 22) or h in (20, 42) or u == 15 or h == 31:
                            c = (210, 204, 190) if material != "metal" else (170, 176, 180)
                        else:
                            glass = (58, 76, 92) if left else (46, 60, 74)
                            # Simple reflection streak.
                            if (u - h // 2) % 9 in (0, 1):
                                glass = shade(glass, 30)
                            c = glass
                elif feature.startswith("door"):
                    if 9 <= u <= 22 and h <= 44:
                        if u in (9, 22) or h == 44:
                            c = shade(accent, -10) if material != "brick" else (92, 86, 80)
                        elif feature.startswith("doorclosed"):
                            c = door_panel(u, h, left)
                        elif feature.startswith("doorbroken"):
                            c = door_remains(u, h, left)
                            if c is None:
                                alpha = 0
                        else:
                            alpha = 0  # open doorway: see straight through
            if alpha:
                put(img, px, y, c, alpha)

    # Top cap (drawn last so it sits over the faces' top rows).
    for py in range(32):
        x0, x1 = diamond_span(py)
        for px in range(x0, x1 + 1):
            # No outline: neighbouring caps merge into one continuous wall top.
            put(img, px, py, shade(cap, rng.randint(-3, 3)))
    return img


def make_roof(kind, seed=0):
    rng = random.Random(seed)
    base = ROOF_COLORS[kind]
    img = Image.new("RGBA", (ISO_TILE_W, ISO_TILE_H), (0, 0, 0, 0))
    for py in range(32):
        x0, x1 = diamond_span(py)
        for px in range(x0, x1 + 1):
            wx, wy = tile_world(px, py)
            c = shade(base, rng.randint(-4, 4))
            if kind == "residential":
                # Shingle rows along world x, staggered tabs.
                row = int(wy // 4)
                if wy % 4 < 0.8:
                    c = shade(base, -18)
                elif (int(wx + (row % 2) * 3) % 6) == 0:
                    c = shade(base, -10)
                else:
                    c = shade(c, (row % 3) * 3 - 3)
            elif kind == "commercial":
                if rng.random() < 0.08:
                    c = shade(base, rng.choice([-16, 14]))
                if wx % 16 < 0.6 or wy % 16 < 0.6:
                    c = shade(base, -10)  # seams in the membrane
            else:
                if int(wy) % 4 == 0:
                    c = shade(base, 14)
                elif int(wy) % 4 == 2:
                    c = shade(base, -12)
            put(img, px, py, c)
    return img


def add_structure_sprites(img, sprites):
    """Wall blocks, doorways and roofs, placed below the older atlas rows."""
    y_full = 320
    x = 0
    seed = 1
    for material in ("plaster", "brick", "metal"):
        for feature, suffix in ((None, ""), ("window_l", "_window_l"), ("window_r", "_window_r"),
                                ("door_l", "_door_l"), ("door_r", "_door_r")):
            block = make_block(material, WALL_H, feature, seed)
            seed += 1
            img.paste(block, (x, y_full), block)
            sprites["wall_" + material + suffix] = {"x": x, "y": y_full, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_H}
            x += ISO_TILE_W

    y_low = y_full + ISO_TILE_H + WALL_H
    x = 0
    for material in ("plaster", "brick", "metal"):
        block = make_block(material, WALL_LOW_H, None, seed)
        seed += 1
        img.paste(block, (x, y_low), block)
        sprites["wall_" + material + "_low"] = {"x": x, "y": y_low, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_LOW_H}
        x += ISO_TILE_W
    for kind in ("residential", "commercial", "industrial"):
        roof = make_roof(kind, seed)
        seed += 1
        img.paste(roof, (x, y_low), roof)
        sprites["roof_" + kind] = {"x": x, "y": y_low, "w": ISO_TILE_W, "h": ISO_TILE_H}
        x += ISO_TILE_W

    # Doors: closed and broken, per material and face. Below the legacy props.
    y_doors = 512
    x = 0
    for material in ("plaster", "brick", "metal"):
        for feature in ("doorclosed_l", "doorclosed_r", "doorbroken_l", "doorbroken_r"):
            block = make_block(material, WALL_H, feature, seed)
            seed += 1
            img.paste(block, (x, y_doors), block)
            sprites["wall_" + material + "_" + feature] = {"x": x, "y": y_doors, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_H}
            x += ISO_TILE_W


# --- Legacy props ------------------------------------------------------------
# 64x64 cells drawn like characters: the renderer puts the bottom centre of the
# cell on the tile centre, so each prop's footprint sits along the bottom.

PROP = 64


def ground_ellipse(img, cx, cy, rx, ry, color_fn):
    """Fill an iso-flattened ellipse; color_fn(dx, dy, t) -> (rgb, alpha) or None."""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx = (x + 0.5 - cx) / rx
            dy = (y + 0.5 - cy) / ry
            t = dx * dx + dy * dy
            if t <= 1.0 and 0 <= x < PROP and 0 <= y < PROP:
                c = color_fn(dx, dy, t)
                if c is not None:
                    put(img, x, y, c[0], c[1])


def soft_shadow(img, cx, cy, rx, ry, alpha=70):
    def fn(dx, dy, t):
        return ((10, 12, 10), int(alpha * (1.0 - t) ** 0.6))
    ground_ellipse(img, cx, cy, rx, ry, fn)


def dirt_mound(img, rng, cx, cy, rx, ry, base, height=5):
    """A low mound: iso ellipse with a lit top-left and a darker front lip."""
    soft_shadow(img, cx + 2, cy + 2, rx + 3, ry + 2)

    def fn(dx, dy, t):
        light = -dx * 10 - dy * 16
        n = rng.randint(-9, 9)
        c = shade(base, light + n)
        if rng.random() < 0.07:
            c = shade(c, 22)  # pebbles
        return (c, 255)
    # body (raised): draw the front lip first, then the top shifted up
    def lip(dx, dy, t):
        if dy < 0.1:
            return None
        return (shade(base, -26 + rng.randint(-6, 6)), 255)
    ground_ellipse(img, cx, cy, rx, ry, lip)
    ground_ellipse(img, cx, cy - height * 0.5, rx, ry, fn)


def headstone(img, rng, x0, y_base, w, h, stone=(132, 134, 136)):
    """Upright slab with a rounded top, a lit front and a darker east side."""
    side = 4
    top_r = w // 2
    for y in range(y_base - h, y_base):
        for x in range(x0, x0 + w + side):
            # rounded top
            ry = y - (y_base - h)
            if ry < top_r:
                cxr = x0 + w / 2.0
                if (x + 0.5 - cxr) ** 2 + (top_r - ry - 0.5) ** 2 > top_r ** 2 and x < x0 + w:
                    continue
                if x >= x0 + w and ry < top_r - 2:
                    continue
            if x < x0 + w:
                c = shade(stone, 10 - ry * 0.5 + rng.randint(-5, 5))
                if x == x0:
                    c = shade(c, 14)
            else:
                # side face, sheared up by the iso angle
                if y - (x - x0 - w) // 2 < y_base - h + top_r - 1:
                    continue
                c = shade(stone, -34 + rng.randint(-4, 4))
            put(img, x, y, c)
    # carved lines
    for i, yy in enumerate((y_base - h + top_r + 3, y_base - h + top_r + 6, y_base - h + top_r + 9)):
        if yy >= y_base - 3:
            break
        inset = 3 if i else 2
        for x in range(x0 + inset, x0 + w - inset):
            put(img, x, yy, shade(stone, -30))
    # moss
    for _ in range(6):
        x = x0 + rng.randint(0, w - 1)
        y = y_base - rng.randint(1, 5)
        put(img, x, y, (78, 104, 58))


def wooden_cross(img, x_mid, y_base, h):
    wood = (118, 86, 54)
    dark = shade(wood, -32)
    for y in range(y_base - h, y_base):
        put(img, x_mid, y, wood)
        put(img, x_mid + 1, y, wood)
        put(img, x_mid + 2, y, dark)
    arm_y = y_base - h + 5
    for x in range(x_mid - 6, x_mid + 9):
        put(img, x, arm_y, shade(wood, 10))
        put(img, x, arm_y + 1, wood)
        put(img, x, arm_y + 2, dark)


def outline_opaque(img, color, alpha_min=200):
    """Darken opaque pixels that touch transparency, for a readable silhouette."""
    w, h = img.size
    src = img.copy()
    for y in range(h):
        for x in range(w):
            r, g, b, a = src.getpixel((x, y))
            if a < alpha_min:
                continue
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if not (0 <= nx < w and 0 <= ny < h) or src.getpixel((nx, ny))[3] < 60:
                    img.putpixel((x, y), (color[0], color[1], color[2], 255))
                    break


def make_prop(kind, seed):
    rng = random.Random(9000 + seed)
    img = Image.new("RGBA", (PROP, PROP), (0, 0, 0, 0))
    cx = PROP / 2.0
    if kind == "grave":
        dirt_mound(img, rng, cx + 1, 54, 17, 7, (92, 72, 50), height=4)
        headstone(img, rng, 22, 50, 15, 22)
        # a few grass tufts at the base
        for _ in range(10):
            x = rng.randint(14, 48)
            y = rng.randint(55, 61)
            put(img, x, y, (70, 112, 52))
            put(img, x, y - 1, (84, 128, 60))
    elif kind == "grave_cross":
        dirt_mound(img, rng, cx + 1, 54, 17, 7, (70, 52, 36), height=5)
        wooden_cross(img, 30, 52, 24)
        # a small stone or two on the fresh soil
        for (x, y) in ((24, 55), (38, 53)):
            put(img, x, y, (150, 150, 146))
            put(img, x + 1, y, (128, 128, 124))
    elif kind == "pack":
        soft_shadow(img, cx + 2, 57, 15, 5, 90)
        body = (138, 100, 62)
        # main bag lying on its back, seen from the iso angle
        for y in range(40, 58):
            for x in range(19, 45):
                fx = (x - 32) / 13.0
                fy = (y - 49) / 9.0
                if fx * fx + fy * fy > 1.0:
                    continue
                c = shade(body, -fx * 10 - fy * 18 + rng.randint(-5, 5))
                put(img, x, y, c)
        # top flap
        for y in range(38, 46):
            for x in range(23, 41):
                fx = (x - 32) / 9.0
                fy = (y - 42) / 4.5
                if fx * fx + fy * fy <= 1.0:
                    put(img, x, y, shade(body, 14 - fy * 8 + rng.randint(-4, 4)))
        # straps and buckle
        for y in range(43, 57):
            put(img, 27, y, (58, 50, 36))
            put(img, 37, y, (58, 50, 36))
        for x in (26, 27, 28):
            put(img, x, 50, (176, 160, 104))
        for x in (36, 37, 38):
            put(img, x, 50, (176, 160, 104))
        # side pocket
        for y in range(48, 55):
            for x in range(42, 47):
                put(img, x, y, shade(body, -18 + rng.randint(-3, 3)))
        outline_opaque(img, (52, 38, 26))
        # a red tag so it reads as someone's belongings, not a rock
        for (x, y) in ((40, 41), (41, 42), (41, 41), (42, 43)):
            put(img, x, y, (186, 54, 44))
    elif kind == "cairn":
        soft_shadow(img, cx + 2, 56, 14, 5, 80)
        stones = [(32, 54, 11, 5, (128, 126, 120)), (31, 47, 9, 4, (142, 140, 134)),
                  (33, 41, 7, 3.5, (120, 120, 116)), (32, 35.5, 5, 3, (150, 148, 142))]
        for sx, sy, rx, ry, col in stones:
            def fn(dx, dy, t, col=col):
                return (shade(col, -dx * 12 - dy * 22 + rng.randint(-5, 5)), 255)
            ground_ellipse(img, sx, sy, rx, ry, fn)
        # a strip of cloth tied around the top stone
        for x in range(28, 37):
            put(img, x, 36, (150, 60, 52))
        put(img, 37, 37, (150, 60, 52))
        put(img, 38, 38, (130, 52, 46))
    return img


def add_legacy_props(img, sprites):
    """Graves, a dropped pack and a retirement cairn, below the structure rows."""
    y = 448
    x = 0
    for seed, kind in enumerate(("grave", "grave_cross", "pack", "cairn")):
        prop = make_prop(kind, seed)
        img.paste(prop, (x, y), prop)
        sprites["prop_" + kind] = {"x": x, "y": y, "w": PROP, "h": PROP}
        x += PROP

    # A torn journal page lying flat (32x32, drawn at pickup size).
    note = Image.new("RGBA", (ITEM_SIZE, ITEM_SIZE), (0, 0, 0, 0))
    rng = random.Random(77)
    paper = (214, 204, 176)
    for py in range(10, 26):
        for px in range(4, 28):
            # a parallelogram lying on the iso ground
            if abs((px - 16) * 0.5 + (py - 18)) > 6 or abs(px - 16 - (py - 18) * 0.6) > 11:
                continue
            put(note, px, py, shade(paper, rng.randint(-8, 6)))
    for py in range(12, 25, 3):
        for px in range(6, 26):
            if note.getpixel((px, py))[3] and rng.random() < 0.7:
                put(note, px, py, (110, 104, 96))
    outline_opaque(note, (96, 86, 70))
    img.paste(note, (x, y), note)
    sprites["prop_note"] = {"x": x, "y": y, "w": ITEM_SIZE, "h": ITEM_SIZE}


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
