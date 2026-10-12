# TotalRestore in-game QA checklist

Run after every change set, with the default `TotalRestore.ini` unless a step says otherwise. Each item maps to an acceptance criterion in `odd/tasks/balance-overhaul.md`. Record the result (pass / fail + notes) next to each box.

Setup: a test save with (a) a human squad member with Toughness >= 100, (b) one with low Toughness (< 20), (c) a skeleton, (d) an organic character with a robotic prosthetic limb, (e) a save made with the original LimbToughness mod if one is available, and (f) a player-owned animal, ideally from a save made while animals were buffed. Check `RE_Kenshi_log.txt` for `[TotalRestore ERROR]` lines first: every hook must report installed.

## 0. Plugin start
- [ ] The log shows `LimbToughness hooks installed successfully.`, `HealingEngine hooks installed successfully.`, `Damage signal hooks installed successfully.`, `DeathTracker hooks installed successfully.` and `ReviveSystem hook installed successfully.`, with no `[TotalRestore ERROR]`.

## 1. Limb max HP (T2)
- [ ] A character with Toughness 106 and a 220-HP base part (e.g. a Shek torso) shows about **244** max on that part, not 530 and not 330.
- [ ] A Toughness 100 human (base 100) shows about 130 per part; a low-Toughness human shows close to the base.
- [ ] A low-HP race gets a slightly larger bonus than a high-HP race at the same Toughness (80 -> ~+31.7, 220 -> ~+24.6 at T100).
- [ ] Gaining Toughness XP in a fight raises the max a little, and current flesh keeps its proportion (no jump to full, no drop).
- [ ] Save, quit to menu, reload: the max is the same as before saving (no compounding on each load).
- [ ] Loading a save made with the original LimbToughness: the max is base + bonus, not the old inflated value.
- [ ] Skeleton parts and prosthetic limbs keep their native max (`ApplyToughnessToRobots=0`).
- [ ] `LimbBonusFormula=Percent`: the old behaviour returns (base * capped multiplier).
- [ ] A player animal (e.g. a Bull) gets no max-HP bonus (`ApplyToughnessToAnimals=0`).
- [ ] Loading a save made with the buffed version (animal max about double): after load the animal's parts are back to their base max, and flesh keeps its proportion.

## 2. Passive regeneration (T3)
- [ ] A T>=100 character with a wounded limb regenerates flesh up to **50%** of the part's base max (base without the toughness bonus) and stops there.
- [ ] A low-Toughness character stops at about 10-15% of base, and heals slower.
- [ ] A part already above the ceiling (treated by a doctor) is never reduced.
- [ ] Stun and wear keep recovering after flesh reaches the ceiling, while the part is still below `RegenStartBelowFraction`.
- [ ] `RegenStartBelowFraction=0.5` (default): a part at **70%** health (flesh minus stun) does not regenerate at all.
- [ ] A part at **40%** regenerates up to the ceiling or the 50% threshold, whichever comes first, then stops. Other healthy parts of the same character stay unchanged.
- [ ] `RegenStartBelowFraction=1` restores the old behaviour: every wounded part heals up to the ceiling.
- [ ] Regeneration is paused **in combat**.
- [ ] Regeneration is paused for **5 s after damage** (hit, then watch the part: no gain for 5 s). Repeat with acid rain or gas.
- [ ] Regeneration is paused while the character is **knocked out**, and resumes after waking up.
- [ ] At game speed x3 the same heal takes about a third of the real time; nothing heals while paused.
- [ ] Skeleton parts heal with the same formula (`HealSkeletonParts=1`).
- [ ] A robotic prosthetic on an organic character does not heal (`HealRobotLimbsOnOrganics=0`).
- [ ] Severed stumps are not healed by passive regen.
- [ ] Once blood is full, bleeding stops (`currentBleedRate` and extra body-part blood loss cleared).
- [ ] `HealingFormula=Legacy` with `RegenStartBelowFraction=1`: the old full refill over `WoundRegenerationSeconds` returns.
- [ ] A player animal gets no TotalRestore healing (`ApplyHealingToAnimals=0`): in a fight it loses HP and does not heal faster than vanilla; no blood refill or limb regrowth from TotalRestore.
- [ ] With `ApplyHealingToAnimals=1` and `ApplyToughnessToAnimals=1` the old behaviour returns (animal buffed and healed).
- [ ] With DesolationSystem loaded: the log shows `External max HP mod detected (DesolationSystem.dll)`. Heal a wounded character to full with first aid; passive regen is not paused by the HP settling at the max afterwards (it resumes once the part drops below `RegenStartBelowFraction`). Real hits still pause healing.
- [ ] Without DesolationSystem: no detection line, and the watchdog behaves exactly as before.
- [ ] With DesolationSystem and `TolerateExternalFleshClamp=0`: every flesh drop pauses healing again (old behaviour).
- [ ] With DesolationSystem (`YieldMaxHealthToOtherMods=1`): a character Desolation manages keeps Desolation's limb max HP; after first aid to full and after gaining Toughness XP, HP no longer drops by itself. The log shows `another mod rewrote a limb max HP` once.
- [ ] With DesolationSystem: a character Desolation skips (its log `ApplyAll 全部位无基线`) keeps the TotalRestore limb bonus.
- [ ] Without DesolationSystem: limb max HP and Toughness XP re-application are identical to before (no yield line in the log).
- [ ] With DesolationSystem and `YieldMaxHealthToOtherMods=0`: TotalRestore writes its bonus again on load/XP (old behaviour; the two mods overwrite each other).

