#pragma once

#include <map>

/*
 * LimbBaselineCache
 * Remembers the unbuffed max HP of a body part, first insert wins, exactly like
 * the original LimbToughness. Keys are opaque pointers so this stays free of
 * game includes and can be unit tested:
 *   part    = the body-part GameData (HealthPartStatus::data)
 *   race    = the race GameData (RaceData::data), may be NULL
 *   variant = the attached RobotLimbItem for a prosthetic, NULL for the native part
 */

namespace TotalRestore
{
    struct LimbBaselineKey
    {
        const void* part;
        const void* race;
        const void* variant;

        LimbBaselineKey(const void* partData, const void* raceData, const void* variantData);
        bool operator<(const LimbBaselineKey& other) const;
    };

    class LimbBaselineCache
    {
    public:
        bool Find(const LimbBaselineKey& key, float& outBase) const;

        // Stores the base only when the key is new and the value is a valid max (> 0).
        // Returns the value now associated with the key (the first one ever stored),
        // or `base` itself when nothing could be stored.
        float Remember(const LimbBaselineKey& key, float base);

        size_t Size() const;
        void Clear();

    private:
        std::map<LimbBaselineKey, float> m_bases;
    };
}
