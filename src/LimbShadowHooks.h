#pragma once

#include "TotalRestore.h"

/*
 * LimbShadowHooks
 * Engine hooks that keep the two truths of a regrowing limb consistent:
 *   - getter hooks (RobotLimbs::getState/getLimb/getMask, MedicalSystem::getLimbState,
 *     HealthPartStatus::getRobotLimbState) report STUMP + stashed item to gameplay
 *     and ORIGINAL with no item inside the visual refresh;
 *   - RobotLimbs::save writes a stump into the save file;
 *   - HealthPartStatus::isDead keeps the regrowing mesh visible during the refresh;
 *   - AppearanceHuman::updateHiddenParts runs the native refresh with the raw
 *     states set to ORIGINAL and the visual-depth counter raised.
 */

namespace TotalRestore
{
    bool InitLimbShadowHooks();

    // Runs the native updateHiddenParts for a humanoid with the visual truth.
    void RefreshLimbVisual(Character* character);

    // Forces STUMP + stashed item into the raw limb state for the lifetime of the
    // scope (around the native medicalUpdate), then restores whatever the native
    // code left as STUMP. A native change (robotic limb installed) is kept.
    class ShadowStumpScope
    {
    public:
        explicit ShadowStumpScope(MedicalSystem* medical);
        ~ShadowStumpScope();

    private:
        ShadowStumpScope(const ShadowStumpScope&);
        ShadowStumpScope& operator=(const ShadowStumpScope&);

        RobotLimbs* m_robotLimbs;
        bool m_changed[4];
        LimbState m_states[4];
        Item* m_items[4];
    };
}
