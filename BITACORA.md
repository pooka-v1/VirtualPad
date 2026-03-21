# Bitácora — VirtualPad

> Registro histórico de sesiones de desarrollo.
> Cada entrada documenta qué se hizo, qué se aprendió y qué decisiones se tomaron.

---

## V1Pro3 — ~2026/03/03 — Base: lectura WinMM → virtualización ViGEm

**Qué se hizo:**
- Primer prototipo funcional: leer un mando físico y reemitirlo como Xbox 360 virtual.
- Implementada la interfaz abstracta `IInputSource` (strategy pattern).
- `EightBitDoInputSource`: lee joystick físico vía WinMM (`joyGetPosEx`), normaliza ejes y botones a `GamepadState`.
- `GamepadState`: struct compartida que representa el estado normalizado del mando (sticks, triggers, botones, dpad).
- `ViGEmOutputAdapter`: crea un pad virtual Xbox 360 via ViGEmBus y envía el estado cada tick.
- Loop principal en consola: scan manual de puertos WinMM → lectura → forwarding → salida.

**Hardware objetivo:** 8BitDo Pro 2/Pro 3 en modo D (DInput).
