#pragma once

#include <map>

/*
 * ReviveRules
 * Pure revive-window rules and the death clock (no game includes, unit tested).
 * Death times are in-game minutes (GameWorld::getTimeStamp_inGameHours) keyed by
 * the character's handle fields, never by a reusable Character pointer.
 */

namespace TotalRestore
{
    struct DeathKey
    {
        unsigned int type;
        unsigned int container;
        unsigned int containerSerial;
        unsigned int index;
        unsigned int serial;

        DeathKey();
        DeathKey(unsigned int handleType, unsigned int handleContainer, unsigned int handleContainerSerial,
                 unsigned int handleIndex, unsigned int handleSerial);
        bool operator<(const DeathKey& other) const;
    };

    class DeathClock
    {
    public:
        // Records the moment of death; an existing record is kept (first record wins).
        void RecordDeath(const DeathKey& key, double nowMinutes);

        // Returns the recorded death time. A character with no record (died before the
        // plugin loaded, or in a save) gets `nowMinutes` as its first observation. A record
        // in the future (an earlier save was loaded) is reset to `nowMinutes`.
        double Observe(const DeathKey& key, double nowMinutes);

        bool Find(const DeathKey& key, double& outMinutes) const;
        void Forget(const DeathKey& key);
        size_t Size() const;

    private:
        std::map<DeathKey, double> m_deaths;
    };

    namespace ReviveRules
    {
        // True while the character may still be revived.
        bool IsWithinReviveWindow(bool limitEnabled, double maxMinutes, double deathMinutes, double nowMinutes);

        // ReviveHealthFraction clamped to [0.01, 1].
        float ClampReviveFraction(float fraction);
    }
}
