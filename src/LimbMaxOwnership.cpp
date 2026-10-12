#include "LimbMaxOwnership.h"

#include <float.h>
#include <math.h>

namespace TotalRestore
{
    // Same noise floor LimbToughness uses before writing a new max.
    static const float kMaxChangeEpsilon = 0.001f;

    bool LimbMaxOwnership::IsForeignWrite(float writtenMax, const void* writtenVariant, float currentMax, const void* currentVariant)
    {
        if (writtenVariant != currentVariant) return false;
        if (_finite(writtenMax) == 0 || _finite(currentMax) == 0) return false;
        if (!(writtenMax > 0.0f)) return false;
        return fabsf(currentMax - writtenMax) >= kMaxChangeEpsilon;
    }

    void LimbMaxOwnership::RecordWrite(const void* part, float writtenMax, const void* variant)
    {
        if (part == NULL) return;
        if (m_records.size() >= kMaxRecords && m_records.find(part) == m_records.end()) m_records.clear();

        Record record;
        record.writtenMax = writtenMax;
        record.variant = variant;
        m_records[part] = record;
    }

    void LimbMaxOwnership::Release(const void* part)
    {
        m_records.erase(part);
    }

    bool LimbMaxOwnership::IsForeignOwned(const void* part, float currentMax, const void* currentVariant) const
    {
        std::map<const void*, Record>::const_iterator it = m_records.find(part);
        if (it == m_records.end()) return false;
        return IsForeignWrite(it->second.writtenMax, it->second.variant, currentMax, currentVariant);
    }

    size_t LimbMaxOwnership::Size() const
    {
        return m_records.size();
    }

    void LimbMaxOwnership::Clear()
    {
        m_records.clear();
    }
}
