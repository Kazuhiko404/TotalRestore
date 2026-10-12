# TotalRestore - Kenshi Native Plugin (RE_Kenshi)

Plugin unificado y avanzado para **Kenshi** a través de **RE_Kenshi**.
Combina y expande las características de:
1. **Adaptive Limb Toughness** (Escalado orgánico y robótico de vida por Toughness).
2. **Advanced Healing Factor** (Regeneración de heridas, sangre/aceite y extremidades con compatibilidad total de esqueletos).
3. **Kenshi Revive Integration** (Capacidad de revivir/resucitar personajes caídos vía GUI integrada).

---


## 🏗️ Arquitectura del Proyecto

Cada responsabilidad vive en su propio módulo. Las reglas puras (sin includes del juego) están cubiertas por tests unitarios; los módulos que tocan el juego solo leen el estado del motor, llaman a esas reglas y escriben el resultado.

```
TotalRestore/
├── TotalRestore.vcxproj        # Proyecto del plugin (toolset v100, MSBuild)
├── CMakeLists.txt              # Lista de fuentes de referencia
├── TotalRestore.ini            # Archivo de configuración central unificado
├── src/
│   ├── TotalRestore.h/.cpp     # Config, logging y punto de entrada (startPlugin)
│   ├── Config.cpp              # Parser del .ini
│   │   ── Reglas puras (testeadas) ──
│   ├── ToughnessScaling.*      # s, techo de regeneración, velocidad, bono de vida por extremidad, reescalado de carne
│   ├── LimbMaxRules.*          # Fórmulas Scaled / Percent, estimación inversa de la base, filtros robot/animal
│   ├── LimbBaselineCache.*     # Base sin bono por (parte, raza, prótesis); la primera captura gana
│   ├── RegenRules.*            # dt de simulación, watchdog de daño, elegibilidad de partes, techo, fórmula Legacy
│   ├── RegenStateRegistry.*    # Estado por personaje con sello de identidad (handle)
│   ├── ReviveRules.*           # Reloj de muerte por handle, ventana de revive, fracción de vida
│   ├── RegrowthRules.*         # Curva de crecimiento, ajustes por extremidad, HP final, snapshot espejo
│   │   ── Módulos de juego ──
│   ├── LimbToughness.*         # Escribe el bono en _maxHealth (hooks setup/init/load/XP de Toughness)
│   ├── HealingEngine.*         # Tick por frame: carne, stun, desgaste, sangre/aceite, protección de muerte
│   ├── DamageSignals.cpp       # Hooks de daño (applyDamage + 7 daños ambientales) y limpieza de estado
│   ├── LimbRegrowth.*          # Ciclo de regeneración de extremidades (port de HealingFactor)
│   ├── LimbRegrowthState.*     # Estado por extremidad y contador de profundidad visual
│   ├── LimbBoneSnapshot.*      # Captura, crecimiento y restauración de huesos Bip01 (Ogre)
│   ├── LimbShadowHooks.*       # Hooks que mantienen las dos verdades de la extremidad
│   ├── MedicalUpdateHook.*     # Único hook de medicalUpdate (regrowth + DeathTracker)
│   ├── DeathTracker.*          # Registra el minuto de muerte (declareDead / medicalUpdate)
│   ├── ReviveSystem.cpp        # Botón de revive (MyGUI) y resurrección
│   └── CriticalSection.h       # Lock RAII mínimo (v100 no tiene std::mutex)
├── tests/
│   └── totalrestore_tests.vcxproj  # Tests unitarios de las reglas puras (consola, sin frameworks)
├── reference/                  # Código fuente original y desensamblados de referencia
└── docs/
    ├── REVERSE_ENGINEERING.md  # Hooks, offsets y el conflicto HealingFactor + LimbToughness
    └── QA-CHECKLIST.md         # Verificaciones en el juego para cada criterio de aceptación
```

---

## ⚙️ Características Técnicas

Todo es configurable en `TotalRestore.ini`. Los valores por defecto son el modo balanceado; el comportamiento anterior sigue disponible (por ejemplo un modo "cheat" con `LimbBonusFormula=Percent`, `HealingFormula=Legacy`, `EnableDeathProtection=1`, `RegenStartBelowFraction=1.0`).

