#pragma once

#include "TotalRestore.h"

namespace TotalRestore
{
    // Player character targeted by the healing modules (ApplyToAllPlayerHumanoids / NameSuffix).
    bool IsHealingEligible(Character* character);

    // Starts the WoundHealingDelaySeconds pause for an eligible character.
    void NotifyCharacterDamaged(Character* character);

    // Drops every per-character healing state (character left the update list).
    void ForgetHealingState(Character* character);

    // Hooks that report damage and removal to the healing engine (DamageSignals.cpp).
    bool InitDamageSignals();
}
