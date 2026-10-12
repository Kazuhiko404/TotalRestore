# Technical Reverse Engineering & Hook Architecture

This document records the disassembly, memory layout, and function hooks reverse-engineered from:
1. `HealingFactor.dll` (Steam ID: 3770919479)
2. `LimbToughness.dll` (Steam ID: 3787222853)
3. `KenshiRevive` (by SilbinaryWolf)

All engine addresses are resolved at runtime with `KenshiLib::GetRealAddress(&Class::fn)`: the RVAs printed in the KenshiLib 0.5.0 headers are not authoritative on the current Steam executable. For virtual methods, use the `_NV_` variant only when no subclass overrides the method (an `_NV_` thunk skips virtual dispatch).

---

## 1. The HealingFactor + LimbToughness conflict (verified)

Running both original mods together pins vitals and freezes healing. The cause is a pair of hooks on the same call, not a `maxHealth` getter discrepancy:

1. HealingFactor's per-tick refresh calls `MedicalSystem::updateStats` (vtable slot `+0x8`) after it heals.
2. LimbToughness hooks that same `updateStats` call, re-applies its toughness max, and rescales flesh with `floorf(hp * newMax / oldMax + 0.0001)`.
3. The floor throws away every fractional heal. A tick heals less than 1 HP on parts with a small max, so on parts with `max < 80` the healed fraction is floored away on every tick and those parts never heal.
4. Vitals that the death-protection floor holds at a fractional value (5% of max) are floored below it and pushed back up every frame, so they stay pinned at the floor.
5. HealingFactor's flesh-drop watchdog compares each part's flesh with the previous tick (`prev - max(0.005 * max, 0.5) > flesh`). The floor reads as unreported damage, so the watchdog restarts the `WoundHealingDelaySeconds` (5 s) pause again and again.

**How TotalRestore avoids it:** it never hooks `updateStats`; the limb bonus is applied only on `HealthPartStatus::setup` (baseline capture), `MedicalSystem::init`, `MedicalSystem::load` and Toughness XP; flesh is rescaled without flooring; and the watchdog snapshot is taken after TotalRestore's own healing writes.

### Why the limb bonus is written into `_maxHealth`
The native `MedicalSystem::validateHealthValues` clamps flesh against the raw `_maxHealth * age * HPMult - wearDamage`, not against the `maxHealth()` getter. A getter-only buff (the previous TotalRestore approach) made the clamp and the healing target disagree. Saves made with the original LimbToughness already carry a baked `_maxHealth`, which a getter multiplied a second time (observed 220 -> 530). TotalRestore captures the unbuffed base from `setup` and writes `_maxHealth = base + bonus`, which also repairs those saves on load.

> Correction (C1 static RE, 2026-10-10): the clamp formula above is right, but the standalone `validateHealthValues` is never called on the Steam 1.0.65 exe (see "Native max-health getter and flesh clamp" below). The argument for writing `_maxHealth` still holds: the native code inlines the raw formula, so a getter-only buff is invisible to it.

### Native max-health getter and flesh clamp (C1, static RE of the Steam 1.0.65 `kenshi_x64.exe`)
Method: `dumpbin /disasm` over the disk image (image base `0x140000000`, `.text` file offset = RVA - `0xC00`, `.rdata` file offset = RVA - `0x1600`), functions identified by content, call sites found with a byte scan for every `E8`/`E9` rel32, every RIP-relative disp32 and every absolute qword in the file. The KenshiLib header RVAs (`maxHealth` `0x2967C0`, `validateHealthValues` `0x644E20`) do not point at these functions on this exe; `0x2967C0` is an unrelated function.

* **`HealthPartStatus::maxHealth()`** at RVA `0x296C70` (incremental-link thunk `0x16176: jmp 0x296C70`), a 5-instruction leaf:
  `movss xmm0,[rcx+58h]; mulss xmm0,[rcx+54h]; mulss xmm0,[rcx+5Ch]; subss xmm0,[rcx+50h]; ret`
  -> returns `age * _maxHealth * HPMult - wearDamage`. The getter already subtracts wear.
* **`MedicalSystem::validateHealthValues()`** at RVA `0x6455A0` (thunk `0x1715C: jmp 0x6455A0`). Loop over `anatomy` (count `+0x198`, data `+0x1A0`); per part, with `m = age * _maxHealth * HPMult - wearDamage` computed inline (not through the getter):
  * `flesh = max(min(flesh, m), -m)` (sign mask at `0x14168BA20`);
  * `bandaging = max(min(bandaging, 2 * m), 0)` (constants `2.0f` at `0x141682AD0`, `0.0f` at `0x141681B38`);
  * `fleshStun`, `wearDamage` and `_maxHealth` are not touched, `updateDerivedHealths` is not called.
