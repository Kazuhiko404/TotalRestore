cmake_minimum_required(VERSION 3.20)
project(TotalRestore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DLL Output setup
add_library(TotalRestore SHARED
    src/TotalRestore.h
    src/TotalRestore.cpp
    src/Config.cpp
    src/LimbToughness.cpp
    src/HealingEngine.cpp
    src/ReviveSystem.cpp
    src/ToughnessScaling.h
    src/ToughnessScaling.cpp
    src/LimbMaxRules.h
    src/LimbMaxRules.cpp
    src/LimbBaselineCache.h
    src/LimbBaselineCache.cpp
    src/LimbToughness.h
    src/RegenRules.h
    src/RegenRules.cpp
    src/RegenStateRegistry.h
    src/RegenStateRegistry.cpp
    src/HealingEngine.h
    src/DamageSignals.cpp
    src/LimbRegrowth.h
    src/LimbRegrowth.cpp
    src/CriticalSection.h
    src/ReviveRules.h
    src/ReviveRules.cpp
    src/DeathTracker.h
    src/DeathTracker.cpp
    src/RegrowthRules.h
    src/RegrowthRules.cpp
    src/LimbRegrowthState.h
    src/LimbRegrowthState.cpp
    src/LimbBoneSnapshot.h
    src/LimbBoneSnapshot.cpp
    src/LimbShadowHooks.h
    src/LimbShadowHooks.cpp
    src/MedicalUpdateHook.h
    src/MedicalUpdateHook.cpp
    src/FleshCapRules.h
    src/FleshCapRules.cpp
    src/ExternalModCompat.h
    src/ExternalModCompat.cpp
    src/LimbMaxOwnership.h
    src/LimbMaxOwnership.cpp
)

target_include_directories(TotalRestore PRIVATE
    src
    reference
)

# Target properties for RE_Kenshi plugin
set_target_properties(TotalRestore PROPERTIES
    PREFIX ""
    OUTPUT_NAME "TotalRestore"
)
