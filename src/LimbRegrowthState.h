#pragma once

#include "TotalRestore.h"
#include <ogre/OgreVector3.h>

namespace Ogre { class OldSkeletonInstance; }

/*
 * LimbRegrowthState
 * Shared state of the limb regrowth port: one LimbEntry per limb per character,
 * and the visual-depth counter that selects which "truth" the limb getters report.
 *
 * During regrowth the raw RobotLimbs::states[i] is LIMB_ORIGINAL (so the native
 * updateHiddenParts draws the organic mesh) while the hooked getters report
 * LIMB_STUMP plus the stashed item to gameplay ("shadow" state). The counter is
 * > 0 only around the native updateHiddenParts call (VisualDepthGuard).
 */

namespace TotalRestore
{
    struct BoneRecord
    {
        std::string name;
        Ogre::Vector3 boneSize;  // OldBone::getBoneSize
        Ogre::Vector3 posSize;   // OldBone::getBonePositionalSize
        Ogre::Vector3 scale;     // OldNode::getScale (local)
    };

    struct LimbEntry
    {
        bool active;          // Regrowth in progress
        bool visualStarted;   // Reveal done, growth running
        bool shadow;          // Raw state forced ORIGINAL, getters report STUMP
        bool snapValid;       // Bone snapshot usable (captured intact or copied from the mirror)
        bool logDeferred;     // "No snapshot" logged once
        float elapsed;        // Simulated seconds grown
        float savedMax;       // Raw _maxHealth before amputation (0 = unknown)
        Item* stash;          // RobotLimbs::items[i] captured at the reveal
        Ogre::OldSkeletonInstance* snapSkel; // Skeleton the snapshot belongs to
        std::vector<BoneRecord> bones;

        LimbEntry();
    };

    struct CharacterLimbs
    {
        unsigned long long identity;
        LimbEntry limbs[4]; // RobotLimbs::Limb order

        CharacterLimbs();
    };

    // The health fields setLimb or the visual refresh may touch; saved and put back.
    struct PartHealthSnapshot
    {
        float flesh, fleshStun, bandaging, juryRigging, wearDamage, maxHealth, derivedPercent;

        PartHealthSnapshot();
        void Save(const MedicalSystem::HealthPartStatus* part);
        void Restore(MedicalSystem::HealthPartStatus* part) const;
    };

    namespace RegrowthState
    {
        // Creates (or resets, when the handle identity changed) the entry set of a character.
        CharacterLimbs& GetOrCreate(Character* character);
        CharacterLimbs* Find(Character* character);
        void Erase(Character* character);

        // Every tracked character (for unload).
        void CollectCharacters(std::vector<Character*>& out);

        // The entry of a limb that is regrowing in the shadow state, or NULL.
        LimbEntry* ShadowEntry(Character* character, int limb);
        LimbEntry* ShadowEntryForRobotLimbs(RobotLimbs* robotLimbs, int limb);
        // Resolves the limb by comparing the part with the four limb parts of its MedicalSystem.
        LimbEntry* ShadowEntryForPart(MedicalSystem::HealthPartStatus* part);
        bool AnyActive(Character* character);

        // Maintains the global shadow count used as a fast path by the getter hooks.
        void SetShadow(LimbEntry& entry, bool shadow);

        // > 0 only while the native updateHiddenParts runs under VisualDepthGuard.
        int VisualDepth();

        int LimbIndexOfPart(MedicalSystem* medical, MedicalSystem::HealthPartStatus* part);
        MedicalSystem::HealthPartStatus* LimbPart(MedicalSystem* medical, int limb);
    }

    class VisualDepthGuard
    {
    public:
        VisualDepthGuard();
        ~VisualDepthGuard();

    private:
        VisualDepthGuard(const VisualDepthGuard&);
        VisualDepthGuard& operator=(const VisualDepthGuard&);
    };
}
