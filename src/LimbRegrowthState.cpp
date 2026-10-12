#include "LimbRegrowthState.h"
#include "CriticalSection.h"
#include "RegenStateRegistry.h"

namespace TotalRestore
{
    static const int kLimbCount = 4;

    // The map is mutated on the main thread (tick, amputate, removal) but the getter
    // hooks may run elsewhere; lookups and mutations take this lock. Entries are never
    // erased outside the main thread, so a returned pointer stays valid for the caller.
    static std::map<Character*, CharacterLimbs> g_characters;
    static CriticalSection g_charactersLock;
    static volatile LONG g_shadowCount = 0;
    // Per thread: only code running inside our own updateHiddenParts call (same call stack)
    // sees the visual truth. HealingFactor used one global int; this is stricter.
    static __declspec(thread) int t_visualDepth = 0;

    LimbEntry::LimbEntry()
        : active(false)
        , visualStarted(false)
        , shadow(false)
        , snapValid(false)
        , logDeferred(false)
        , elapsed(0.0f)
        , savedMax(0.0f)
        , stash(NULL)
        , snapSkel(NULL)
        , bones()
    {
    }

    CharacterLimbs::CharacterLimbs()
        : identity(0)
    {
    }

    static unsigned long long IdentityOf(Character* character)
    {
        const hand& handle = character->getHandle();
        return RegenStateRegistry::MakeIdentity(handle.index, handle.serial);
    }

    static int CountShadows(const CharacterLimbs& limbs)
    {
        int count = 0;
        for (int i = 0; i < kLimbCount; ++i)
        {
            if (limbs.limbs[i].shadow) ++count;
        }
        return count;
    }

    namespace RegrowthState
    {
        CharacterLimbs& GetOrCreate(Character* character)
        {
            const unsigned long long identity = IdentityOf(character);
            ScopedLock lock(g_charactersLock);
            CharacterLimbs& limbs = g_characters[character];
            if (limbs.identity != identity)
            {
                // A reused pointer: the old character's shadows cannot be restored any more.
                InterlockedExchangeAdd(&g_shadowCount, -CountShadows(limbs));
                limbs = CharacterLimbs();
                limbs.identity = identity;
            }
            return limbs;
        }

        CharacterLimbs* Find(Character* character)
        {
            ScopedLock lock(g_charactersLock);
            std::map<Character*, CharacterLimbs>::iterator it = g_characters.find(character);
            return (it != g_characters.end()) ? &it->second : NULL;
        }

        void Erase(Character* character)
        {
            ScopedLock lock(g_charactersLock);
            std::map<Character*, CharacterLimbs>::iterator it = g_characters.find(character);
            if (it == g_characters.end()) return;
            InterlockedExchangeAdd(&g_shadowCount, -CountShadows(it->second));
            g_characters.erase(it);
        }

        void CollectCharacters(std::vector<Character*>& out)
        {
            ScopedLock lock(g_charactersLock);
            for (std::map<Character*, CharacterLimbs>::iterator it = g_characters.begin(); it != g_characters.end(); ++it)
            {
                out.push_back(it->first);
            }
        }

        LimbEntry* ShadowEntry(Character* character, int limb)
        {
            if (g_shadowCount <= 0 || character == NULL || limb < 0 || limb >= kLimbCount) return NULL;

            ScopedLock lock(g_charactersLock);
            std::map<Character*, CharacterLimbs>::iterator it = g_characters.find(character);
            if (it == g_characters.end()) return NULL;
            LimbEntry& entry = it->second.limbs[limb];
            return (entry.active && entry.shadow) ? &entry : NULL;
        }

        LimbEntry* ShadowEntryForRobotLimbs(RobotLimbs* robotLimbs, int limb)
        {
            if (g_shadowCount <= 0 || robotLimbs == NULL) return NULL;
            return ShadowEntry(robotLimbs->character, limb);
        }

        LimbEntry* ShadowEntryForPart(MedicalSystem::HealthPartStatus* part)
        {
            if (g_shadowCount <= 0 || part == NULL) return NULL;
            const int limb = LimbIndexOfPart(part->medical, part);
            if (limb < 0) return NULL;
            Character* owner = (part->me != NULL) ? part->me : (part->medical != NULL ? part->medical->me : NULL);
            return ShadowEntry(owner, limb);
        }

        bool AnyActive(Character* character)
        {
            ScopedLock lock(g_charactersLock);
            std::map<Character*, CharacterLimbs>::iterator it = g_characters.find(character);
            if (it == g_characters.end()) return false;
            for (int i = 0; i < kLimbCount; ++i)
            {
                if (it->second.limbs[i].active) return true;
            }
            return false;
        }

        void SetShadow(LimbEntry& entry, bool shadow)
        {
            if (entry.shadow == shadow) return;
            entry.shadow = shadow;
            InterlockedExchangeAdd(&g_shadowCount, shadow ? 1 : -1);
        }

        int VisualDepth()
        {
            return t_visualDepth;
        }

        int LimbIndexOfPart(MedicalSystem* medical, MedicalSystem::HealthPartStatus* part)
        {
            if (medical == NULL || part == NULL) return -1;
            if (part == medical->leftArm) return 0;
            if (part == medical->rightArm) return 1;
            if (part == medical->leftLeg) return 2;
            if (part == medical->rightLeg) return 3;
            return -1;
        }

        MedicalSystem::HealthPartStatus* LimbPart(MedicalSystem* medical, int limb)
        {
            if (medical == NULL) return NULL;
            switch (limb)
            {
                case 0: return medical->leftArm;
                case 1: return medical->rightArm;
                case 2: return medical->leftLeg;
                case 3: return medical->rightLeg;
                default: return NULL;
            }
        }
    }

    VisualDepthGuard::VisualDepthGuard()
    {
        ++t_visualDepth;
    }

    VisualDepthGuard::~VisualDepthGuard()
    {
        --t_visualDepth;
    }
}

namespace TotalRestore
{
    PartHealthSnapshot::PartHealthSnapshot()
        : flesh(0.0f), fleshStun(0.0f), bandaging(0.0f), juryRigging(0.0f), wearDamage(0.0f), maxHealth(0.0f), derivedPercent(0.0f)
    {
    }

    void PartHealthSnapshot::Save(const MedicalSystem::HealthPartStatus* part)
    {
        flesh = part->flesh;
        fleshStun = part->fleshStun;
        bandaging = part->bandaging;
        juryRigging = part->juryRigging;
        wearDamage = part->wearDamage;
        maxHealth = part->_maxHealth;
        derivedPercent = part->derivedFleshHealthPercent;
    }

    void PartHealthSnapshot::Restore(MedicalSystem::HealthPartStatus* part) const
    {
        part->flesh = flesh;
        part->fleshStun = fleshStun;
        part->bandaging = bandaging;
        part->juryRigging = juryRigging;
        part->wearDamage = wearDamage;
        part->_maxHealth = maxHealth;
        part->derivedFleshHealthPercent = derivedPercent;
    }
}
