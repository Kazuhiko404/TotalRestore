# Feature: TotalRestore balance overhaul

Locator: `TotalRestore/odd/tasks/balance-overhaul.md` · Engram topic `odd/balance-overhaul/tasks` · branch `feat/totalrestore-balance` (repo root `F:\Proyectos\Kenshi-mods`).

## Objective
Make TotalRestore balanced: toughness-scaled passive regeneration with a configurable ceiling, a race-aware capped limb max-HP bonus, a revive window measured from the real death time, a faithful port of HealingFactor limb regrowth, and death protection kept but disabled by default. Everything stays configurable (a "cheat mode" must remain possible through the INI).

## Problem (audit 2026-10-09)
- Death protection floors (+5% vitals, 10% blood, every frame) make player characters immortal; they never even get KO'd.
- Regen rate uses the toughness-buffed max, ignores KO, has no damage watchdog / environmental damage hooks, ignores game speed, and heals robotic parts.
- The limb bonus multiplies inside the `maxHealth()` getter. Native `validateHealthValues` clamps against the raw `_maxHealth`, so the getter buff and the healing target disagree. Saves made with the original LimbToughness already carry a baked `_maxHealth`, which the getter multiplies again (observed 220 -> 530).
- The original HF+LT conflict: HF's refresh calls `MedicalSystem::updateStats` (vtable +8). LT hooks that call and `floorf`s flesh. As a result, vitals get pinned at fractional death floors, the HF watchdog misreads the flooring as damage (5 s pauses), and parts with max < 80 never heal.
- Revive: the time window starts on first selection, not at death, and the state is keyed by a reusable `Character*`.
- Regrowth: none of HF's amputate/RobotLimbs state/updateHiddenParts hooks are ported, so the growing limb is invisible. Bones are reset to 1.0 instead of the captured scale. `g_charStates` is never cleaned.

## User requirements (2026-10-09)
1. Passive regen scales with Toughness and only refills up to a ceiling (default max 50% of max HP at the toughness cap, configurable).
2. The limb max-HP bonus scales with Toughness and is capped (about +30 HP per part at T100). It must be race-aware and **inverted**: races with less base HP get a slightly larger bonus than high-HP races.
3. Revive: keep the time window (e.g. 1 h after death, then permanently dead) and a capped revive HP fraction.
4. Organic limb regrowth works like the original, but the completion HP respects the same ceiling.
5. Skeletons regenerate HP with the same formula and never regrow parts.
6. Remove nothing: legacy behaviour and death protection stay available and are disabled by default where they caused the imbalance.

## Formulas (approved)
- `s = clamp(T, 0, ToughnessCap) / ToughnessCap` (ToughnessCap default 100).
- Regen ceiling: `ceiling = baseMax * lerp(RegenCeilingMinFraction=0.10, RegenCeilingMaxFraction=0.50, s)`.
- Regen speed: `HP/s = baseMax * lerp(RegenRateMinPercentPerMinute=1, RegenRateMaxPercentPerMinute=4, s) / 100 / 60`. Sim time = frameDelta * getFrameSpeedMultiplier().
- `baseMax` is the part max **without** the toughness bonus.
- Limb bonus: `bonus = s * LimbBonusAtReference(30) * (LimbBonusReferenceHealth(100) / baseMax) ^ LimbBonusExponent(0.25)`, clamped to `[0, LimbBonusMaxPerPart(40)]`. Bonus at T100 per base: 80 -> 31.7, 100 -> 30, 200 -> 25.2, 220 -> 24.6.
- `_maxHealth = baseMax + bonus`. The baseline is cached once per (part GameData, race), captured from `HealthPartStatus::setup`. Flesh is rescaled proportionally **without flooring**.

## Constraints
- Toolset v100 (VS2010) via MSBuild 18; KenshiLib 0.5.0 vendored in `TotalRestore/deps`.
- On this Steam exe the KenshiLib header RVAs are not authoritative; always hook through `KenshiLib::GetRealAddress(&fn)`.
- Each concern in its own module (isolated SOLID); pure math in a testable module with unit tests.
- Artifacts in English. Conventional Commits, no AI attribution.
- Deploy after a green build unless `kenshi_x64.exe` is running.