## 3. Mortality (T3)
- [ ] With default settings a player character **can be knocked out** in a fight.
- [ ] With default settings a player character **can die** (e.g. let a bleeding character die, or lose the head/chest).
- [ ] `EnableDeathProtection=1`: vitals and blood stay above the emergency floors (old behaviour).

## 4. Revive (T4)
- [ ] Selecting a freshly dead character shows the Revive button; reviving gives every part and the blood about **`ReviveHealthFraction`** (10%) of max, with no stun or wear left.
- [ ] The revived character joins the squad.
- [ ] A character dead for longer than `MaxReviveTimeMinutes` in-game minutes (wait or sleep past it) **cannot** be revived: no button.
- [ ] The window counts from the death, not from the first selection: let a character die, wait past the window without selecting it, then select it: no button.
- [ ] A character that was already dead in the loaded save can be revived within the window counted from the first time it is selected (documented fallback).
- [ ] `EnableReviveTimeLimit=0`: any dead character can be revived.

## 5. Limb regrowth (T5)
- [ ] The log shows `LimbRegrowth hooks installed successfully.` (needs `EnableBodyPartRegeneration=1` or `EnableBloodRegeneration=1`).
- [ ] Sever an arm of an intact human squad member: a small arm appears at the shoulder (attached, not floating) and visibly grows over `RegenerationSeconds` of game time.
- [ ] While growing, the limb is still a stump for gameplay: no weapon in that hand (or no walking on that leg), the medical panel shows it severed, and it cannot be severed again.
- [ ] At completion the limb is full size (same size as the opposite limb, not oversized), usable, and its flesh is `min(CompletionHealthFraction x max, regen ceiling)`; blood and bleeding did not jump.
- [ ] `HealingFormula=Legacy`: the completion flesh is `CompletionHealthFraction x max` only.
- [ ] Growth pauses while in combat, after damage and while knocked out, and resumes afterwards.
- [ ] Save mid-growth and reload: the limb is a stump again and the regrowth restarts from the start (small limb).
- [ ] Load a save where a character already has a stump and the mirror limb is intact: the stump regrows (mirror snapshot). With both limbs of the pair missing, it waits (log: `regrowth deferred`).
- [ ] Install a robotic prosthetic on the stump mid-growth: regrowth stops (log: `robotic replacement installed`), the prosthetic works, and the bones are back to normal.
- [ ] Skeleton characters: severed limbs never regrow; their parts still heal.
- [ ] Change equipment or appearance mid-growth: growth continues (or restarts) without a misplaced limb.
- [ ] Dismiss or unload the character mid-growth, then bring it back: no shrunk bones; the limb is a stump that regrows.

## 6. Build gates
- [ ] `tests/totalrestore_tests.exe`: all checks pass.
- [ ] `TotalRestore.vcxproj` Release x64 builds with 0 errors.