Con Toughness `T`, todas las escalas usan `s = min(T, ToughnessCap) / ToughnessCap` (`ToughnessCap=100`).

### 1. Limb Toughness (vida máxima por extremidad)
* El bono se **escribe en `_maxHealth`** de cada parte. El clamp nativo (`MedicalSystem::validateHealthValues`) usa ese campo crudo, no el getter `maxHealth()`, así que el getter ya no se hookea.
* Fórmula `Scaled` (por defecto, invertida y con tope): `bono = s * 30 * (100 / base) ^ 0.25`, limitado a `[0, 40]` por parte. Con s = 1: base 80 → +31.7, 100 → +30, 200 → +25.2, 220 → +24.6. Las razas con menos vida base reciben un bono un poco mayor.
* Fórmula `Percent` (legado de LimbToughness): `base * min(1 + T * HealthPercentPerToughness / 100, MaxToughnessMultiplier)`.
* La base sin bono se captura una vez por (parte, raza) desde el argumento `_max` de `HealthPartStatus::setup` (la primera captura gana). Se reaplica tras `MedicalSystem::init`, `MedicalSystem::load` y la XP de Toughness. `updateStats` **no** se hookea.
* La carne se reescala proporcionalmente **sin redondear hacia abajo**.
* **Compatibilidad con partidas guardadas:** las partidas hechas con el LimbToughness original guardan un `_maxHealth` ya inflado. Al cargar, la base capturada en `setup` lo corrige a `base + bono` (ejemplo: T106, base 220 → ~244, no 530).
* Partes robóticas (esqueletos y prótesis) no reciben bono por defecto (`ApplyToughnessToRobots=0`).
* Los animales no reciben bono por defecto (`ApplyToughnessToAnimals=0`): con el bono y la curación se volvían prácticamente inmortales.
* Las partes que no se escalan (robots/animales con su interruptor en 0) vuelven a su base capturada en `setup` al iniciar, cargar y con la XP de Toughness. Así una partida guardada con un animal ya inflado recupera su máximo base. Las prótesis no tienen base capturada y no se tocan.

### 2. Healing & Regeneration Engine
* Tiempo: `dt = frameDelta * getFrameSpeedMultiplier()`; no hay tick en pausa ni con multiplicador < 0 o > 100.
* Fórmula `Toughness` (por defecto), sobre la base **sin** el bono de Toughness:
  * Techo: `base * lerp(0.10, 0.50, s)`. La carne solo se rellena hasta el techo y nunca se reduce si ya está por encima (por ejemplo tras un médico).
  * Velocidad: `base * lerp(1, 4, s)` % por minuto de simulación. Stun y desgaste se recuperan a la misma velocidad, sin techo.