## Tasks
| ID | Task | Route | Status | Commit |
|---|---|---|---|---|
| T1 | Pure `ToughnessScaling` module (s, ceiling, rate, limb bonus) + unit test project | delegated writer (2+ non-trivial files) | done | 38d0a01 |
| T2 | Limb max-HP: baseline from `HPS::setup`, write `_maxHealth = base + bonus`, unfloored flesh rescale; legacy percent formula selectable; re-apply on init/load/toughness XP | delegated writer | done | 533889c |
| T3 | Healing engine: new regen formula with ceiling on baseMax, KO pause, dt * speed multiplier, flesh-drop watchdog, environmental damage hooks, skeleton vs robotic-limb rules, state cleanup on removeFromUpdateListMain, death protection default off; legacy formula selectable | delegated writer | done | 24a93d3 |
| T4 | Revive: death timestamp recorded at death and keyed by handle; healCompletely then scale to ReviveHealthFraction | delegated writer | done | 8ba539a |
| T5 | Limb regrowth faithful port (amputate, RobotLimbs state hooks, updateHiddenParts, bone snapshot restore), completion HP = min(fraction, ceiling) | delegated writer (after HF regrowth spec) | done | 213fc80 + (this commit; see `git log`) |
| T6 | INI (add keys, defaults), README, fix docs/REVERSE_ENGINEERING.md errors, docs/QA-CHECKLIST.md | delegated writer | done | d676c7f |

## Acceptance
- At T106 with base 220, the limb max shows about 244, not 530.
- Passive regen stops at the ceiling (50% at T>=100) and is paused while KO'd, in combat, and for 5 s after damage.
- Player characters can be KO'd and can die with default settings.
- A character dead longer than MaxReviveTimeMinutes cannot be revived; a revived character gets ReviveHealthFraction.
- A severed organic limb visibly regrows; skeleton parts heal but never regrow.
- Unit tests green; plugin builds with 0 errors.

