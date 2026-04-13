#!/usr/bin/env python3
"""Generate items.json for the Dead Pixel Survival item database."""

import json
import os

ITEMS = [
    # Food items
    {"id": 0,  "name": "Canned Food",    "category": "food",    "weight": 0.5, "maxStack": 5,  "sprite": "item_canned_food",  "hungerRestore": 30.0, "thirstRestore": 0.0,  "spoilHours": 72.0},
    {"id": 1,  "name": "Apple",          "category": "food",    "weight": 0.2, "maxStack": 8,  "sprite": "item_apple",        "hungerRestore": 18.0, "thirstRestore": 5.0,  "spoilHours": 24.0},
    {"id": 2,  "name": "Bread",          "category": "food",    "weight": 0.3, "maxStack": 5,  "sprite": "item_bread",        "hungerRestore": 25.0, "thirstRestore": 0.0,  "spoilHours": 36.0},
    # Drink items
    {"id": 3,  "name": "Water Bottle",   "category": "drink",   "weight": 0.4, "maxStack": 3,  "sprite": "item_water_bottle", "hungerRestore": 0.0,  "thirstRestore": 40.0, "spoilHours": 0.0},
    # Medical
    {"id": 4,  "name": "Bandage",        "category": "medical", "weight": 0.1, "maxStack": 5,  "sprite": "item_bandage",      "hungerRestore": 0.0,  "thirstRestore": 0.0,  "spoilHours": 0.0},
    # Weapons
    {"id": 5,  "name": "Knife",          "category": "weapon",  "weight": 0.8, "maxStack": 1,  "sprite": "item_knife",    "weaponCategory": 1},
    {"id": 6,  "name": "Baseball Bat",   "category": "weapon",  "weight": 1.5, "maxStack": 1,  "sprite": "item_bat",      "weaponCategory": 2},
    {"id": 7,  "name": "Pipe",           "category": "weapon",  "weight": 1.2, "maxStack": 1,  "sprite": "item_pipe",     "weaponCategory": 2},
    {"id": 8,  "name": "Axe",            "category": "weapon",  "weight": 2.0, "maxStack": 1,  "sprite": "item_axe",      "weaponCategory": 1},
    # Building materials
    {"id": 9,  "name": "Wood Planks",    "category": "material","weight": 1.0, "maxStack": 10, "sprite": "item_wood",     "hungerRestore": 0.0, "thirstRestore": 0.0, "spoilHours": 0.0},
    {"id": 10, "name": "Scrap Metal",    "category": "material","weight": 1.5, "maxStack": 8,  "sprite": "item_scrap",    "hungerRestore": 0.0, "thirstRestore": 0.0, "spoilHours": 0.0},
    {"id": 11, "name": "Rope",           "category": "material","weight": 0.5, "maxStack": 5,  "sprite": "item_rope",     "hungerRestore": 0.0, "thirstRestore": 0.0, "spoilHours": 0.0},
    {"id": 12, "name": "Fuel Can",       "category": "material","weight": 2.0, "maxStack": 3,  "sprite": "item_fuel",     "hungerRestore": 0.0, "thirstRestore": 0.0, "spoilHours": 0.0},
    # Trap materials
    {"id": 13, "name": "Glass Shards",   "category": "material","weight": 0.3, "maxStack": 10, "sprite": "item_glass",    "hungerRestore": 0.0, "thirstRestore": 0.0, "spoilHours": 0.0},
]

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    out_dir = os.path.join(project_root, "assets", "data")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "items.json")

    with open(out_path, "w") as f:
        json.dump({"items": ITEMS}, f, indent=2)

    print(f"Generated {len(ITEMS)} items -> {out_path}")

if __name__ == "__main__":
    main()
