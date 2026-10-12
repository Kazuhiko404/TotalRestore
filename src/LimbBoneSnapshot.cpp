#include "LimbBoneSnapshot.h"
#include "RegrowthRules.h"

#include <ogre/OgreOldSkeletonInstance.h>
#include <ogre/OgreOldBone.h>
#include <ogre/OgreOldNode.h>

namespace TotalRestore
{
    namespace LimbBoneSnapshot
    {
        typedef const Ogre::Vector3& (*FnGetVector)(const Ogre::OldBone*);
        typedef void (*FnSetVector)(Ogre::OldBone*, const Ogre::Vector3&);

        static FnGetVector g_getBoneSize = NULL;
        static FnGetVector g_getBonePositionalSize = NULL;
        static FnSetVector g_setBoneSize = NULL;
        static FnSetVector g_setBonePositionalSize = NULL;

        bool ResolveOgreExports()
        {
            HMODULE ogre = GetModuleHandleA("OgreMain_x64.dll");
            if (ogre == NULL) return false;

            g_getBoneSize = (FnGetVector)GetProcAddress(ogre, "?getBoneSize@OldBone@Ogre@@QEBAAEBVVector3@2@XZ");
            g_getBonePositionalSize = (FnGetVector)GetProcAddress(ogre, "?getBonePositionalSize@OldBone@Ogre@@QEBAAEBVVector3@2@XZ");
            g_setBoneSize = (FnSetVector)GetProcAddress(ogre, "?setBoneSize@OldBone@Ogre@@QEAAXAEBVVector3@2@@Z");
            g_setBonePositionalSize = (FnSetVector)GetProcAddress(ogre, "?setBonePositionalSize@OldBone@Ogre@@QEAAXAEBVVector3@2@@Z");

            return g_getBoneSize != NULL && g_getBonePositionalSize != NULL && g_setBoneSize != NULL && g_setBonePositionalSize != NULL;
        }

        static bool ExportsReady()
        {
            return g_getBoneSize != NULL && g_getBonePositionalSize != NULL && g_setBoneSize != NULL && g_setBonePositionalSize != NULL;
        }

        // Exact bone hierarchy from HealingFactor; index 0 is the limb root.
        static void BoneNames(int limb, std::vector<std::string>& out)
        {
            const char* side = (limb == 0 || limb == 2) ? "L" : "R";
            if (RegrowthRules::IsArm(limb))
            {
                out.push_back(std::string("Bip01 ") + side + " UpperArm");
                out.push_back(std::string("Bip01 ") + side + " Forearm");
                out.push_back(std::string("Bip01 ") + side + " Hand");
            }
            else
            {
                out.push_back(std::string("Bip01 ") + side + " Thigh");
                out.push_back(std::string("Bip01 ") + side + " Calf");
                out.push_back(std::string("Bip01 ") + side + " Foot");
                out.push_back(std::string("Bip01 ") + side + " Toe0");
            }
        }

        static void MarkDirty(Ogre::OldBone* bone)
        {
            // OldBone redeclares needUpdate non-virtually; dispatch through OldNode.
            static_cast<Ogre::OldNode*>(bone)->needUpdate(true);
        }

        Ogre::OldSkeletonInstance* SkeletonOf(Character* character)
        {
            if (character == NULL) return NULL;
            AppearanceBase* appearance = character->getAppearance();
            return (appearance != NULL) ? appearance->getSkeleton() : NULL;
        }

        void Invalidate(LimbEntry& entry)
        {
            entry.snapValid = false;
            entry.snapSkel = NULL;
            entry.bones.clear();
        }

        bool Capture(Character* character, int limb, LimbEntry& entry)
        {
            Ogre::OldSkeletonInstance* skeleton = SkeletonOf(character);
            if (skeleton == NULL || !ExportsReady()) return false;

            std::vector<std::string> names;
            BoneNames(limb, names);

            Invalidate(entry);
            for (size_t i = 0; i < names.size(); ++i)
            {
                if (!skeleton->hasBone(names[i]))
                {
                    Log("Regrowth: skeleton is missing expected bone " + names[i]);
                    entry.bones.clear();
                    return false;
                }
                Ogre::OldBone* bone = skeleton->getBone(names[i]);
                BoneRecord record;
                record.name = names[i];
                record.boneSize = g_getBoneSize(bone);
                record.posSize = g_getBonePositionalSize(bone);
                record.scale = bone->getScale();
                entry.bones.push_back(record);
            }
            entry.snapValid = true;
            entry.snapSkel = skeleton;
            return true;
        }

        bool CopyFromMirror(Character* character, int limb, const LimbEntry& mirror, LimbEntry& entry)
        {
            Ogre::OldSkeletonInstance* skeleton = SkeletonOf(character);
            if (skeleton == NULL || !mirror.snapValid) return false;

            std::vector<std::string> names;
            BoneNames(limb, names);
            if (names.size() != mirror.bones.size()) return false;

            std::vector<BoneRecord> copied;
            for (size_t i = 0; i < names.size(); ++i)
            {
                if (!skeleton->hasBone(names[i])) return false;
                BoneRecord record = mirror.bones[i];
                record.name = names[i];
                copied.push_back(record);
            }
            entry.bones.swap(copied);
            entry.snapValid = true;
            entry.snapSkel = skeleton;
            return true;
        }

        bool Rebind(LimbEntry& entry, Ogre::OldSkeletonInstance* skeleton)
        {
            if (skeleton == NULL || entry.bones.empty()) return false;
            for (size_t i = 0; i < entry.bones.size(); ++i)
            {
                if (!skeleton->hasBone(entry.bones[i].name)) return false;
            }
            entry.snapSkel = skeleton;
            return true;
        }

        bool ApplyScale(Character* character, LimbEntry& entry, float scale)
        {
            Ogre::OldSkeletonInstance* skeleton = SkeletonOf(character);
            if (skeleton == NULL || skeleton != entry.snapSkel || !ExportsReady()) return false;

            for (size_t i = 0; i < entry.bones.size(); ++i)
            {
                const BoneRecord& record = entry.bones[i];
                if (!skeleton->hasBone(record.name)) return false;

                Ogre::OldBone* bone = skeleton->getBone(record.name);
                bone->setScale(record.scale); // the captured local scale, not multiplied
                g_setBoneSize(bone, record.boneSize * scale);
                g_setBonePositionalSize(bone, record.posSize * RegrowthRules::PositionalSizeFactor((int)i, scale));
                MarkDirty(bone);
            }
            return true;
        }

        void Restore(Character* character, LimbEntry& entry)
        {
            if (!entry.snapValid) return;
            Ogre::OldSkeletonInstance* skeleton = SkeletonOf(character);
            if (skeleton == NULL || skeleton != entry.snapSkel || !ExportsReady()) return;

            for (size_t i = 0; i < entry.bones.size(); ++i)
            {
                const BoneRecord& record = entry.bones[i];
                if (!skeleton->hasBone(record.name)) continue;

                Ogre::OldBone* bone = skeleton->getBone(record.name);
                bone->setScale(record.scale);
                g_setBoneSize(bone, record.boneSize);
                g_setBonePositionalSize(bone, record.posSize);
                MarkDirty(bone);
            }
        }
    }
}
