#pragma once

#include "TotalRestore.h"

/*
 * MedicalUpdateHook
 * Single owner of the MedicalSystem::medicalUpdate hook (one hook per function).
 * It composes the participants explicitly:
 *   - LimbRegrowth: ShadowStumpScope around the native call (regrowing limbs read as stumps);
 *   - DeathTracker: records the death time after the native call.
 * Each participant enables itself; installing twice is a no-op.
 */

namespace TotalRestore
{
    bool InitMedicalUpdateHook();
}
