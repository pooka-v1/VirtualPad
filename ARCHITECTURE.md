# Architecture — VirtualPad

> Decisiones de diseño estructural. No es contexto operativo (→ SESSION_CONTEXT.md)
> ni referencia de hardware/protocolo (→ REFERENCE.md).
> Actualizar cuando se tome una decisión que cambie la estructura.

---

## Component System (2026/04/21)

Diseño del sistema de componentes físicos → virtuales.
Objetivo: reemplazar los loops + switches centrales de `HIDInputSource::read()`
por objetos que saben procesarse a sí mismos.

### Decisiones clave

- **`std::variant`** para los tipos de componente (conjunto cerrado, sin heap, sin vtable).
- **`VirtualTarget` como descriptor**, no objeto. `GamepadState` sigue plana (requerimiento ViGEm).
- **`std::array` + `std::optional`** para el mapa de componentes dentro de `PhysicalController`
  (acceso O(1) por ComponentId, sin hash de strings en el hot path).
- **`StickAccumulator` / `GyroAccumulator`**: los semiejes del mismo eje se coordinan
  antes de escribir en `GamepadState`. Elimina el bug de escritura doble.
- **Analog vs binario**: determinado por el tipo del `VirtualTarget`, no por un flag.
  `VirtualTrigger`, `VirtualStickSlot`, `VirtualMouseMove`, `VirtualPassthrough` → proporcional.
  Todo lo demás → binario (umbral).
- **`RangedHalfAxis`** compartido entre `PhysicalTrigger`, `PhysicalAnalogDir` y `PhysicalGyro`.
  Lista vacía = passthrough proporcional implícito.

---

### Enums de soporte

```cpp
enum class ComponentId : uint8_t {
    BtnA, BtnB, BtnX, BtnY,
    BtnLB, BtnRB, BtnL3, BtnR3,
    BtnBack, BtnStart, BtnHome,
    DpadUp, DpadDown, DpadLeft, DpadRight,
    TriggerL, TriggerR,
    LeftXPos,  LeftXNeg,  LeftYPos,  LeftYNeg,
    RightXPos, RightXNeg, RightYPos, RightYNeg,
    Touchpad, Gyro,
    _Count   // siempre el último — usado para dimensionar el array
};

enum class ButtonId : uint8_t {
    A, B, X, Y, LB, RB, L3, R3, Back, Start, Home
};

enum class DpadDir     : uint8_t { Up, Down, Left, Right };
enum class TriggerSide : uint8_t { L, R };

enum class StickSlotId : uint8_t {
    LeftXPos,  LeftXNeg,  LeftYPos,  LeftYNeg,
    RightXPos, RightXNeg, RightYPos, RightYNeg
};

enum class GyroHalf    : uint8_t { XPos, XNeg, YPos, YNeg, ZPos, ZNeg };
enum class MouseAxis   : uint8_t { X, Y };
enum class MouseButton : uint8_t { Left, Right, Middle, Forward, Back };
```

---

### VirtualTarget

Descriptor de qué escribir en `GamepadState`. No es un objeto con vida propia.

```cpp
struct VirtualButton     { ButtonId    id;                    };
struct VirtualDpadDir    { DpadDir     dir;                   };
struct VirtualTrigger    { TriggerSide side;                  }; // proporcional
struct VirtualStickSlot  { StickSlotId slot;                  }; // → StickAccumulator
struct VirtualKeyboard   { std::vector<uint8_t> keys;         };
struct VirtualMacro      { std::string name;                  };
struct VirtualMouseClick { MouseButton button;                };
struct VirtualMouseMove  { MouseAxis axis; float speed;       }; // proporcional
struct VirtualBot        { std::string name;                  }; // arrancar/parar bot por nombre
struct VirtualPassthrough{                                    }; // fluye al equivalente natural

using VirtualTarget = std::variant<
    VirtualButton,
    VirtualDpadDir,
    VirtualTrigger,
    VirtualStickSlot,
    VirtualKeyboard,
    VirtualMacro,
    VirtualMouseClick,
    VirtualMouseMove,
    VirtualBot,
    VirtualPassthrough
>;
```

---

### Range y RangedHalfAxis

```cpp
struct Range {
    float         from;    // [0.0, 1.0]
    float         to;      // [0.0, 1.0]
    VirtualTarget target;
};

struct RangedHalfAxis {
    std::vector<Range> ranges;
    // vacío = VirtualPassthrough implícito
};
```

Usado por `PhysicalTrigger`, `PhysicalAnalogDir` y `PhysicalGyro`.
El tipo del `VirtualTarget` dentro de cada `Range` determina si el comportamiento es
proporcional (valor escala dentro del rango) o binario (umbral activa).

---

### Tipos físicos

