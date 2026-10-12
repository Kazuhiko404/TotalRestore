#include "MedicalUpdateHook.h"
#include "DeathTracker.h"
#include "LimbRegrowth.h"
#include "LimbShadowHooks.h"

namespace TotalRestore
{
    static void (*g_origMedicalUpdate)(MedicalSystem*, float) = NULL;
    static bool g_installed = false;

    static void MedicalSystem_medicalUpdate_hook(MedicalSystem* thisptr, float frameTime)
    {
        if (IsLimbRegrowthActive())
        {
            ShadowStumpScope stumps(thisptr);
            g_origMedicalUpdate(thisptr, frameTime);
        }
        else
        {
            g_origMedicalUpdate(thisptr, frameTime);
        }

        OnMedicalUpdated(thisptr);
    }

    bool InitMedicalUpdateHook()
    {
        if (g_installed) return true;

        if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                KenshiLib::GetRealAddress(&MedicalSystem::medicalUpdate), MedicalSystem_medicalUpdate_hook, &g_origMedicalUpdate))
        {
            LogError("Could not install MedicalSystem::medicalUpdate hook.");
            return false;
        }
        g_installed = true;
        return true;
    }
}
