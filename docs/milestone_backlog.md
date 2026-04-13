# Dead Pixel Survival — Milestone Backlog

**Version:** 1.0
**Created:** 2026-04-11
**Status:** Active

---

## Milestone Definitions

### M0 — Planning and Scope Lock ✅
**Deliverables:** Milestone backlog (this document), World Foundation Refactor spec, runtime pipeline decision, save-model decision, save migration policy.
**Exit criterion:** No remaining ambiguity about whether the game is fixed-world-per-slot.

**Decisions locked:**
- **Runtime pipeline:** Generator-first. Procedural generation at runtime. Tiled is demoted to tooling/template reference only.
- **Save model:** Fixed-world-per-slot. World layout is generated once at slot creation and reused for all runs in that slot.
- **Save migration:** Version-reject. Old saves without a `saveVersion` field are treated as incompatible. A fresh slot is created. Old files are preserved on disk but not loaded.

---

### M1 — World Foundation Refactor ✅
**Deliverables:** Stable layout per slot, separate run variance, district data model, correct grave/structure placement on stable geography, retirement path separated from death path.
**Exit criterion:** Multiple runs in one slot share identical geography while spawns vary.

---

### M2 — Prototype Stabilization ✅
**Deliverables:** Cleaner interaction prompts, container UX improvements, structure repair/dismantle loops, run-state checkpointing (sleep, district transition, timed), initial balancing instrumentation.
**Exit criterion:** 5–10 seeded playtests produce useful data instead of confusion.

---

### M3 — District-Aware Survival Loop ✅
**Deliverables:** District-weighted loot/spawn logic, medical/industrial/resource identity per district, early world-memory hooks.
**Exit criterion:** Districts feel meaningfully different and affect player decisions.

---

### M4 — NPC Survivor v0 ✅
**Deliverables:** Active-district NPC simulation, simple personality axes, observe/approach/trade interaction loop, NPC persistence hooks.
**Exit criterion:** NPCs create pressure and opportunity without collapsing stability.

---

### M5 — Psychology and Social Integration ✅
**Deliverables:** Refined panic/depression, 24-hour depression-crisis retirement window, Hardened unlock path, social recovery hooks tied to NPCs.
**Exit criterion:** Psychology is readable and systemic, not just numerical decay.

---

### M6 — Weather, Seasons, and Infection v0 ✅
**Deliverables:** Season temperature loop, rain/storms, temperature pressure on needs, district infection spread, overwhelmed states, early zombie aging.
**Exit criterion:** Time passing changes the strategic map.

---

### M7 — Crafting and Expanded Base Loop
**Deliverables:** Two-item discovery crafting, notebook memory, expanded utility structures, stronger world-memory linkage.
**Exit criterion:** Crafting and base decisions open new survival routes.

---

### M8 — Vehicles and Fire
**Deliverables:** Narrow vehicle slice (sedan, fuel, condition, storage, noise), narrow fire slice (flammability, spread, suppression), integrated with noise/weather/persistence.
**Exit criterion:** Both systems add decisions without breaking save/world model.

---

### M9 — Full Legacy and World History
**Deliverables:** Bloodline traits, retirement markers, richer history artifacts, deeper world memory.
**Exit criterion:** Inter-run identity is obvious and valuable.

---

### M10 — Modes, Threat Framing, and UI Overhaul
**Deliverables:** Campaign mode, Endless mode, threat modes, tone presets, minimap/world map, notebook UI, contextual panels, polished death/retirement screens.
**Exit criterion:** The game matches its intended strategic and presentation surface.

---

### M11 — Hardening and Release Readiness
**Deliverables:** Full documentation sync, regression coverage, performance tuning, save compatibility policy, final economy/spawn tuning.
**Exit criterion:** Future work builds on a stable base.

---

## Dependency Graph

```
M0 → M1 → M2 → M3 → M4 → M5
                 ↓         ↓
                M3 → M6 → M7
                      ↓
                     M8 → M9 → M10 → M11
```

Key constraints:
- M1 blocks everything else. Do not expand systems before the world foundation is correct.
- M5 (psychology) requires M4 (NPCs) for social recovery hooks.
- M6 (weather/infection) requires M3 (districts) for meaningful spatial effects.
- M8 (vehicles/fire) requires M6 (weather) and M7 (crafting/base) to be stable.
- M10 (UI/modes) waits until feature surface has mostly stabilized.
- M11 (hardening) is terminal — depends on all prior milestones.

---

## Execution Rules

1. Never start a milestone before its dependencies pass their exit criteria.
2. Cut breadth before cutting architecture if schedule pressure appears.
3. Each milestone ends with a verification pass against its exit criterion.
4. Do not add combat breadth, new buildables, or flashy systems before M2.
5. Vehicles and fire start as narrow slices, not full GDD breadth.
