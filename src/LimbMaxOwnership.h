#pragma once

#include <map>
#include <stddef.h>

/*
 * LimbMaxOwnership
 * Who owns HealthPartStatus::_maxHealth for a body part (no game includes, unit tested).
 *
 * DesolationSystem rewrites _maxHealth from its own baseline after every
 * MedicalSystem::updateStats for the parts it manages (docs/REVERSE_ENGINEERING.md).
 * If TotalRestore kept writing base + bonus as well, the two mods would overwrite
 * each other and the foreign flesh clamp would pull HP down after every write.
 *
 * LimbToughness records the value it last wrote per part. When the current max
 * no longer matches it on the same limb (same prosthetic pointer), another mod
 * rewrote the field and TotalRestore yields that part: no write, no rescale.
 * Native HealthPartStatus::setup and MedicalSystem::load write the field
 * legitimately, so they release the record and TotalRestore applies again.
 * Keys are opaque pointers (the HealthPartStatus and its RobotLimbItem).
 */

namespace TotalRestore
{
    class LimbMaxOwnership
    {
    public:
        // The table is dropped when it reaches this size (parts are never freed explicitly).
        static const size_t kMaxRecords = 16384;

        // Pure rule: true when the max changed since our write (beyond float noise) on the
        // same limb. A changed prosthetic pointer is a native swap, not a foreign writer.
        // Invalid values (NaN, Inf, a written max <= 0) are never foreign.
        static bool IsForeignWrite(float writtenMax, const void* writtenVariant, float currentMax, const void* currentVariant);

        void RecordWrite(const void* part, float writtenMax, const void* variant);
        void Release(const void* part);

        // False for a part TotalRestore never wrote, so untouched parts are never yielded.
        bool IsForeignOwned(const void* part, float currentMax, const void* currentVariant) const;

        size_t Size() const;
        void Clear();

    private:
        struct Record
        {
            float writtenMax;
            const void* variant;
        };

        std::map<const void*, Record> m_records;
    };
}
