#include "TotalRestore.h"
#include <fstream>
#include <sstream>

namespace TotalRestore
{
    Config::Config()
        : enableMod(true)
        , applyToAllPlayerHumanoids(true)
        , nameSuffix(".")
        , enableLimbToughness(true)
        , healthPercentPerToughness(0.5f)
        , maxToughnessMultiplier(1.5f)
        , applyToughnessToRobots(false)
        , applyToughnessToAnimals(false)
        , limbBonusFormula(LIMB_BONUS_SCALED)
        , toughnessCap(100.0f)
        , limbBonusAtReference(30.0f)
        , limbBonusReferenceHealth(100.0f)
        , limbBonusExponent(0.25f)
        , limbBonusMaxPerPart(40.0f)
        , yieldMaxHealthToOtherMods(true)
        , enableBodyPartRegeneration(true)
        , enableFleshRegeneration(true)
        , enableStunRegeneration(true)
        , enableWearRegeneration(true)
        , healingFormula(HEALING_FORMULA_TOUGHNESS)
        , regenCeilingMinFraction(0.10f)
        , regenCeilingMaxFraction(0.50f)
        , regenStartBelowFraction(0.50f)
        , regenRateMinPercentPerMinute(1.0f)
        , regenRateMaxPercentPerMinute(4.0f)
        , pauseHealingWhileUnconscious(true)
        , healSkeletonParts(true)
        , healRobotLimbsOnOrganics(false)
        , woundRegenerationSeconds(30.0f)
        , woundHealingDelaySeconds(5.0f)
        , pauseHealingInCombat(true)
        , applyHealingToAnimals(false)
        , tolerateExternalFleshClamp(true)
        , enableBloodRegeneration(true)
        , bloodRegenerationSeconds(240.0f)
        , pauseBloodDuringDamage(true)
        , clearBleedingWhenHealed(true)
        , enableDeathProtection(false)
        , emergencyVitalFraction(0.05f)
        , emergencyBloodFraction(0.10f)
        , enableLimbRegeneration(true)
        , enableLeftArmRegeneration(true)
        , enableRightArmRegeneration(true)
        , enableLeftLegRegeneration(true)
        , enableRightLegRegeneration(true)
        , limbRegenerationSeconds(120.0f)
        , armRegenerationSeconds(0.0f)
        , legRegenerationSeconds(0.0f)
        , initialScale(0.03f)
        , armInitialScale(0.0f)
        , legInitialScale(0.0f)
        , completionHealthFraction(0.10f)
        , armCompletionHealthFraction(0.0f)
        , legCompletionHealthFraction(0.0f)
        , limbGrowthCurve("Smooth")
        , enableReviveSystem(true)
        , allowRevivingEnemies(true)
        , enableReviveTimeLimit(true)
        , maxReviveTimeMinutes(60.0f)
        , reviveHealthFraction(0.10f)
    {
    }

