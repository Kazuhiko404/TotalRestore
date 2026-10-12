#include "TotalRestore.h"
#include "ExternalModCompat.h"
#include "LimbRegrowth.h"
#include <iostream>

namespace TotalRestore
{
    Config g_config;

    void Log(const std::string& msg)
    {
        DebugLog(("[TotalRestore] " + msg).c_str());
        std::cout << "[TotalRestore] " << msg << std::endl;
    }

    void LogError(const std::string& msg)
    {
        DebugLog(("[TotalRestore ERROR] " + msg).c_str());
        std::cerr << "[TotalRestore ERROR] " << msg << std::endl;
    }
}

// RE_Kenshi Plugin Entry Point (Mangled C++ signature for RE_Kenshi)
__declspec(dllexport) void startPlugin()
{
    TotalRestore::Log("Plugin starting...");

    // 1. Locate config file relative to game root or mod directory
    char modulePath[MAX_PATH];
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    std::string path(modulePath);
    size_t lastSlash = path.find_last_of("\\/");
    std::string dir = (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : ".";
    std::string iniPath = dir + "\\mods\\TotalRestore\\TotalRestore.ini";

    TotalRestore::g_config.Load(iniPath);

    // Other plugins that also write _maxHealth (DesolationSystem): watchdog tolerance only.
    TotalRestore::InitExternalModCompat();

    if (!TotalRestore::g_config.enableMod)
    {
        TotalRestore::Log("Mod is disabled via configuration.");
        return;
    }

    // 2. Initialize Subsystems
    if (TotalRestore::g_config.enableLimbToughness)
    {
        TotalRestore::InitLimbToughness();
    }

    if (TotalRestore::g_config.enableBodyPartRegeneration || TotalRestore::g_config.enableBloodRegeneration)
    {
        TotalRestore::InitHealingEngine();
    }

    if (TotalRestore::g_config.enableLimbRegeneration)
    {
        // Ticked by the HealingEngine (needs body-part or blood regeneration enabled).
        TotalRestore::InitLimbRegrowth();
    }

    if (TotalRestore::g_config.enableReviveSystem)
    {
        TotalRestore::InitReviveSystem();
    }

    TotalRestore::Log("Plugin started successfully with all modules active.");
}

__declspec(dllexport) void stopPlugin()
{
    // Put back shrunk bones and raw stump states of any limb still regrowing.
    TotalRestore::ShutdownLimbRegrowth();
    TotalRestore::Log("Plugin stopped.");
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        break;
    default:
        break;
    }
    return TRUE;
}
