#pragma once

#include "LimbRegrowthState.h"

namespace Ogre { class OldSkeletonInstance; }

/*
 * LimbBoneSnapshot
 * Ogre side of the regrowth: captures the intact Bip01 limb bones (size,
 * positional size, local scale), scales them while the limb grows, and restores
 * the captured values at the end. The four OldBone size functions are not in the
 * KenshiLib Ogre headers and are resolved from OgreMain_x64.dll.
 */

namespace TotalRestore
{
    namespace LimbBoneSnapshot
    {
        // Resolves the OgreMain exports. False when any of the four is missing.
        bool ResolveOgreExports();

        Ogre::OldSkeletonInstance* SkeletonOf(Character* character);

        // Captures the limb's bones into the entry (HealingFactor capture).
        bool Capture(Character* character, int limb, LimbEntry& entry);

        // Copies the mirror limb's captured values under this limb's bone names.
        bool CopyFromMirror(Character* character, int limb, const LimbEntry& mirror, LimbEntry& entry);

        // Rebinds the snapshot to a rebuilt skeleton when every bone still exists.
        bool Rebind(LimbEntry& entry, Ogre::OldSkeletonInstance* skeleton);

        void Invalidate(LimbEntry& entry);

        // size = captured * scale; the root keeps its positional size. False if a bone is missing.
        bool ApplyScale(Character* character, LimbEntry& entry, float scale);

        // Writes the captured values back (missing bones are skipped).
        void Restore(Character* character, LimbEntry& entry);
    }
}