* **No caller.** The only reference to `0x6455A0` is its own thunk, and nothing in the image calls, jumps to, or stores the address of the thunk (no rel32, no RIP-relative, no absolute pointer). The compiler inlined the function at its call sites, so a hook on it would never fire.
* **Inlined raw-max sites.** A scan for the inlined formula (`mulss [reg+5Ch]` followed by a flesh store `[reg+40h]`) finds only medical code in `0x644000-0x651000`: damage application, `setup` (`flesh = m`), a set-health-by-part-name routine (clamp to `[-m, m]`), and the per-part bandage update (`0x64F7xx-0x64FAxx`), which only moves bandaging into flesh while `flesh < m` and snaps an overshoot to `m`. None of them lowers flesh that already sits above `m`.
* **Consequence for DesolationSystem.** No native clamp found here pulls flesh back to the raw `m`, so the reported "HP drops right after healing" is not explained by `validateHealthValues`. The writer is DesolationSystem itself (next section).

### DesolationSystem.dll max HP and flesh clamp (C1, static RE of workshop 3796024680)
Method: `dumpbin /disasm` of the DLL (image base `0x180000000`, `.rdata` file offset = RVA - `0x1200`); the stats line `[韧性表] ... 血量补满 B=%d C=%d D回夹=%d 预算拦截=%d` (format at `0x18005CCAB`, printed from `0x18001ED5D`) names its counters: B `0x18013C3F0`, C `0x1800BC3D8`, **D `0x1800BC3DC`**, budget `0x18013C3F4`.
* **Hooks.** Registered as (KenshiLib symbol, detour, trampoline): `?_NV_updateStats@MedicalSystem@@QEAAXXZ` -> `0x1800250B0`, `?maxHealth@HealthPartStatus@MedicalSystem@@QEBAMXZ` -> `0x1800299B0`, `?setup@HealthPartStatus...` -> `0x180029A90`. The `maxHealth` detour is a pass-through (crash-trace label, then the original): **Desolation does not add its bonus through the getter.**
* **`updateStats` detour.** Calls the original, then `ApplyAll(medical, character)` at `0x18003B060`. TotalRestore's `medical.updateStats()` in `HealBodyParts` therefore runs `ApplyAll` on every healing tick, and native `updateStats` calls run it too.
* **Baseline table.** `0x1800BC3F0`, 32-byte entries keyed by the `HealthPartStatus*` (`+0x0`), baseline max at `+0x8`, last applied max at `+0xC`, tick at `+0x10`, a "filled" flag at `+0x14`; count at `0x1800BC3D4` (purged at `0x4000`). Filled by the `setup` detour from the `_max` argument; a part with no entry is skipped (log `ApplyAll 全部位无基线`).
* **`ApplyAll` per part** (`0x18003B2B8-0x18003B6BE`), stat `0x15` (Toughness) read through a function pointer:
  * `T = baseline * (1 + bonus(toughness))`, with NaN/Inf guards; a part with `T <= baseline` is skipped.
  * If `|_maxHealth - T| >= eps`: **writes `_maxHealth = T`** (`0x18003B57F`), then the keep-full fill (`0x18003E690`, presumably the `ToughHpCurSync` policy) and records `T` at `+0xC`.
  * Else, with `cap(x) = age * HPMult * x - wearDamage` (`0x18003F060`, HPMult guarded):
    * C fill (`0x18003B506`): flesh raised to `cap(T)` once per entry when the part was full against the baseline;
    * **D clamp-back** (`0x18003B623`): if `flesh > cap(T) + c` then **`flesh = cap(T)`**, counter D++.