```cpp
// Binarios — un target directo, sin rangos
struct PhysicalButton {
    uint8_t       bit;     // posición en HID report (1-based)
    VirtualTarget target;
};

struct PhysicalDpadDir {
    DpadDir       dir;
    VirtualTarget target;
};

// Con rangos — valor continuo [0.0, 1.0]
struct PhysicalTrigger {
    TriggerSide    side;
    RangedHalfAxis axis;
};

struct PhysicalAnalogDir {
    StickSlotId    slot;   // posición física: LeftXPos, RightYNeg, ...
    RangedHalfAxis axis;
};

struct PhysicalGyro {
    RangedHalfAxis xPos, xNeg;
    RangedHalfAxis yPos, yNeg;
    RangedHalfAxis zPos, zNeg;
    // Mismo modelo que analógico pero en 3 ejes.
    // XInput no tiene salida de giroscopio: los targets son VirtualStickSlot,
    // VirtualMouseMove, VirtualButton, etc.
};

// Placeholder — crecerá con gestos, zonas táctiles, combos de dos dedos, etc.
struct PhysicalTouchpad {
    TouchpadConfig cfg;
    // PhysicalButton del click físico va en components[ComponentId::BtnHome] o similar,
    // NO aquí. El touchpad surface es independiente del botón de click.
};

using PhysicalComponent = std::variant<
    PhysicalButton,
    PhysicalDpadDir,
    PhysicalTrigger,
    PhysicalAnalogDir,
    PhysicalTouchpad,
    PhysicalGyro
>;
```

---

### Accumulators

Los semiejes de un mismo eje coordinan su resultado antes de escribir en `GamepadState`.
Evita escrituras dobles y resuelve el bug de "golpes" en analógicos.

```cpp
struct StickAccumulator {
    float xPos = 0, xNeg = 0, yPos = 0, yNeg = 0;

    void flush(float& outX, float& outY) const {
        float vx = xPos - xNeg, vy = yPos - yNeg;
        float mag = std::sqrt(vx * vx + vy * vy);
        if (mag > 1.0f) { vx /= mag; vy /= mag; }
        outX = vx;
        outY = vy;
    }
};

struct GyroAccumulator {
    float xPos = 0, xNeg = 0;
    float yPos = 0, yNeg = 0;
    float zPos = 0, zNeg = 0;

    void flush(float& outX, float& outY, float& outZ) const {
        outX = std::clamp(xPos - xNeg, -1.0f, 1.0f);
        outY = std::clamp(yPos - yNeg, -1.0f, 1.0f);
        outZ = std::clamp(zPos - zNeg, -1.0f, 1.0f);
    }
};
```

`StickAccumulator` normaliza si la magnitud supera 1 (movimiento diagonal).
`GyroAccumulator` clampea — los ejes de rotación son independientes entre sí.

---

### PhysicalController

```cpp
static constexpr size_t kComponentCount = static_cast<size_t>(ComponentId::_Count);

struct PhysicalController {
    std::string name;
    uint16_t    vid, pid;

    std::array<std::optional<PhysicalComponent>, kComponentCount> components;

    uint8_t         playerPosition = 1;   // 1..N — LEDs indicadores del mando físico
    VibrationConfig vibration;
    LEDConfig       led;                  // solo PS4 (color + intensidad)

    void process(GamepadState& state) const;

    std::optional<PhysicalComponent>& operator[](ComponentId id) {
        return components[static_cast<size_t>(id)];
    }
    const std::optional<PhysicalComponent>& operator[](ComponentId id) const {
        return components[static_cast<size_t>(id)];
    }
};
```

`playerPosition`: investigar protocolo 8BitDo Pro 2/Pro 3 para enviar posición de jugador
(útil cuando dos mandos físicos comparten un mando virtual). Windows siempre enciende
el mismo LED; esto es futuro.

#### process() — hot path

```cpp
void PhysicalController::process(GamepadState& state) const {
    StickAccumulator accumLeft, accumRight;
    GyroAccumulator  accumGyro;

    for (const auto& opt : components) {
        if (!opt) continue;
        std::visit([&](const auto& c) {
            c.process(state, accumLeft, accumRight, accumGyro);
        }, *opt);
    }

    accumLeft .flush(state.leftX,  state.leftY);
    accumRight.flush(state.rightX, state.rightY);
    accumGyro .flush(state.gyroX,  state.gyroY, state.gyroZ);
}
```

Cada tipo físico implementa `process(GamepadState&, StickAccumulator&, StickAccumulator&, GyroAccumulator&)`.
Sin switches centrales. Sin recorridos múltiples.

---

### Pendiente de diseñar

- `VirtualController`: contiene bots, mando virtual seleccionable (XInput / DInput futuro)
- Separación de `LightningBot` y futuros bots del resto del código.
- `PhysicalTouchpad`: diseño de gestos, zonas táctiles, combos de dos dedos.
- Protocolo `playerPosition` para 8BitDo Pro 2/Pro 3.

---

## Sistema de capas de configuración (diseñado 2026/04/22)

### Modelo de tres niveles

