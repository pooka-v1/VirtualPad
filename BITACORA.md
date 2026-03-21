# Bitácora — VirtualPad

> Registro histórico de sesiones de desarrollo.
> Cada entrada documenta qué se hizo, qué se aprendió y qué decisiones se tomaron.

---

## V1Pro3 — ~2026/03/03 — Base: lectura WinMM → virtualización ViGEm

- Primer prototipo funcional: WinMM → `GamepadState` → ViGEmBus.
- `IInputSource` (strategy pattern), `EightBitDoInputSource`, `ViGEmOutputAdapter`.
- Loop principal en consola. Hardware objetivo: 8BitDo Pro 2/Pro 3 D-mode.

---

## V2Pro3 — ~2026/03/04 — Configuración dinámica por VID/PID

- `ConfigLoader` + `ControllerConfig`: mappings desde JSON indexados por VID/PID.
- Soporte `dinput` / `xinput`. Librería `nlohmann/json` añadida.

---

## V3Pro3 — ~2026/03/10 — Bot visual: LightningBot (FFX Thunder Plains)

- `LightningBot`: detecta flash de pantalla en thread dedicado → pulsa botón virtual.
- `tools/TriggerCount/`: herramienta de calibración de timings (correlaciona flashes con pulsaciones manuales). No forma parte del producto.

---

## V4Pro3 — ~2026/03/15 — Sistema de macros (consola pura, pre-GUI)

- `MacroParser` + DSL compacto: secuencias, combos, holds, repeats, analógicos.
- `LuluMacro`: rotación stick derecho ~4 RPM para FFX (7 vueltas conseguidas).
- `tools/lulu_macro_tests.csv`: datos de calibración del timing.
- Aplicación de consola pura, pre-refactor.

---

## V5Pro3 — ~2026/03/15 — Refactor: modularización en directorios

- Fuentes reorganizados en `input/`, `output/`, `config/`, `bots/`, `macros/`.
- Refactor puro, sin cambios funcionales.

---

## V6Pro3 — ~2026/03/15 — GUI con ImGui + threading (PadEngine / AppWindow)

- Dear ImGui (Win32 + D3D11). `PadEngine` en hilo de fondo (8ms tick). `AppWindow` en hilo principal.
- `VirtualPad.cpp` queda en tres líneas. `configs/` → `data/`.

---

## V7Pro3 — ~2026/03/15 — PadScanner: enumeración visual de mandos WinMM

- `PadScanner`: enumera puertos WinMM y lee valores raw. Tab Scanner en ImGui.
- Separación: PadScanner lee disponibles, PadEngine gestiona el activo.

---

## V8 — ~2026/03/17 — Consolidación + data/ + virtualpad.json + preparación dual-API

**Qué se hizo:**
- Añadido `data/virtualpad.json`: VID/PID del mando virtual creado por ViGEm.
- Preparación arquitectónica interna para soportar dos APIs de entrada (WinMM + HID) sin cambios de interfaz pública.
- Estructura de archivos estable y definitiva para las versiones siguientes.