* Umbral de inicio (`RegenStartBelowFraction`, por defecto 0.5): la regeneración pasiva solo se aplica a una parte cuya vida (carne menos stun, como la muestra la barra) está por debajo de esa fracción de su máximo. Una parte al 70% no se cura; una al 40% se cura hasta el techo o hasta el umbral, lo que llegue antes. Se evalúa por parte, así que una extremidad muy herida sigue curándose aunque el resto esté sano, y los personajes de nivel alto ya no se vuelven inmortales. Afecta a carne, stun y desgaste con ambas fórmulas; no cambia la sangre ni la regeneración de extremidades. `1.0` = curar siempre (comportamiento anterior), `0` = sin regeneración pasiva de partes.
* Fórmula `Legacy`: el comportamiento anterior (100% del máximo en `WoundRegenerationSeconds`, x0.5 para partes robóticas).
* Pausas: `WoundHealingDelaySeconds` tras recibir daño (golpes, ácido, gas, quemaduras, ser comido, viento, y un watchdog que detecta caídas de carne no explicadas), en combate y mientras está inconsciente (`PauseHealingWhileUnconscious=1`).
* Compatibilidad con DesolationSystem: ese mod también reescribe la vida máxima de cada parte (`_maxHealth`) tras cada `MedicalSystem::updateStats` y luego baja la carne hasta ese máximo. Esa bajada no es daño: con `TolerateExternalFleshClamp=1` (por defecto) el watchdog la ignora y la curación no se pausa. Sin DesolationSystem la opción no cambia nada. Para que los dos mods no se pisen, con `YieldMaxHealthToOtherMods=1` (por defecto) TotalRestore deja de escribir su bono en cada parte cuya vida máxima DesolationSystem ya reescribió: esa parte conserva el máximo de DesolationSystem, y las partes que DesolationSystem no gestiona (por ejemplo personajes sin línea base tras cargar) conservan el bono de TotalRestore. Los dos bonos no se suman en una misma parte.
* Animales: no reciben la curación de TotalRestore (heridas, sangre, regeneración de extremidades) por defecto (`ApplyHealingToAnimals=0`); solo conservan su autocuración nativa. La pausa de combate (`isInCombatMode`) no detecta a los animales en pelea, así que con `1` se curan en pleno combate. Con `ApplyHealingToAnimals=1` y `ApplyToughnessToAnimals=1` vuelve el comportamiento anterior.
* Esqueletos: sus partes se curan con la misma fórmula (`HealSkeletonParts=1`) y nunca regeneran extremidades. Las prótesis robóticas en personajes orgánicos no se curan (`HealRobotLimbsOnOrganics=0`).
* Sangre/aceite con la misma pausa; `ClearBleedingWhenFullyHealed` limpia también la pérdida extra de sangre de las partes.
* Protección de muerte (pisos de emergencia) sigue implementada pero desactivada por defecto (`EnableDeathProtection=0`): con ella el escuadrón era inmortal.
* El estado por personaje se limpia en `GameWorld::removeFromUpdateListMain` y lleva un sello del handle, así un puntero reutilizado no hereda estado viejo.

### 3. Regeneración de extremidades (port de HealingFactor)
* Una extremidad orgánica amputada de un humanoide del escuadrón vuelve a crecer de forma visible: los huesos `Bip01` del brazo/pierna crecen desde `InitialScale` hasta el tamaño capturado mientras estaba intacta, en `RegenerationSeconds` de tiempo simulado.
* Mientras crece sigue siendo un muñón para el juego (y en la partida guardada); el estado crudo es `LIMB_ORIGINAL` solo para que el motor dibuje la malla. Al recargar, el crecimiento vuelve a empezar.
* Al completar: huesos restaurados al tamaño capturado, extremidad usable y carne = `min(CompletionHealthFraction × max, techo de regeneración)` (con `HealingFormula=Legacy`, solo la fracción).
* Los esqueletos nunca regeneran extremidades; instalar una prótesis robótica detiene el crecimiento.

### 4. Revive System (Integración GUI)
* Hook en `ForgottenGUI::selectedObjectsChanged`; botón inyectado junto al `StatusPanel` al seleccionar un personaje muerto.
* La ventana de revive (`MaxReviveTimeMinutes`, minutos de juego) se mide **desde la muerte**: se registra en `Character::declareDead` y tras `MedicalSystem::medicalUpdate`, con clave en el handle del personaje (no en `Character*`). Si murió antes de cargar el plugin o en una partida guardada, cuenta desde la primera vez que se le ve muerto.
* Resurrección:
  1. `character->medical.dead = false;`
  2. `character->healCompletely();`
  3. Carne de cada parte y sangre escaladas a `ReviveHealthFraction` (0.01 – 1.0).
  4. `player_interface->recruit(deadCharacter, false);`

---

## 🛠️ Compilación y Tests
* **MSBuild** (Visual Studio) con el toolset **v100** y KenshiLib 0.5.0 en `deps/`.
* Plugin: `MSBuild TotalRestore/TotalRestore.vcxproj -p:Configuration=Release -p:Platform=x64` (si hace falta Boost: `BOOST_INCLUDE_PATH`).
* Tests: `MSBuild TotalRestore/tests/totalrestore_tests.vcxproj -p:Configuration=Release -p:Platform=x64` y ejecutar `TotalRestore/tests/x64/Release/totalrestore_tests.exe`.
* Verificación en el juego: `docs/QA-CHECKLIST.md`.
