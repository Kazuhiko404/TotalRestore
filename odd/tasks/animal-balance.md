# Feature: TotalRestore animal balance

Locator: `TotalRestore/odd/tasks/animal-balance.md` · Engram topic `odd/animal-balance/tasks` · branch `feat/totalrestore-balance` (repo root `F:\Proyectos\Kenshi-mods`).

## Objective
Stop TotalRestore from making player-owned animals immortal: animals get neither the Toughness max-HP bonus nor TotalRestore regeneration. Vanilla animal behaviour (native self-heal) is left untouched.

## Problem (2026-10-10, user report)
- A player-owned Bull gained almost double HP: `ApplyToughnessToAnimals` defaults to 1 in `Config.cpp` and the INI.
- The Bull heals faster than it takes damage. The combat pause relies on `Character::isInCombatMode(true, true)`, which does not catch the animal in a fight, so TotalRestore keeps healing it.
- The buffed `_maxHealth` is baked into saves. Turning the bonus off only stops scaling; it never restores the base, so an already buffed animal stays buffed.

## Requirements
1. Animals get no limb max-HP bonus by default (`ApplyToughnessToAnimals=0`).
2. Animals get no TotalRestore healing (wounds, blood, regrowth) by default: new key `ApplyHealingToAnimals=0`.
3. Parts that are not scaled are restored to their captured base on init/load/Toughness XP, so saves with a baked bonus are repaired.
4. Everything stays configurable (1 re-enables the old behaviour).

## Constraints
- Same as `balance-overhaul.md`: v100 toolset, KenshiLib 0.5.0 vendored, isolated SOLID modules, pure rules unit tested, English artifacts, Conventional Commits without AI attribution.
- Deploy after a green build unless `kenshi_x64.exe` is running. When deploying, edit only the affected keys in the installed INI.

## Tasks
| ID | Task | Route | Status | Commit |
|----|------|-------|--------|--------|
| A1 | Exclude animals from healing (`ApplyHealingToAnimals`, default 0) and default `ApplyToughnessToAnimals` to 0 | delegated (writer trigger: 2+ non-trivial files) | done | 8b73064 |
| A2 | Restore unscaled parts to their captured base (pure rule + unit tests + LimbToughness wiring) | delegated (same writer) | done | 7eef604 |
| A3 | Docs: INI comments, README, QA checklist | delegated (same writer) | done | this docs commit |
| R1 | Passive regen only starts below a health threshold (`RegenStartBelowFraction`, default 0.5, per part) | delegated (writer trigger: 2+ non-trivial files) | done | 8352ef7 |
| C1 | DesolationSystem coexistence: watchdog tolerates its flesh clamp (`TolerateExternalFleshClamp`, pure `FleshCapRules` + `ExternalModCompat`) | delegated (writer trigger: 2+ non-trivial files) | done (fallback; full bonus stacking not possible from TotalRestore) | 7d6c0ba |
| C2 | Yield limb `_maxHealth` to DesolationSystem per part (`YieldMaxHealthToOtherMods`, pure `LimbMaxOwnership`) | delegated (writer trigger: 2+ non-trivial files) | done | 5ba9bdc |

## Checks
- Unit tests (`tests/totalrestore_tests.vcxproj`) green, including new rule tests.
- Plugin build 0 errors.
- In-game QA (pending): the Bull's max HP returns to base after load, and it no longer heals beyond vanilla.

