#include "RegenStateRegistry.h"

namespace TotalRestore
{
    CharacterRegenState::CharacterRegenState()
        : identity(0)
        , damageCooldown(0.0f)
        , previousFlesh()
    {
    }

    CharacterRegenState& RegenStateRegistry::Get(const void* character, unsigned long long identity)
    {
        CharacterRegenState& state = m_states[character];
        if (state.identity != identity)
        {
            state = CharacterRegenState();
            state.identity = identity;
        }
        return state;
    }

    CharacterRegenState* RegenStateRegistry::Find(const void* character, unsigned long long identity)
    {
        std::map<const void*, CharacterRegenState>::iterator it = m_states.find(character);
        if (it == m_states.end() || it->second.identity != identity) return NULL;
        return &it->second;
    }

    void RegenStateRegistry::Forget(const void* character)
    {
        m_states.erase(character);
    }

    size_t RegenStateRegistry::Size() const
    {
        return m_states.size();
    }

    unsigned long long RegenStateRegistry::MakeIdentity(unsigned int index, unsigned int serial)
    {
        return (static_cast<unsigned long long>(serial) << 32) | static_cast<unsigned long long>(index);
    }
}
