#include "LimbBaselineCache.h"

namespace TotalRestore
{
    LimbBaselineKey::LimbBaselineKey(const void* partData, const void* raceData, const void* variantData)
        : part(partData)
        , race(raceData)
        , variant(variantData)
    {
    }

    bool LimbBaselineKey::operator<(const LimbBaselineKey& other) const
    {
        if (part != other.part) return part < other.part;
        if (race != other.race) return race < other.race;
        return variant < other.variant;
    }

    bool LimbBaselineCache::Find(const LimbBaselineKey& key, float& outBase) const
    {
        std::map<LimbBaselineKey, float>::const_iterator it = m_bases.find(key);
        if (it == m_bases.end()) return false;
        outBase = it->second;
        return true;
    }

    float LimbBaselineCache::Remember(const LimbBaselineKey& key, float base)
    {
        float existing = 0.0f;
        if (Find(key, existing)) return existing;
        if (key.part == NULL || !(base > 0.0f)) return base;

        m_bases.insert(std::make_pair(key, base));
        return base;
    }

    size_t LimbBaselineCache::Size() const
    {
        return m_bases.size();
    }

    void LimbBaselineCache::Clear()
    {
        m_bases.clear();
    }
}
