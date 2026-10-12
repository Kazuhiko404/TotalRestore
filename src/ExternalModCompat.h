#pragma once

/*
 * ExternalModCompat
 * Detects other plugins that also own HealthPartStatus::_maxHealth.
 *
 * DesolationSystem.dll (static RE, docs/REVERSE_ENGINEERING.md) hooks
 * MedicalSystem::updateStats and, after the native call, rewrites each part's
 * _maxHealth from its own setup baseline scaled by Toughness, then clamps flesh
 * down to age * HPMult * max - wearDamage. Both mods write _maxHealth, so the
 * flesh-drop watchdog must not read that clamp as damage.
 *
 * RE_Kenshi may load the other plugin after TotalRestore, so detection is
 * retried (throttled) until the module is found.
 */

namespace TotalRestore
{
    // Logs whether an external max HP mod is already loaded. Call once at startup.
    void InitExternalModCompat();

    // True when TolerateExternalFleshClamp=1 and an external max HP mod is loaded.
    // Without such a mod this is always false, so the watchdog keeps its vanilla behaviour.
    bool IsExternalFleshClampTolerated();

    // True once an external max HP mod (DesolationSystem.dll) is loaded; retried every 5 s until found.
    bool IsExternalMaxHealthModLoaded();

    // True when YieldMaxHealthToOtherMods=1 and an external max HP mod is loaded: LimbToughness
    // leaves a part's _maxHealth to that mod once it rewrote it (LimbMaxOwnership.h).
    // Without such a mod this is always false.
    bool IsMaxHealthYieldActive();
}
