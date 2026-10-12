#include "ReviveRules.h"

namespace TotalRestore
{
    static const float kMinReviveFraction = 0.01f;
    static const float kMaxReviveFraction = 1.0f;

    DeathKey::DeathKey()
        : type(0), container(0), containerSerial(0), index(0), serial(0)
    {
    }

    DeathKey::DeathKey(unsigned int handleType, unsigned int handleContainer, unsigned int handleContainerSerial,
                       unsigned int handleIndex, unsigned int handleSerial)
        : type(handleType)
        , container(handleContainer)
        , containerSerial(handleContainerSerial)
        , index(handleIndex)
        , serial(handleSerial)
    {
    }

    bool DeathKey::operator<(const DeathKey& other) const
    {
        if (type != other.type) return type < other.type;
        if (container != other.container) return container < other.container;
        if (containerSerial != other.containerSerial) return containerSerial < other.containerSerial;
        if (index != other.index) return index < other.index;
        return serial < other.serial;
    }

    void DeathClock::RecordDeath(const DeathKey& key, double nowMinutes)
    {
        m_deaths.insert(std::make_pair(key, nowMinutes));
    }

    double DeathClock::Observe(const DeathKey& key, double nowMinutes)
    {
        std::map<DeathKey, double>::iterator it = m_deaths.find(key);
        if (it == m_deaths.end())
        {
            m_deaths.insert(std::make_pair(key, nowMinutes));
            return nowMinutes;
        }
        if (it->second > nowMinutes)
        {
            it->second = nowMinutes;
        }
        return it->second;
    }

    bool DeathClock::Find(const DeathKey& key, double& outMinutes) const
    {
        std::map<DeathKey, double>::const_iterator it = m_deaths.find(key);
        if (it == m_deaths.end()) return false;
        outMinutes = it->second;
        return true;
    }

    void DeathClock::Forget(const DeathKey& key)
    {
        m_deaths.erase(key);
    }

    size_t DeathClock::Size() const
    {
        return m_deaths.size();
    }

    namespace ReviveRules
    {
        bool IsWithinReviveWindow(bool limitEnabled, double maxMinutes, double deathMinutes, double nowMinutes)
        {
            if (!limitEnabled) return true;
            const double minutesDead = nowMinutes - deathMinutes;
            if (minutesDead < 0.0) return true; // clock went backwards (earlier save loaded)
            return minutesDead <= maxMinutes;
        }

        float ClampReviveFraction(float fraction)
        {
            if (!(fraction >= kMinReviveFraction)) return kMinReviveFraction;
            return (fraction > kMaxReviveFraction) ? kMaxReviveFraction : fraction;
        }
    }
}