## Progress
- 2026-10-10: feature document created. Deploy on hold: Kenshi is running.
- 2026-10-10: A1 done (8b73064). `ApplyHealingToAnimals` (default 0) gates `IsHealingEligible`, which also covers blood, regrowth and the damage cooldown; `ApplyToughnessToAnimals` now defaults to 0 (Config.cpp + INI, with comments). Plugin build 0 errors.
- 2026-10-10: A2 done (7eef604). Pure `LimbMaxRules::TargetLimbMax` (RED: test build failed with C2039 `TargetLimbMax` not a member; GREEN: 291/291 checks, was 286). `ApplyLimbToughness` now writes the setup-captured base back on unscaled parts via `BaseMaxFor` (Find only, never Remember) + shared `WriteLimbMax`; prosthetics have no captured base and stay untouched. Plugin build 0 errors.
- 2026-10-10: A3 done. README (limb toughness + healing sections), QA-CHECKLIST (setup item f, animal checks in sections 1 and 2). REVERSE_ENGINEERING.md had nothing stale. Note: restoration only runs with `EnableLimbToughness=1`. Deploy still on hold (Kenshi running); in-game QA pending.
- 2026-10-10: R1 done (8352ef7) (user request: characters at 70%+ should not heal back to full; high-level characters were effectively immortal). Pure `RegenRules::ShouldStartPartRegen` + `ClampStartBelowFraction` (RED: test build failed with C2039 `ShouldStartPartRegen` not a member; GREEN: 306/306 checks, was 291). `HealBodyParts` skips a part (flesh, stun, wear; both formulas) when `(flesh - fleshStun) / maxHealth()` >= `RegenStartBelowFraction`; `derivedFleshHealthPercent` was not used because its formula is unverified. Blood regen and limb regrowth unchanged. New INI key (HEALING FACTOR section, also in the cheat-setup note), README, QA-CHECKLIST section 2. Plugin build 0 errors. Deploy on hold (Kenshi running); in-game QA pending.
- 2026-10-10: C1 stopped before the hook (task step 5). Static RE of the Steam 1.0.65 exe (docs/REVERSE_ENGINEERING.md, "Native max-health getter and flesh clamp"): `maxHealth()` (RVA 0x296C70) returns `age * _maxHealth * HPMult - wearDamage`; `validateHealthValues` (RVA 0x6455A0) clamps flesh to `[-m, m]` and bandaging to `[0, 2m]` with the same raw `m`, but has no caller (only its own thunk references it). No inlined native site found lowers flesh above the raw max, so the DesolationSystem HP drop needs another explanation. Next step: identify the writer that lowers flesh (static RE of DesolationSystem.dll's `updateStats` / `updateDerivedHealths` / `setup` hooks, or a data breakpoint on one part's `flesh` with Desolation loaded) before choosing the hook point. No source, INI or test changes.
- 2026-10-10: C1 root cause found by static RE of DesolationSystem.dll (docs/REVERSE_ENGINEERING.md, "DesolationSystem.dll max HP and flesh clamp"): its `maxHealth()` hook is a pass-through; its `updateStats` detour runs `ApplyAll`, which writes `_maxHealth = baseline * (1 + bonus)` from a private per-part table filled in `setup`, and then clamps flesh down to `age * HPMult * max - wear` (log counter "D回夹"). Both mods own `_maxHealth`, so stacking both bonuses is not possible without reading Desolation's private table (rejected as unsafe). Fallback implemented: pure `FleshCapRules::IsFleshCapClamp` (RED: test build failed with C1083 `FleshCapRules.h` not found; GREEN: 321/321 checks, was 306), `ExternalModCompat` (detects `DesolationSystem.dll`, retried every 5 s, logs once), the watchdog skips such clamps when `TolerateExternalFleshClamp=1` (default; exact no-op without the DLL). Avoiding TotalRestore's own `updateStats` call was rejected: native calls also run `ApplyAll`, and the call refreshes posture. INI, README, QA-CHECKLIST updated. Plugin build 0 errors. Not deployed (Kenshi may be running); in-game QA pending.
- 2026-10-10: C2 done (user goal: the two mods must stop fighting, keeping both). Pure `LimbMaxOwnership` (per-part record of the max we wrote + prosthetic pointer; foreign when the max changed >= 0.001 on the same limb; bounded at 16384 records) (RED: test build failed with C1083 `LimbMaxOwnership.h` not found; GREEN: 342/342 checks, was 321). `ApplyLimbToughness` skips yielded parts before `BaseMaxFor`, records each write; `setup` and `load` release the record. Gated on `DesolationSystem.dll` + `YieldMaxHealthToOtherMods=1` (not general: TotalRestore's own regrowth writes `_maxHealth` outside LimbToughness and `LimbRegrowth.cpp` was outside the authorized surface). Heal ceiling unchanged: base from the setup cache, clamped to `maxHealth()`. INI, README, QA-CHECKLIST, REVERSE_ENGINEERING updated. Plugin build 0 errors. Not deployed; in-game QA pending.
