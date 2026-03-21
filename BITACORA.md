# Bitácora — VirtualPad

> Registro histórico de sesiones de desarrollo.
> Cada entrada documenta qué se hizo, qué se aprendió y qué decisiones se tomaron.

---

## V1Pro3 — ~2026/03/03 — Base: WinMM → ViGEm

- Primer prototipo: `IInputSource`, `EightBitDoInputSource`, `ViGEmOutputAdapter`. Loop consola.

## V2Pro3 — ~2026/03/04 — Config dinámica por VID/PID

- `ConfigLoader` + JSON. Modos `dinput` / `xinput`. `nlohmann/json`.

## V3Pro3 — ~2026/03/10 — LightningBot (FFX Thunder Plains)

- Bot de esquive de rayos por detección de flash de pantalla. `tools/TriggerCount/` (calibración, no es producto).

## V4Pro3 — ~2026/03/15 — Sistema de macros (consola, pre-GUI)

- DSL de macros: secuencias, combos, holds, analógicos. `LuluMacro`. `tools/lulu_macro_tests.csv`.

## V5Pro3 — ~2026/03/15 — Refactor modular

- Fuentes en `input/`, `output/`, `config/`, `bots/`, `macros/`. Sin cambios funcionales.

## V6Pro3 — ~2026/03/15 — ImGui GUI + PadEngine threaded

- Dear ImGui (Win32 + D3D11). PadEngine en hilo de fondo 8ms. `configs/` → `data/`.

## V7Pro3 — ~2026/03/15 — PadScanner visual

- PadScanner enumera WinMM, lee valores raw. Tab Scanner en ImGui.

## V8 — ~2026/03/17 — data/ + virtualpad.json + prep dual-API

- `data/virtualpad.json`. Preparación interna para HID.

## V9 — ~2026/03/18 — HIDInputSource + HIDScanner

- HID raw input (overlapped). ValCaps para normalización. hid_brake/hid_accel analógicos. DeviceCandidate unifica WinMM+HID en PadEngine.

---

## V10 — ~2026/03/19 — Logging (spdlog) + HidHide integration (Fase B)

**Qué se hizo:**

### Logging con spdlog
- Librería `spdlog` añadida al proyecto (header-only).
- `Log.h`: dos sinks — consola coloreada + fichero rotativo (`logs/virtualpad.log`, 1MB × 3 ficheros).
- Nivel de log configurable en `data/virtualpad.json` → `"log_level": "debug"/"info"`.
- `Log::init()` se llama en `main()` antes de crear PadEngine.
- Niveles: debug (raw bytes, scan, ValCaps), info (eventos), warn (desconexiones), error (fallos driver).

### HidHideClient (Fase B)
- `HidHideClient` (`output/`): interfaz programática al driver kernel HidHide de Nefarius.
- Al arrancar: añade VirtualPad.exe a la whitelist (idempotente).
- Al tomar un mando: lo añade al blacklist + activa el filtro.
- Al soltar/cerrar: lo quita del blacklist + desactiva el filtro (solo si nosotros lo activamos).
- Si HidHide no instalado: `isAvailable()=false`, todo es no-op.
- **Resultado**: Steam solo ve el mando virtual Xbox 360. El físico queda oculto.

### Estado al cerrar
- Cadena completa verificada: Pro 3 D-mode BT → VirtualPad → ViGEm → Steam ✓
- Pro 3 X-mode ✓, Pro 2 X-mode ✓, Pro 2 D-mode ✓, F310 D-mode ✓ (en scanner).
- Fases A1–A4 y B completadas.