* **Why HP drops and healing pauses.** TotalRestore writes `_maxHealth = base + bonus` (setup, init, load, Toughness XP) and rescales flesh with it; Desolation writes `_maxHealth = T` from its own baseline on every `updateStats`, and its D clamp pulls flesh down to `cap(T)`. Whenever TotalRestore's max is above `T`, flesh ends above the new cap, the clamp lowers it, and the flesh-drop watchdog read that as damage. Both mods own the same field with private baselines, so TotalRestore cannot reconstruct "both bonuses" without reading Desolation's private table.
* **TotalRestore fallback (C1).** `ExternalModCompat` detects `DesolationSystem.dll` (retried every 5 s until found, since RE_Kenshi may load it later) and, with `TolerateExternalFleshClamp=1`, the watchdog skips a drop for which `FleshCapRules::IsFleshCapClamp(previous, current, maxHealth())` holds: the previous flesh was above the current max and the new flesh sits on that max (tolerance `max(1e-4 * max, 0.01)`). A drop below the max still pauses healing. Without the DLL the watchdog is unchanged.
* Note: DesolationSystem logs `healthPartMaxHealth_hook -> kenshi+0x296C30`, which on this disk image lies inside the preceding function (`.pdata` range `0x296BD0-0x296C44`), 0x40 bytes before the getter. Its printed table does not match the disk image's function starts, so its logged addresses are not authoritative either.

### Original LimbToughness (mode 4) behaviour
* Baseline cached first-insert-wins in a map keyed by (part `GameData*` at `HealthPartStatus+0x0`, race `GameData*`), captured from the `_max` argument of `HealthPartStatus::setup(GameData*, float _hitchance, float _max, float _age, MedicalSystem*, bool, Character*)`. Fallback: the current `_maxHealth` when no setup was seen.
* Re-applied on `MedicalSystem::init`, `MedicalSystem::load`, `CharStats::xpStat_eventBased` / `xpStat_timeBased` (Toughness only), and `updateStats` (the hook that causes the conflict above; TotalRestore does not port it).

### HealingFactor behaviour ported by TotalRestore
* Tick: `dt = isPaused ? 0 : frameDelta * GameWorld::getFrameSpeedMultiplier()`; the update is skipped when the multiplier is `< 0` or `> 100`. Hooked through `GameWorld::_NV_mainLoop_GPUSensitiveStuff`.
* Damage pause from `HealthPartStatus::applyDamage` and the environmental damage functions `MedicalSystem::gettingEaten`, `gettingAcidRain`, `gettingAcidWater`, `gettingAcidFeet`, `gettingGassed`, `gettingWindyface`, `gettingBurnt` (all present in KenshiLib 0.5.0), plus the flesh-drop watchdog above.
* Per-character state cleaned on `GameWorld::removeFromUpdateListMain`.
* HealingFactor skipped robotic parts entirely (`isRobotic()` checks at `0x180006386` / `0x180006a12`) and stumps (`getLimbState() == 1`, i.e. `LIMB_STUMP`, at `0x1800063ad` / `0x180006a69`). TotalRestore heals skeleton-race parts (`HealSkeletonParts=1`), skips prosthetics on organics (`HealRobotLimbsOnOrganics=0`) and never heals stumps directly.

### HealingFactor limb regrowth (ported in T5)
* **Two truths per regrowing limb.** The raw `RobotLimbs::states[i]` is `LIMB_ORIGINAL` (after `setLimb(ORIGINAL)` at the reveal), so the native `AppearanceHuman::updateHiddenParts` draws the organic mesh. Gameplay sees `LIMB_STUMP` plus the stashed `items[i]` through hooks on `RobotLimbs::getState`, `getLimb`, `getMask`, `MedicalSystem::getLimbState` and `HealthPartStatus::getRobotLimbState`. The hooks report the visual truth only while a depth counter is > 0, which happens only around the native `updateHiddenParts` call (TotalRestore: RAII guard, per-thread counter; HealingFactor used one global int).
* `updateHiddenParts` is declared private in the KenshiLib header; KenshiLib.dll exports it as public `?updateHiddenParts@AppearanceHuman@@QEAAXXZ`, resolved with `GetProcAddress`.
* `RobotLimbs::save` and `MedicalSystem::medicalUpdate` force STUMP + stash into the raw state around the native call. After `medicalUpdate`, a state the native code changed (a robotic limb installed) is kept, so the tick stops the regrowth. The save file records a stump; regrowth progress is not persisted.
* `HealthPartStatus::isDead` returns false for a regrowing limb only inside the visual refresh. `HealthPartStatus::applyDamage` snapshots the intact limbs before the hit. `MedicalSystem::amputate` is swallowed for a limb that is already regrowing.
* `setLimb(ORIGINAL)` changes blood (`+0x70`), `extraBloodLossFromBodyparts` (`+0x74`), `currentBleedRate` (`+0x78`) and the part health fields; all are saved and restored around it.
* Bones: per `Bip01` limb bone the snapshot keeps `getBoneSize` and `getBonePositionalSize` (OgreMain exports, not in the headers) and the local `getScale`. Growth writes `size = captured x scale`; the root bone (UpperArm / Thigh) keeps its captured positional size, the others are scaled; then `needUpdate(true)` dispatched through `OldNode` (OldBone redeclares it non-virtually). A limb with no snapshot copies the mirror limb's; with none it waits. Completion restores the snapshot (not 1.0).
* Regrowth logic reads `rl->states` / `rl->items` directly, never the hooked getters (the lie would stop the regrowth).
* All `MedicalSystem::medicalUpdate` participants share one hook (`MedicalUpdateHook.cpp`): the regrowth scope and the death tracker.
* TotalRestore differences: humanoids only and skeleton races (`RaceData::robot`) never regrow; growth pauses with the healing pauses; completion flesh = `min(CompletionHealthFraction x max, regen ceiling)` (Legacy formula: fraction only); the completed limb max is re-normalised by LimbToughness.