    static std::string Trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    void Config::Load(const std::string& iniPath)
    {
        std::ifstream file(iniPath.c_str());
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = Trim(line.substr(0, eqPos));
            std::string val = Trim(line.substr(eqPos + 1));

            if (key == "EnableMod") enableMod = (val == "1");
            else if (key == "ApplyToAllPlayerHumanoids") applyToAllPlayerHumanoids = (val == "1");
            else if (key == "NameSuffix") nameSuffix = val;
            else if (key == "EnableLimbToughness") enableLimbToughness = (val == "1");
            else if (key == "HealthPercentPerToughness") healthPercentPerToughness = (float)atof(val.c_str());
            else if (key == "MaxToughnessMultiplier") maxToughnessMultiplier = (float)atof(val.c_str());
            else if (key == "ApplyToughnessToRobots") applyToughnessToRobots = (val == "1");
            else if (key == "ApplyToughnessToAnimals") applyToughnessToAnimals = (val == "1");
            else if (key == "LimbBonusFormula") limbBonusFormula = LimbMaxRules::ParseFormula(val.c_str(), limbBonusFormula);
            else if (key == "ToughnessCap") toughnessCap = (float)atof(val.c_str());
            else if (key == "LimbBonusAtReference") limbBonusAtReference = (float)atof(val.c_str());
            else if (key == "LimbBonusReferenceHealth") limbBonusReferenceHealth = (float)atof(val.c_str());
            else if (key == "LimbBonusExponent") limbBonusExponent = (float)atof(val.c_str());
            else if (key == "LimbBonusMaxPerPart") limbBonusMaxPerPart = (float)atof(val.c_str());
            else if (key == "EnableBodyPartRegeneration") enableBodyPartRegeneration = (val == "1");
            else if (key == "EnableFleshRegeneration") enableFleshRegeneration = (val == "1");
            else if (key == "EnableStunRegeneration") enableStunRegeneration = (val == "1");
            else if (key == "EnableWearRegeneration") enableWearRegeneration = (val == "1");
            else if (key == "HealingFormula") healingFormula = RegenRules::ParseFormula(val.c_str(), healingFormula);
            else if (key == "RegenCeilingMinFraction") regenCeilingMinFraction = (float)atof(val.c_str());
            else if (key == "RegenCeilingMaxFraction") regenCeilingMaxFraction = (float)atof(val.c_str());
            else if (key == "RegenStartBelowFraction") regenStartBelowFraction = RegenRules::ClampStartBelowFraction((float)atof(val.c_str()));
            else if (key == "RegenRateMinPercentPerMinute") regenRateMinPercentPerMinute = (float)atof(val.c_str());
            else if (key == "RegenRateMaxPercentPerMinute") regenRateMaxPercentPerMinute = (float)atof(val.c_str());
            else if (key == "PauseHealingWhileUnconscious") pauseHealingWhileUnconscious = (val == "1");
            else if (key == "HealSkeletonParts") healSkeletonParts = (val == "1");
            else if (key == "HealRobotLimbsOnOrganics") healRobotLimbsOnOrganics = (val == "1");
            else if (key == "WoundRegenerationSeconds") woundRegenerationSeconds = (float)atof(val.c_str());
            else if (key == "WoundHealingDelaySeconds") woundHealingDelaySeconds = (float)atof(val.c_str());
            else if (key == "PauseHealingInCombat") pauseHealingInCombat = (val == "1");
            else if (key == "ApplyHealingToAnimals") applyHealingToAnimals = (val == "1");
            else if (key == "TolerateExternalFleshClamp") tolerateExternalFleshClamp = (val == "1");
            else if (key == "YieldMaxHealthToOtherMods") yieldMaxHealthToOtherMods = (val == "1");
            else if (key == "EnableBloodRegeneration") enableBloodRegeneration = (val == "1");
            else if (key == "BloodRegenerationSeconds") bloodRegenerationSeconds = (float)atof(val.c_str());
            else if (key == "PauseBloodRegenerationDuringDamage") pauseBloodDuringDamage = (val == "1");
            else if (key == "ClearBleedingWhenFullyHealed") clearBleedingWhenHealed = (val == "1");
            else if (key == "EnableDeathProtection") enableDeathProtection = (val == "1");
            else if (key == "EmergencyVitalHealthFraction") emergencyVitalFraction = (float)atof(val.c_str());
            else if (key == "EmergencyBloodFraction") emergencyBloodFraction = (float)atof(val.c_str());
            else if (key == "EnableLimbRegeneration") enableLimbRegeneration = (val == "1");
            else if (key == "EnableLeftArmRegeneration") enableLeftArmRegeneration = (val == "1");
            else if (key == "EnableRightArmRegeneration") enableRightArmRegeneration = (val == "1");
            else if (key == "EnableLeftLegRegeneration") enableLeftLegRegeneration = (val == "1");
            else if (key == "EnableRightLegRegeneration") enableRightLegRegeneration = (val == "1");
            else if (key == "RegenerationSeconds") limbRegenerationSeconds = (float)atof(val.c_str());
            else if (key == "ArmRegenerationSeconds") armRegenerationSeconds = (float)atof(val.c_str());
            else if (key == "LegRegenerationSeconds") legRegenerationSeconds = (float)atof(val.c_str());
            else if (key == "InitialScale") initialScale = (float)atof(val.c_str());
            else if (key == "ArmInitialScale") armInitialScale = (float)atof(val.c_str());
            else if (key == "LegInitialScale") legInitialScale = (float)atof(val.c_str());
            else if (key == "CompletionHealthFraction") completionHealthFraction = (float)atof(val.c_str());
            else if (key == "ArmCompletionHealthFraction") armCompletionHealthFraction = (float)atof(val.c_str());
            else if (key == "LegCompletionHealthFraction") legCompletionHealthFraction = (float)atof(val.c_str());
            else if (key == "LimbGrowthCurve") limbGrowthCurve = val;
            else if (key == "EnableReviveSystem") enableReviveSystem = (val == "1");
            else if (key == "AllowRevivingEnemies") allowRevivingEnemies = (val == "1");
            else if (key == "EnableReviveTimeLimit") enableReviveTimeLimit = (val == "1");
            else if (key == "MaxReviveTimeMinutes") maxReviveTimeMinutes = (float)atof(val.c_str());
            else if (key == "ReviveHealthFraction") reviveHealthFraction = (float)atof(val.c_str());
        }
    }
}
