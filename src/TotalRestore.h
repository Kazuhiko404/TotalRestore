#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>

#include <Debug.h>
#include <core/Functions.h>

#include <kenshi/Kenshi.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/CharStats.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Platoon.h>
#include <kenshi/GameData.h>
#include <kenshi/Appearance.h>
#include <kenshi/gui/ForgottenGUI.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>

#include "LimbMaxRules.h"
#include "RegenRules.h"

namespace TotalRestore
{
    struct Config
    {
        bool enableMod;
        bool applyToAllPlayerHumanoids;
        std::string nameSuffix;

        // Limb Toughness
        bool enableLimbToughness;
        float healthPercentPerToughness;
        float maxToughnessMultiplier;
        bool applyToughnessToRobots;
        bool applyToughnessToAnimals;
        LimbBonusFormula limbBonusFormula; // Scaled (default) or Percent (legacy)
        float toughnessCap;               // Shared by the limb bonus and the regen formula
        float limbBonusAtReference;
        float limbBonusReferenceHealth;
        float limbBonusExponent;
        float limbBonusMaxPerPart;
        bool yieldMaxHealthToOtherMods;   // Leave _maxHealth to another mod that rewrites it (DesolationSystem)

        // Healing Factor
        bool enableBodyPartRegeneration;
        bool enableFleshRegeneration;
        bool enableStunRegeneration;
        bool enableWearRegeneration;
        HealingFormula healingFormula; // Toughness (default) or Legacy
        float regenCeilingMinFraction;
        float regenCeilingMaxFraction;
        float regenStartBelowFraction; // part regen only while health / max < this (1 = always)
        float regenRateMinPercentPerMinute;
        float regenRateMaxPercentPerMinute;
        bool pauseHealingWhileUnconscious;
        bool healSkeletonParts;
        bool healRobotLimbsOnOrganics;
        float woundRegenerationSeconds;
        float woundHealingDelaySeconds;
        bool pauseHealingInCombat;
        bool applyHealingToAnimals;      // Off by default: animals keep only their native self-heal
        bool tolerateExternalFleshClamp; // Watchdog ignores a foreign mod clamping flesh to the max (DesolationSystem)

        // Blood & Oil
        bool enableBloodRegeneration;
        float bloodRegenerationSeconds;
        bool pauseBloodDuringDamage;
        bool clearBleedingWhenHealed;

        // Death Protection
        bool enableDeathProtection;
        float emergencyVitalFraction;
        float emergencyBloodFraction;

        // Limb Regrowth
        bool enableLimbRegeneration;
        bool enableLeftArmRegeneration;
        bool enableRightArmRegeneration;
        bool enableLeftLegRegeneration;
        bool enableRightLegRegeneration;
        float limbRegenerationSeconds;
        float armRegenerationSeconds;
        float legRegenerationSeconds;
        float initialScale;
        float armInitialScale;
        float legInitialScale;
        float completionHealthFraction;
        float armCompletionHealthFraction;
        float legCompletionHealthFraction;
        std::string limbGrowthCurve;

        // Revive System
        bool enableReviveSystem;
        bool allowRevivingEnemies;
        bool enableReviveTimeLimit;
        float maxReviveTimeMinutes;
        float reviveHealthFraction;

        Config();
        void Load(const std::string& iniPath);
    };

    extern Config g_config;

    void Log(const std::string& msg);
    void LogError(const std::string& msg);

    // Module Initializers
    bool InitLimbToughness();
    bool InitHealingEngine();
    bool InitReviveSystem();
}