---

## 2. Kenshi Engine Layout & Signatures

### Medical & Health System
* `LimbState` (`MedicalSystem.h`), returned by `MedicalSystem::getLimbState(RobotLimbs::Limb)`, `HealthPartStatus::getRobotLimbState()` and stored in `RobotLimbs::states[4]`:
  * `0 = LIMB_ORIGINAL`
  * `1 = LIMB_STUMP` (severed)
  * `2 = LIMB_REPLACED` (replaced by a robotic limb)
  * `3 = LIMB_CRUSHED`
* `HealthPartStatus` offsets: `data +0x0`, `fatal +0x32`, `flesh +0x40`, `fleshStun +0x44`, `bandaging +0x48`, `juryRigging +0x4C`, `wearDamage +0x50`, `_maxHealth +0x54`, `age +0x58`, `HPMult +0x5C`.
* `MedicalSystem` offsets: `blood +0x70`, `extraBloodLossFromBodyparts +0x74`, `currentBleedRate +0x78`, `robotLimbs +0xC8`, `me +0xE0`, `dead +0x164`, `anatomy +0x190`.
* `RaceData::robot` (`+0x7C`) marks skeleton races.
* Knocked out: `Character::_NV_isLiterallyUnconciousNotPretending() || Character::_NV_getProneState() == PS_KO` (neither is overridden by `CharacterHuman` / `CharacterAnimal`).
* `Character::isDead()` / `medical.dead`: direct boolean member of the character's `MedicalSystem`.
* `Character::healCompletely()`: restores all body parts, blood, stun, and wear to 100%.

### Death time
* `Character::declareDead()` is exported by KenshiLib 0.5.0. It is not verified in-game that every death path calls it, so TotalRestore also records the death right after `MedicalSystem::medicalUpdate` when `dead` is set, and falls back to the first observation for characters that died before the plugin loaded.
* Records are keyed by the `hand` fields (`type`, `container`, `containerSerial`, `index`, `serial`), not by `Character*`. Recruiting moves a character to another platoon, which changes its handle, so the record is dropped before `recruit`.

### GUI Integration (MyGUI)
* Target hook: `ForgottenGUI::selectedObjectsChanged(ForgottenGUI* thisptr)`
* Suffix matching for Kenshi's prefixed widget identifiers (`prefix_Name`).
* Injection point: child of the `StatusPanel` parent widget.

### TotalRestore yields `_maxHealth` to a foreign writer (C2)
* `LimbMaxOwnership` records, per `HealthPartStatus*`, the max LimbToughness last wrote and the attached `robotLimb`. When the current `_maxHealth` differs by >= 0.001 on the same limb, another mod rewrote it and `ApplyLimbToughness` skips the part (no write, no flesh rescale, no baseline capture from the foreign value). A changed `robotLimb` is a native prosthetic swap and never counts as foreign.
* Native `HealthPartStatus::setup` (hooked) and `MedicalSystem::load` write the field themselves, so both release the record and TotalRestore applies again. After a load TotalRestore therefore writes once and DesolationSystem takes the part back on its next `updateStats`; the resulting clamp is absorbed by the C1 watchdog tolerance. From then on Toughness XP no longer rewrites those parts.
* Active only while `DesolationSystem.dll` is loaded and `YieldMaxHealthToOtherMods=1`. It is not enabled for every foreign writer, because TotalRestore's own limb regrowth (`LimbRegrowth.cpp`: completion and collapsed-max repair) writes `_maxHealth` outside LimbToughness and would then read as foreign.
* DesolationSystem only writes parts with a baseline whose `T > baseline` (bonus > 0). Parts it skips (no baseline after a load, or no bonus yet) are never yielded and keep the TotalRestore bonus. The two bonuses never stack on the same part.
