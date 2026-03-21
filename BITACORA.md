# Bitácora — VirtualPad

> Registro histórico de sesiones de desarrollo.
> Cada entrada documenta qué se hizo, qué se aprendió y qué decisiones se tomaron.

---

## V1–V11 (resumen)

- **V1** — WinMM → ViGEm. Loop consola.
- **V2** — Config JSON por VID/PID.
- **V3** — LightningBot (FFX). tools/TriggerCount.
- **V4** — DSL de macros. LuluMacro. tools/lulu_macro_tests.csv.
- **V5** — Refactor modular.
- **V6** — Dear ImGui + PadEngine threaded. data/.
- **V7** — PadScanner visual.
- **V8** — virtualpad.json.
- **V9** — HIDInputSource + HIDScanner. DeviceCandidate.
- **V10** — spdlog + HidHide. Cadena completa verificada ✓.
- **V11** — Fix normalizeHIDAxis (unsigned axes). Fix hat switch [1,8] vs [0,8].

---

## Sesión 2026/03/20 (1) — V12: perfiles de juego + DS4 + F310 + botones extra

### Sistema de perfiles de juego
- Limpiado `controllers.json`: eliminados macros y bots. Config base pura.
- Botones sin equivalente Xbox (Rp, Lp, L4, R4) documentados con claves `_` en buttons.
- `data/FinalFantasyX.json`: overrides para Pro 3 y Pro 2 D-mode.
- `ConfigLoader`: `GameProfile`, `loadGameProfile`, `applyProfile`. Claves `_` ignoradas en el parser.
- Selector de perfil en tab Engine: descubre JSONs de perfiles en `data/` automáticamente.
- Hot-swap en `PadEngine`: detección de cambio en el loop, `effectiveCfg` se recalcula, `input->setConfig()` actualiza IInputSource sin reabrir el device.
- `setConfig()` añadido a `IInputSource` (virtual pura), implementado en `EightBitDoInputSource` y `HIDInputSource`.

### A4.3 — Logitech F310 D-mode ✓
- Gatillos **digitales** (botones 7/8). Ejes: dwXpos/dwYpos (left), dwZpos/dwRpos (right).
- Mode `dinput` (WinMM). Descartado modo HID.

### A4.2 — Sony DualShock 4 v2 ✓ (USB + BT)
- VID:054C PID:09CC. WinMM dinput. BT y USB comparten VID/PID → config única.
- Gatillos analógicos en dwUpos (L2) y dwVpos (R2). 13 botones activos.

### A4.4 — Botones extra Pro 3/Pro 2 D-mode ✓
- Home mapeado. Lp/Rp/L4/R4 sin equivalente Xbox → solo macro/bot en perfiles.

### Fix: traza periódica HIDInputSource
- `m_readCount` ahora avanza también en el path de timeout (mandos que solo envían en cambio de estado).
