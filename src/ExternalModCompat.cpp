#include "ExternalModCompat.h"
#include "TotalRestore.h"

namespace TotalRestore
{
    static const char* const kExternalMaxHealthModule = "DesolationSystem.dll";
    static const DWORD kRedetectIntervalMs = 5000;

    static bool g_detected = false;
    static bool g_checkedOnce = false;
    static DWORD g_lastCheckMs = 0;

    bool IsExternalMaxHealthModLoaded()
    {
        if (g_detected) return true;

        const DWORD now = GetTickCount();
        if (g_checkedOnce && (now - g_lastCheckMs) < kRedetectIntervalMs) return false;
        g_checkedOnce = true;
        g_lastCheckMs = now;

        if (GetModuleHandleA(kExternalMaxHealthModule) == NULL) return false;

        g_detected = true;
        Log(std::string("External max HP mod detected (") + kExternalMaxHealthModule + "): " +
            (g_config.yieldMaxHealthToOtherMods ? "limb max HP it rewrites is left to it" : "limb max HP is not yielded (YieldMaxHealthToOtherMods=0)") +
            "; " +
            (g_config.tolerateExternalFleshClamp ? "flesh clamped to maxHealth() by it is not treated as damage."
                                                 : "its flesh clamp pauses healing (TolerateExternalFleshClamp=0)."));
        return true;
    }

    void InitExternalModCompat()
    {
        IsExternalMaxHealthModLoaded();
    }

    bool IsExternalFleshClampTolerated()
    {
        if (!g_config.tolerateExternalFleshClamp) return false;
        return IsExternalMaxHealthModLoaded();
    }

    bool IsMaxHealthYieldActive()
    {
        if (!g_config.yieldMaxHealthToOtherMods) return false;
        return IsExternalMaxHealthModLoaded();
    }
}