```
Base  (controllers.json)
  └── + Perfil activo  (FinalFantasyX.json, etc.)   ← manual, un solo slot
        └── + Capa de modificador  (LP / RP held)   ← runtime, botón mantenido
```

Cada nivel es un **delta** sobre el anterior: solo especifica lo que cambia, el resto hereda. Los tres niveles son estructuralmente idénticos (conjunto de overrides de componentes); lo que los distingue es el ciclo de vida y el trigger de activación.

### Perfil activo

- Mismo concepto que los perfiles de juego actuales (`FinalFantasyX.json`, `MonsterHunterStories2.json`).
- Se carga manualmente. Un solo slot activo a la vez.
- El usuario decide si el contenido es preferencias personales, ajustes de juego, o ambos.
- No hay distinción técnica entre "perfil de jugador" y "perfil de juego" — es el mismo mecanismo.

### Capas de modificador

#### Decisiones clave

- **Cualquier componente físico puede ser modificador**: botón, dirección de dpad, gatillo, dirección de analógico, touchpad, giroscopio. LP y RP son los candidatos naturales en 8BitDo Pro 2/Pro 3, pero el sistema es genérico. `ComponentId` ya cubre todos los casos.
- **Cualquier combinación de modificadores activa un estado distinto**: si hay N modificadores declarados, hay hasta 2^N estados posibles. Solo existen en el mapa los que el usuario define explícitamente.
- **`ModifierMask`**: bitmask dinámico — cada modificador recibe un bit según su posición en `modifierButtons`. `uint8_t` soporta hasta 8 modificadores (256 estados). Más de 2 simultáneos es funcionalmente complejo pero arquitectónicamente posible.
- **Delta sobre el estado combinado**: la capa se aplica sobre (base + perfil activo), no solo sobre la base.
- **Parcial o completo a elección del usuario**: si se definen 2 overrides, 2 cambian; si se definen 20, 20 cambian. El sistema no distingue.
- **Mientras se mantiene pulsado**: al soltar, el estado vuelve a (base + perfil activo) inmediatamente.
- **Los modificadores pueden tener su propio `VirtualTarget`**: un botón modificador puede además producir una acción propia (en la capa base o en otra capa). No son exclusivamente modificadores salvo que el usuario así lo configure.

#### Estructura de datos (relevante para P1 — Component System)

```cpp
// Bitmask dinámico: el bit i corresponde al modificador en posición i de modifierButtons.
// uint8_t → hasta 8 modificadores (256 combinaciones posibles).
using ModifierMask = uint8_t;
constexpr ModifierMask kModNone = 0x00;

struct PhysicalController {
    std::string name;
    uint16_t    vid, pid;

    // Qué ComponentIds actúan como modificadores (orden define el bit en ModifierMask).
    // Cualquier tipo de componente es válido: botón, dpad, gatillo, analógico, touchpad, gyro.
    // Para fuentes analógicas (gatillo, analógico) se usa un threshold configurable para
    // determinar "activo". Un modificador puede además tener su propio VirtualTarget.
    std::vector<ComponentId> modifierSources;

    // Capa base (siempre presente)
    std::array<std::optional<PhysicalComponent>, kComponentCount> baseLayer;

    // Overrides por combinación de modificadores. Solo existen las entradas definidas.
    // Ejemplo con LP=bit0, RP=bit1:
    //   mask 0x01 → LP held
    //   mask 0x02 → RP held
    //   mask 0x03 → LP+RP held
    std::map<ModifierMask, std::array<std::optional<PhysicalComponent>, kComponentCount>> modifierLayers;

    void process(GamepadState& state) const;
};
```

#### process() — dos pasadas

```
Pasada 1 (solo modificadores):
  → evaluar cada modifierSource según su tipo (botón: pressed; analógico/gatillo: valor > threshold) → construir ModifierMask activa

Pasada 2 (todos los demás componentes):
  → para cada ComponentId:
       1. buscar en modifierLayers[maskActiva]  → si existe, usar ese
       2. si no, buscar en baseLayer            → si existe, usar ese
       3. si no, ignorar
```

#### JSON

```json
{
  "modifier_buttons": ["l5", "r5"],
  "layers": {
    "l5": {
      "buttons": { "a": { "type": "macro", "name": "RapidA" } }
    },
    "r5": {
      "buttons": {
        "x": { "virtual": "y" },
        "y": { "virtual": "x" }
      }
    },
    "l5+r5": {
      "buttons": { "a": { "type": "keyboard", "keys": ["ctrl", "z"] } }
    }
  }
}
```

#### Relación con el Component System (migración P1–P6)

`PhysicalController` debe diseñarse con `baseLayer` + `modifierLayers` desde **P1**. Añadirlo después obligaría a reabrir `ComponentTypes.h` y `process()`. El soporte de capas no requiere UI hasta más adelante — basta con que la estructura de datos y `process()` estén listos.
