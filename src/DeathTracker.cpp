#include "DeathTracker.h"
#include "CriticalSection.h"
#include "MedicalUpdateHook.h"
#include "ReviveRules.h"

/*
 * DeathTracker Module
 * Records when a character dies, in in-game minutes, keyed by its handle fields
 * (type, container, containerSerial, index, serial), never by Character*.
 *
 * Sources, earliest wins:
 *   1. Character::declareDead (KenshiLib 0.5.0 exposes it; not verified in-game
 *      that every death path calls it, hence the second source).
 *   2. MedicalSystem::medicalUpdate: right after the engine's per-character medical
 *      update, a character whose `dead` flag is set and has no record is recorded.
 *      This catches the dead transition for every character the engine updates
 *      (the ones near the player).
 *   3. Fallback in ReviveSystem: a character selected dead with no record (died
 *      before the plugin loaded, or loaded dead from a save) gets the first
 *      observation as its death time.
 * Records live for the session only; a record later than the current clock
 * (an earlier save was loaded) is reset to the current time.
 */

namespace TotalRestore
{
    static DeathClock g_deathClock;
    static CriticalSection g_deathClockLock;

    static void (*g_origDeclareDead)(Character*) = NULL;
    static bool g_trackingEnabled = false;

    static DeathKey KeyOf(Character* character)
    {
        const hand& handle = character->getHandle();
        return DeathKey((unsigned int)handle.type, handle.container, handle.containerSerial, handle.index, handle.serial);
    }

    bool CurrentInGameMinutes(double& outMinutes)
    {
        if (ou == NULL) return false;
        outMinutes = ou->getTimeStamp_inGameHours().getTotalMinutes();
        return true;
    }

    static void RecordDeath(Character* character)
    {
        double now = 0.0;
        if (character == NULL || !CurrentInGameMinutes(now)) return;

        ScopedLock lock(g_deathClockLock);
        g_deathClock.RecordDeath(KeyOf(character), now);
    }

    double ObserveDeathMinutes(Character* character, double nowMinutes)
    {
        ScopedLock lock(g_deathClockLock);
        return g_deathClock.Observe(KeyOf(character), nowMinutes);
    }

    void ForgetDeath(Character* character)
    {
        if (character == NULL) return;
        ScopedLock lock(g_deathClockLock);
        g_deathClock.Forget(KeyOf(character));
    }

    static void Character_declareDead_hook(Character* thisptr)
    {
        g_origDeclareDead(thisptr);
        RecordDeath(thisptr);
    }

    void OnMedicalUpdated(MedicalSystem* thisptr)
    {
        if (g_trackingEnabled && thisptr != NULL && thisptr->dead)
        {
            RecordDeath(thisptr->me);
        }
    }

    bool InitDeathTracker()
    {
        bool ok = true;
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                KenshiLib::GetRealAddress(&Character::declareDead), Character_declareDead_hook, &g_origDeclareDead))
        {
            LogError("Could not install Character::declareDead hook for DeathTracker.");
            ok = false;
        }
        // The medicalUpdate hook is shared with LimbRegrowth (MedicalUpdateHook.cpp).
        g_trackingEnabled = true;
        if (!InitMedicalUpdateHook()) ok = false;
        if (ok) Log("DeathTracker hooks installed successfully.");
        return ok;
    }
}
