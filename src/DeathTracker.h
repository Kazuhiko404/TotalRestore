#pragma once

#include "TotalRestore.h"

namespace TotalRestore
{
    // Hooks that record the in-game minute a character died (DeathTracker.cpp).
    bool InitDeathTracker();

    // In-game minutes since the game started; false when the world is not ready.
    bool CurrentInGameMinutes(double& outMinutes);

    // Recorded death time of a dead character. When none was recorded (died before the
    // plugin loaded, or loaded dead from a save), the first observation is recorded and returned.
    double ObserveDeathMinutes(Character* character, double nowMinutes);

    // Called by the shared medicalUpdate hook after the native update.
    void OnMedicalUpdated(MedicalSystem* medical);

    // Drops the record of a revived character.
    void ForgetDeath(Character* character);
}
