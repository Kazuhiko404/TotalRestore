#include "FleshCapRules.h"

#include <float.h>

namespace TotalRestore
{
    namespace FleshCapRules
    {
        // The foreign clamp writes age * HPMult * max - wear, the getter returns
        // age * max * HPMult - wear: equal up to float rounding.
        static const float kClampRelativeTolerance = 0.0001f;
        static const float kClampMinTolerance = 0.01f;

        static bool IsFinite(float value)
        {
            return _finite(value) != 0;
        }

        bool IsFleshCapClamp(float previousFlesh, float currentFlesh, float maxHealth)
        {
            if (!IsFinite(previousFlesh) || !IsFinite(currentFlesh) || !IsFinite(maxHealth)) return false;
            if (!(maxHealth > 0.0f)) return false;
            if (!(previousFlesh > maxHealth)) return false;

            float tolerance = kClampRelativeTolerance * maxHealth;
            if (tolerance < kClampMinTolerance) tolerance = kClampMinTolerance;

            const float distance = (currentFlesh > maxHealth) ? (currentFlesh - maxHealth) : (maxHealth - currentFlesh);
            return distance <= tolerance;
        }
    }
}
