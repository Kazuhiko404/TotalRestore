#pragma once

#include <map>

/*
 * RegenStateRegistry
 * Per-character healing state (damage cooldown + watchdog snapshot), keyed by
 * the character pointer AND an identity stamp built from its handle. A pointer
 * the engine reuses for another character carries a different stamp, so it
 * never inherits stale state. No game includes: keys are opaque pointers.
 */

namespace TotalRestore
{
    struct CharacterRegenState
    {
        unsigned long long identity;
        float damageCooldown;
        std::map<const void*, float> previousFlesh; // per body part, after our own healing writes

        CharacterRegenState();
    };

    class RegenStateRegistry
    {
    public:
        // Returns the state for this character, resetting it when the identity changed.
        CharacterRegenState& Get(const void* character, unsigned long long identity);

        // Returns NULL when there is no state for this character with this identity.
        CharacterRegenState* Find(const void* character, unsigned long long identity);

        void Forget(const void* character);
        size_t Size() const;

        // Identity stamp from the handle's index and serial.
        static unsigned long long MakeIdentity(unsigned int index, unsigned int serial);

    private:
        std::map<const void*, CharacterRegenState> m_states;
    };
}
