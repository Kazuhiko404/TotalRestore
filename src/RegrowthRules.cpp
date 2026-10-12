#include "RegrowthRules.h"

#include <string.h>

namespace TotalRestore
{
    // HealingFactor INI clamps.
    static const float kMinRegenerationSeconds = 1.0f;
    static const float kMinInitialScale = 0.001f;
    static const float kMaxInitialScale = 0.5f;
    static const float kMinCompletionFraction = 0.01f;
    static const float kMaxCompletionFraction = 1.0f;
    static const float kMinCompletionFlesh = 1.0f;
    static const float kCollapsedMaxFraction = 0.25f;
    static const int kLimbCount = 4;

    RegrowthSettings::RegrowthSettings()
        : enabled(true)
        , regenerationSeconds(120.0f)
        , armRegenerationSeconds(0.0f)
        , legRegenerationSeconds(0.0f)
        , initialScale(0.03f)
        , armInitialScale(0.0f)
        , legInitialScale(0.0f)
        , completionHealthFraction(0.10f)
        , armCompletionHealthFraction(0.0f)
        , legCompletionHealthFraction(0.0f)
        , smoothCurve(true)
    {
        for (int i = 0; i < kLimbCount; ++i) limbEnabled[i] = true;
    }

    namespace RegrowthRules
    {
        static float Clamp(float value, float minValue, float maxValue)
        {
            if (!(value >= minValue)) return minValue;
            return (value > maxValue) ? maxValue : value;
        }

        // Per-limb override: > 0 wins (capped), otherwise the shared value.
        static float PickOverride(float limbValue, float sharedValue)
        {
            return (limbValue > 0.0f) ? limbValue : sharedValue;
        }

        bool IsArm(int limb)
        {
            return limb == 0 || limb == 1;
        }

        int MirrorLimb(int limb)
        {
            switch (limb)
            {
                case 0: return 1;
                case 1: return 0;
                case 2: return 3;
                case 3: return 2;
                default: return -1;
            }
        }

        bool IsLimbEnabled(const RegrowthSettings& settings, int limb)
        {
            if (!settings.enabled || limb < 0 || limb >= kLimbCount) return false;
            return settings.limbEnabled[limb];
        }

        bool AnyLimbEnabled(const RegrowthSettings& settings)
        {
            for (int limb = 0; limb < kLimbCount; ++limb)
            {
                if (IsLimbEnabled(settings, limb)) return true;
            }
            return false;
        }

        float GrowthSeconds(const RegrowthSettings& settings, int limb)
        {
            const float shared = (settings.regenerationSeconds >= kMinRegenerationSeconds) ? settings.regenerationSeconds : kMinRegenerationSeconds;
            return PickOverride(IsArm(limb) ? settings.armRegenerationSeconds : settings.legRegenerationSeconds, shared);
        }

        float InitialScale(const RegrowthSettings& settings, int limb)
        {
            const float shared = Clamp(settings.initialScale, kMinInitialScale, kMaxInitialScale);
            const float picked = PickOverride(IsArm(limb) ? settings.armInitialScale : settings.legInitialScale, shared);
            return Clamp(picked, kMinInitialScale, kMaxInitialScale);
        }

        float CompletionFraction(const RegrowthSettings& settings, int limb)
        {
            const float shared = Clamp(settings.completionHealthFraction, kMinCompletionFraction, kMaxCompletionFraction);
            const float picked = PickOverride(IsArm(limb) ? settings.armCompletionHealthFraction : settings.legCompletionHealthFraction, shared);
            return Clamp(picked, kMinCompletionFraction, kMaxCompletionFraction);
        }

        bool ParseSmoothCurve(const char* text)
        {
            return text == NULL || _stricmp(text, "Linear") != 0;
        }

        float GrowthScale(float elapsed, float growthSeconds, float initScale, bool smooth)
        {
            float progress = (growthSeconds > 0.0f) ? elapsed / growthSeconds : 1.0f;
            progress = Clamp(progress, 0.0f, 1.0f);
            if (smooth)
            {
                progress = progress * progress * (3.0f - 2.0f * progress);
            }
            return initScale + (1.0f - initScale) * progress;
        }

        float CompletionFlesh(float fraction, float maxHealth, float ceiling, bool useCeiling)
        {
            float flesh = fraction * maxHealth;
            if (useCeiling && flesh > ceiling) flesh = ceiling;
            return (flesh > kMinCompletionFlesh) ? flesh : kMinCompletionFlesh;
        }

        SnapshotSource ChooseSnapshotSource(bool ownSnapshotValid, bool mirrorSnapshotValid)
        {
            if (ownSnapshotValid) return SNAPSHOT_OWN;
            return mirrorSnapshotValid ? SNAPSHOT_MIRROR : SNAPSHOT_WAIT;
        }

        RawStateVerdict EvaluateRawState(int rawState, bool shadow)
        {
            const int kOriginal = 0;
            const int kStump = 1;
            const int kReplaced = 2;
            if (rawState == kReplaced) return RAW_STATE_STOP_REPLACED;
            return (rawState == (shadow ? kOriginal : kStump)) ? RAW_STATE_CONTINUE : RAW_STATE_STOP_CHANGED;
        }

        float PositionalSizeFactor(int boneIndex, float scale)
        {
            return (boneIndex == 0) ? 1.0f : scale;
        }

        bool NeedsMaxRepair(float limbMax, float referenceMax)
        {
            return limbMax > 0.0f && referenceMax > 0.0f && limbMax < kCollapsedMaxFraction * referenceMax;
        }
    }
}