## Progress
- 2026-10-09: audit done (see Problem). Branch created. Formulas approved; the inverted bonus was requested by the user.
- 2026-10-09: T1 done. `src/ToughnessScaling.{h,cpp}` (no game includes) + `tests/totalrestore_tests.vcxproj` (v100, plain CHECK macros). RED against a stub: 19/66 checks passed. GREEN: 66/66. Plugin builds with 0 errors.
- 2026-10-09: T2 done. Pure `src/LimbMaxRules.{h,cpp}` (Scaled/Percent max, inverse estimate, robot/animal filter) and `src/LimbBaselineCache.{h,cpp}` (first insert wins, key = part GameData + race GameData + prosthetic item). `src/LimbToughness.cpp` rewritten: the `maxHealth()` getter hook is gone; hooks `HPS::setup` (baseline from `_max`, race from the running `MedicalSystem::init`), `MedicalSystem::init`, `MedicalSystem::load`, `CharStats::xpStat_eventBased/timeBased` (Toughness only) and writes `_maxHealth = base + bonus` (Scaled) or `base * capped multiplier` (Percent); flesh rescaled without flooring. `updateStats` not hooked. New INI keys `LimbBonusFormula`, `ToughnessCap`, `LimbBonusAtReference`, `LimbBonusReferenceHealth`, `LimbBonusExponent`, `LimbBonusMaxPerPart`; `ApplyToughnessToRobots` default 0. RED against stubs 86/119, GREEN 119/119; plugin 0 errors.
- 2026-10-09: T3 done. Pure `src/RegenRules.{h,cpp}` (sim delta, watchdog threshold, part eligibility, ceiling, layers, legacy rate) and `src/RegenStateRegistry.{h,cpp}` (state keyed by Character* + handle index/serial stamp, so a reused pointer starts clean). `src/HealingEngine.cpp` rewritten around them: Toughness formula on `ResolveLimbBaseMax` (T2 cache, else current max minus the bonus), flesh capped at the ceiling and never lowered, stun/wear uncapped; KO pause (`_NV_isLiterallyUnconciousNotPretending() || PS_KO`); combat pause; watchdog snapshot after our writes; `dt = frame * getFrameSpeedMultiplier()`; skeleton parts heal (race `RaceData::robot`), prosthetics on organics do not; 0.5x limiter only in Legacy; ClearBleeding also zeroes `extraBloodLossFromBodyparts`; death protection default 0. New `src/DamageSignals.cpp` hooks `HPS::applyDamage`, all 7 environmental functions (all present in KenshiLib 0.5.0) and `GameWorld::removeFromUpdateListMain` (state cleanup). Regrowth moved unchanged to `src/LimbRegrowth.cpp` (T5 owns the port). New INI keys `HealingFormula`, `RegenCeilingMin/MaxFraction`, `RegenRateMin/MaxPercentPerMinute`, `PauseHealingWhileUnconscious`, `HealSkeletonParts`, `HealRobotLimbsOnOrganics`; `EnableDeathProtection=0`. RED against stubs 141/186, GREEN 186/186; plugin 0 errors, 0 project warnings.
- 2026-10-09: T4 done. Pure `src/ReviveRules.{h,cpp}` (`DeathClock` keyed by all five `hand` fields, first record wins, future record reset after loading an earlier save; window check; fraction clamp). New `src/DeathTracker.cpp` records the death minute from `Character::declareDead` and from `MedicalSystem::medicalUpdate` (dead flag set after the engine update, so the transition is caught for every updated character); selection keeps a first-observation fallback for deaths before plugin load / in a save. Revive: `dead=false`, `healCompletely()`, then flesh and blood scaled to `ReviveHealthFraction`, record forgotten before `recruit` (recruit changes the handle's platoon). Shared `src/CriticalSection.h` lock (also used by the LimbToughness cache). RED against stubs 195/216, GREEN 216/216; plugin 0 errors.
- 2026-10-09: T6 done. INI: every new key has an English comment, a "cheat setup" note at the top, and stale comments fixed (Legacy-only `WoundRegenerationSeconds`, death protection default 0, revive window from death). README (Spanish) architecture and feature sections rewritten. `docs/REVERSE_ENGINEERING.md` fixed (LimbState 1 = LIMB_STUMP, 2 = LIMB_REPLACED; removed the getter-watchdog and 0.5x claims; added the verified HF + LT `updateStats`/`floorf` conflict). New `docs/QA-CHECKLIST.md` covers every acceptance item. Tests 216/216; plugin 0 errors.
- 2026-10-09: T5 done (spec: scratchpad `hf/REGROWTH-SPEC.md`). Pure `src/RegrowthRules.{h,cpp}` (per-limb settings and clamps, growth curve, completion flesh = min(fraction x max, ceiling) or fraction only for Legacy, mirror snapshot choice, raw-state verdict, root positional size, collapsed-max repair); RED against stubs 231/286, GREEN 286/286 (commit 213fc80). Game port split into `LimbRegrowthState` (entries, shadow count, per-thread visual-depth RAII guard), `LimbBoneSnapshot` (OgreMain exports, capture/mirror/rebind/apply/restore), `LimbShadowHooks` (getState/getLimb/getMask/getLimbState/getRobotLimbState lies, save, part isDead, updateHiddenParts resolved via GetProcAddress, ShadowStumpScope) and `LimbRegrowth` (amputate hook with swallow, DetectStumps, reveal/grow/complete/stop with blood + part fields preserved around setLimb, cleanup on removeFromUpdateListMain and stopPlugin). `MedicalUpdateHook.cpp` is now the single medicalUpdate hook (regrowth scope + DeathTracker); `DamageSignals` applyDamage snapshots limbs before the hit. Humanoids only, skeleton races never regrow, growth pauses with the healing pauses. Plugin 0 errors.
- Next: in-game QA with `docs/QA-CHECKLIST.md` (sections 0-6).
- 2026-10-09: all tasks done (T5 = 213fc80 + eccdabe). Tests 286/286, build 0 errors. Old deploy backed up to the scratchpad and replaced in Kenshi/mods/TotalRestore (dll, ini, RE_Kenshi.json; .mod kept). In-game QA pending: docs/QA-CHECKLIST.md.
