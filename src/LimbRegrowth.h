#pragma once

#include "TotalRestore.h"
#include "RegrowthRules.h"

/*
 * LimbRegrowth (faithful HealingFactor port)
 * Lifecycle of organic limb regrowth: amputate hook, stump detection, bone
 * snapshot, reveal, growth, completion, stop, and cleanup. Hooks that keep the
 * two limb truths consistent live in LimbShadowHooks.cpp; bones in LimbBoneSnapshot.cpp.
 */

namespace TotalRestore
{
    RegrowthSettings BuildRegrowthSettings();

    // Installs the amputate hook and the shadow hooks. Only when regrowth and at
    // least one limb switch are enabled and the OgreMain bone exports resolve.
    bool InitLimbRegrowth();
    bool IsLimbRegrowthActive();

    // Per tick for an eligible player character: snapshot intact limbs, detect stumps,
    // and (when `canRegrow`) advance growth. `frameTime` is simulated seconds.
    void ProcessLimbRegrowth(Character* character, float frameTime, bool canRegrow);

    // Snapshots the intact limbs now (called before damage is applied).
    void CaptureIntactLimbs(Character* character);

    // Restores bones, drops shadows and forgets the character (left the update list).
    void ForgetLimbRegrowth(Character* character);

    // Restores every tracked character (plugin unload).
    void ShutdownLimbRegrowth();
}
