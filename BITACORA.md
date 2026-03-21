# Bitácora — VirtualPad

> Registro histórico de sesiones de desarrollo.
> Cada entrada documenta qué se hizo, qué se aprendió y qué decisiones se tomaron.

---

> **Nota sobre fechas pre-2026/03/19**: El proyecto no usa git, los snapshots no tienen metadatos de fecha.
> Las entradas históricas están ordenadas por versión pero sin fecha exacta.
> El contenido se ha reconstruido leyendo el código de cada snapshot.

---

## Sesiones previas — Reconstrucción histórica (V1 → V10)

### V1Pro3 — ~2026/03/03 — Base: lectura WinMM → virtualización ViGEm

**Qué se hizo:**
- Primer prototipo funcional de la idea central: leer un mando físico y reemitirlo como Xbox 360 virtual.
- Implementada la interfaz abstracta `IInputSource` (strategy pattern).
- `EightBitDoInputSource`: lee joystick físico vía WinMM (`joyGetPosEx`), normaliza ejes y botones a `GamepadState`.
- `GamepadState`: struct compartida que representa el estado normalizado del mando (sticks, triggers, botones, dpad).
- `ViGEmOutputAdapter`: crea un pad virtual Xbox 360 via ViGEmBus y envía el estado cada tick.
- Loop principal en consola: scan manual de puertos WinMM → lectura → forwarding → salida.

**Hardware objetivo:** 8BitDo Pro 2/Pro 3 en modo D (DInput).

---

### V2Pro3 — ~2026/03/04 — Configuración dinámica por VID/PID

**Qué se hizo:**
- `ConfigLoader`: carga mappings desde JSON en lugar de estar hardcodeados.
- `ControllerConfig`: estructura de mapeos (ejes, triggers, botones) indexada por VID/PID del mando.
- El scan de WinMM ya captura `wMid`/`wPid` para identificar qué mando está conectado.
- Soporte para dos modos de dispositivo (`dinput` / `xinput`) en el JSON.
- Introducción de `AxisMapping` y `TriggerMapping` como tipos de configuración.

---

### V3Pro3 — ~2026/03/10 — Bot visual: LightningBot (FFX Thunder Plains)

**Qué se hizo:**
- `LightningBot`: bot especializado para esquivar rayos en Final Fantasy X (Thunder Plains).
- Detecta flash lavanda en pantalla (brightness > umbral) monitorizando una región de la pantalla.
- Thread dedicado: detect → wait → press (pulsa un botón del mando virtual al detectar el flash).
- Contador de esquivas (`dodgeCount`).
- La acción bot se dispara desde `ButtonActionType::Bot` en la config: un botón físico activa/desactiva el bot.

---

### V4Pro3 — ~2026/03/15 — Sistema de macros (consola pura, pre-GUI)

**Qué se hizo:**
- Motor de macros completo integrado en el loop de consola.
- `MacroParser`: parsea un DSL compacto de texto a pasos compilados. Sintaxis: `"CU, CUR + X"`, `"B=1000"`, `"(A,B,C)*5000"`.
- `Macro` + `CompiledStep`: cada paso tiene `startMs`, `holdMs`, `endMs`. Se ejecutan contra el `GamepadState` cada tick.
- `MacroEffect`: qué botones y sticks afecta cada paso (`hasLeftStick`, `hasRightStick`).
- `MacroRepeatMode`: Once, TimedMs, UntilRelease, Toggle.
- `LuluMacro`: macro especializada, rotación continua del stick derecho (~4 RPM) para el hechizo Lulu en FFX.
- Los macros se asignan a botones del mando físico en la config JSON.

**Estado:** aplicación de consola pura, sin GUI. Esta versión es la referenciada en SESSION_CONTEXT como "versión pre-refactor (consola pura, macros)".

---

### V5Pro3 — ~2026/03/15 — Refactor: modularización en directorios

**Qué se hizo:**
- Reorganización de todos los archivos fuente en subdirectorios:
  - `input/` — IInputSource, EightBitDoInputSource, ControllerConfig
  - `output/` — ViGEmOutputAdapter
  - `config/` — ConfigLoader
  - `bots/` — LightningBot
  - `macros/` — Macro, MacroParser, LuluMacro
- En root solo quedan: `VirtualPad.cpp`, `GamepadState.h`, `.vcxproj`.
- Los includes se actualizan a rutas relativas (`"input/EightBitDoInputSource.h"`, etc.).
- No se añade funcionalidad nueva: es un refactor puro de estructura.

---

### V6Pro3 — ~2026/03/15 — GUI con ImGui + threading (PadEngine / AppWindow)

**Qué se hizo:**
- Introducción de **Dear ImGui** (v1.92 WIP) con backend Win32 + Direct3D 11.
- `PadEngine`: el pipeline de lectura/macro/ViGEm se mueve a un hilo de fondo (8ms tick). Expone accessors thread-safe: `isRunning()`, `isConnected()`, `getDevice()`, `getStatus()`.
- `AppWindow`: hilo principal maneja ventana Win32, D3D11 (device, context, swapchain, render target), contexto ImGui y WndProc estático.
- `VirtualPad.cpp` queda en tres líneas: `Log::init()` → `PadEngine engine` → `AppWindow window(engine)` → `window.run()`.
- Primera versión con interfaz gráfica visible. Tabs: Engine / Scanner (aún básico).

---

### V7Pro3 — ~2026/03/15 — PadScanner: enumeración visual de mandos WinMM

**Qué se hizo:**
- `PadScanner`: utilidad estática para enumerar todos los puertos WinMM conectados.
- `DeviceInfo`: struct con `port`, `axes`, `buttons`, `vid`, `pid`, `name`.
- `RawInput`: struct con todos los ejes raw (`xpos`, `ypos`, `zpos`, `rpos`, `upos`, `vpos`, `pov`, `buttons`).
- Métodos: `PadScanner::scan()` y `PadScanner::readRaw(port)`.
- La tab Scanner de ImGui muestra la lista de mandos detectados y sus valores raw en tiempo real.
- Separación clara: PadScanner lee lo que hay en WinMM; PadEngine gestiona el mando activo.

---

### V8 — ~2026/03/17 — Consolidación + directorio data/ + preparación dual-API

**Qué se hizo:**
- Creación del directorio `data/` para los ficheros de configuración en runtime (`controllers.json`, `macros.json`, `virtualpad.json`).
- Los ficheros JSON se separan del directorio fuente.
- Preparación arquitectónica para soportar dos APIs de entrada (WinMM + HID) sin cambios de interfaz pública.
- Estructura de archivos root idéntica a V7. Los cambios son principalmente en organización de datos y preparación interna.

---

### V9 — ~2026/03/18 — Soporte HID: HIDInputSource + HIDScanner + refactor PadEngine

**Qué se hizo:**
- `HIDScanner` (`input/`): enumera dispositivos HID filtrando por usage page 0x01 (Generic Desktop), usage 0x04 (Joystick) y 0x05 (Gamepad). Devuelve `DeviceInfo` con VID, PID, usage page/usage, device path y product name.
- `HIDInputSource` (`input/`): lectura de mandos via HID API (ReadFile overlapped). Maneja:
  - HID preparsed data y ValCaps para normalización automática de ejes.
  - Usages mapeados: hid_x, hid_y, hid_z, hid_rx, hid_ry, hid_rz, hid_brake (0xC4), hid_accel (0xC5).
  - D-pad HAT switch parsing.
  - Fix del Report ID mismatch del Pro 2 D-mode BT: si `HidP` falla con `INCOMPATIBLE_REPORT_ID`, swap temporal de `buf[0]` y retry.
- **Refactor PadEngine**: nuevo `struct DeviceCandidate` que unifica WinMM y HID. `selectDevice(int index)` reemplaza `selectDevice(UINT port)`. `getLastState()` thread-safe. `getCandidates()` reemplaza la lista WinMM pura.
- La tab Scanner de ImGui muestra dos secciones: dispositivos WinMM y dispositivos HID.
- **Pro 2 D-mode**: funciona vía HID con gatillos analógicos reales (hid_brake→triggerR, hid_accel→triggerL).
- **F310 D-mode**: visible en scanner HID.

---

### V10 — ~2026/03/19 — Logging (spdlog) + HidHide integration (Fase B)

**Qué se hizo:**

#### Logging con spdlog
- Librería `spdlog` añadida al proyecto (header-only).
- `Log.h`: wrapper de inicialización con dos sinks: consola coloreada + fichero rotativo (`logs/virtualpad.log`, 1MB × 3 ficheros).
- Nivel de log configurable en `data/virtualpad.json` → `"log_level": "debug"/"info"`.
- `Log::init()` se llama en `main()` antes de crear PadEngine.
- Niveles usados: debug (raw bytes, scan, ValCaps), info (eventos normales), warn (desconexiones, HidHide no instalado), error (fallos driver).

#### HidHideClient (Fase B)
- `HidHideClient` (`output/`): interfaz programática al driver kernel HidHide de Nefarius.
- IOCTLs: device type 32769, funciones 2048–2053, access FILE_READ_DATA. Fuente: `HidHide-master/HidHideCLI/src/FilterDriverProxy.cpp`.
- Control device: `\\.\HidHide`, handle con `GENERIC_READ | FILE_SHARE_READ|WRITE|DELETE`.
- Comportamiento:
  - Al arrancar: añade VirtualPad.exe a la whitelist (idempotente).
  - Al tomar un mando: lo añade al blacklist + activa el filtro (guarda si fuimos nosotros quienes lo activamos).
  - Al soltar/cerrar: lo quita del blacklist + desactiva el filtro solo si nosotros lo habíamos activado.
  - Destructor como red de seguridad para crashes.
  - Si HidHide no instalado: `isAvailable()=false`, todo es no-op.
- **Resultado**: Steam solo ve el mando virtual Xbox 360. El mando físico queda oculto mientras VirtualPad está activo.

#### Estado al cerrar esta fase
- Cadena completa verificada: Pro 3 D-mode BT → VirtualPad → ViGEm → Steam ✓
- Pro 3 X-mode ✓, Pro 2 X-mode ✓, Pro 2 D-mode ✓, F310 D-mode ✓ (en scanner)
- Fase A1–A4 completadas. Fase B completada.

---

## Sesión 2026/03/19 — Fase A4: HID X-mode (prototipo) + fixes HIDInputSource

### Contexto
Continuación tras completar Fase B (HidHide). Estado: snapshots V4–V10 existentes, V10 = Fase B completa.

### Decisiones de arquitectura
- **WinMM queda como legacy** para D-mode. No se elimina.
- **XInput descartado**: no aporta nada que HID no dé, y requiere correlación VID/PID externa.
- **HID para todo**: Pro 3 D/X, Pro 2 D/X, F310 D, DS4 — cualquier mando con algo especial.
- **X-mode vía HID = prototipo**: triggers separados funcionan, pero solo expone 10 botones estándar Xbox (sin Home, L4, R4, Lp, Rp). Firmware del 8BitDo limita X-mode a Xbox 360 estándar. Documentado en controllers.json como `_hid_prototype`. Se mantiene WinMM para X-mode hasta tener botones correlativos completos.
- **D-mode** es el camino para botones extra (Home, L4, R4, Lp, Rp) — los expone todos vía HID.

### Bugs encontrados y corregidos

#### `normalizeHIDAxis` — ejes unsigned `[0, -1]`
- **Síntoma**: sticks y triggers devolvían 0.0f siempre en modo HID.
- **Causa**: el descriptor HID del Pro 3 X-mode reporta `logMax = -1` (LONG) para ejes unsigned, lo que hace `range = logMax - logMin = -1`, y el código tenía `if (range <= 0) return 0.0f`.
- **Fix**: añadido `bitSize` a `ValueRange`. Cuando `logMax < logMin` (unsigned), se usa `uMax = (1 << bitSize) - 1` como denominador.
- **Afecta a**: todos los mandos HID con descriptor unsigned — Pro 2 D-mode y F310 estaban también rotos (sticks siempre 0), aunque coincidía con el valor de centro y no se notaba hasta mover el stick.

#### Hat switch — encoding `[1,8]` vs `[0,8]`
- **Síntoma**: diagonal NW (arriba+izquierda) no funcionaba en Pro 3 X-mode.
- **Causa**: Pro 3 X-mode usa hat `[1,8]` donde **8=NW** y center=0 (fuera de rango). El código anterior trataba `hatValue >= logMax` como center, capturando NW como neutral.
- **Fix**: cambio a detección por rango: si `hatValue < logMin || hatValue > logMax` → neutral. Si dentro del rango → `parseHIDDpad(hatValue - logMin)`.
- **Backward compatible**: Pro 2 D-mode usa `[0,8]` donde 8=center y se maneja correctamente por `parseHIDDpad` internamente.

### Hallazgos técnicos — Pro 3 X-mode HID

**Report format (15 bytes, ReportID=0):**
```
[0]      Report ID = 0x00
[1-2]    Left stick X  (uint16 LE, unsigned, center ~0x8000)
[3-4]    Left stick Y  (uint16 LE, unsigned, center ~0x7FFF)
[5-6]    Right stick X (uint16 LE, unsigned, center ~0x8000)
[7-8]    Right stick Y (uint16 LE, unsigned, center ~0x7FFF)
[9-10]   Trigger Z combinado (uint16 LE, center=0x8000, LT→0, RT→0xFFFF)
[11-12]  Botones (bitfield, HID button usages 1-10)
[13]     Hat switch (0=center, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW)
[14]     Padding / no usado
```

**ValCaps (todos ReportID=0, todos unsigned [0,-1] excepto hat):**
- 0x30 (X), 0x31 (Y), 0x33 (Rx), 0x34 (Ry), 0x32 (Z) — range [0,-1], bitSize=16
- 0x39 (Hat) — range [1,8]

**Botones (10 estándar Xbox, correlativos 1-10):**
| HID | Virtual | Físico (8BitDo) |
|-----|---------|-----------------|
| 1 | a | A (pos. sur) |
| 2 | b | B (pos. este) |
| 3 | x | X (pos. oeste) |
| 4 | y | Y (pos. norte) |
| 5 | l1 | LB |
| 6 | r1 | RB |
| 7 | select | Select/Back |
| 8 | start | Start |
| 9 | l3 | L3 |
| 10 | r3 | R3 |

**Botones NO accesibles en X-mode**: Home, L4, R4, Lp, Rp — no están en el descriptor HID. Solo accesibles en D-mode.

**HID Z trigger combinado**: `invert: true` necesario (LT empuja hacia 0 = negativo sin invertir = triggerR incorrecto).

### Estado al cerrar sesión
- Pro 3 X-mode: WinMM (sin cambios funcionales respecto a V10)
- Pro 2 X-mode: WinMM (sin cambios)
- Pro 2 D-mode: HID — ahora con normalización de ejes correcta (fix unsigned)
- F310 D-mode: HID — ídem
- Snapshot V11 ✓

---

## Sesión 2026/03/20 (1) — Sistema de perfiles de juego

- Limpiado `controllers.json`: eliminados todos los macros y bots. Queda como config base pura.
- Documentados los botones sin equivalente Xbox (Rp, Lp, L4, R4) con claves `_` dentro del objeto `buttons` en lugar de un campo `_button_map` separado.
- Verificado mapa completo de botones del Pro 3 D-mode: L4=17, R4=18, Lp=6, Rp=3.
- Creado `data/FinalFantasyX.json` con overrides para Pro 3 y Pro 2 D-mode.
- Implementado sistema de perfiles en `ConfigLoader`: `GameProfile`, `loadGameProfile`, `applyProfile`. Las claves `_` en buttons ahora se ignoran en el parser.
- Implementado selector de perfil en la tab Engine de `AppWindow`: descubre automáticamente los JSONs de perfiles en `data/`.
- Implementado hot-swap de perfil en `PadEngine`: detección de cambio en el loop principal, `effectiveCfg` se recalcula, `input->setConfig()` actualiza el IInputSource sin reabrir el device, macros se re-inicializan.
- Añadido `setConfig()` a `IInputSource` (virtual pura), implementado en `EightBitDoInputSource` y `HIDInputSource`.
- README actualizado: sección de perfiles de juego con formato, ejemplos y explicación de los botones no estándar.
- Snapshot V12 pendiente.
- Verificado 2026/03/20: Pro 2 D-mode responde correctamente al perfil FinalFantasyX.json (botones 3, 6, 15 coinciden con Pro 3).

### Pro 3 D-mode — mapa de botones WinMM (verificado 2026/03/20)

| WinMM | Físico | Virtual Xbox | Notas |
|---|---|---|---|
| 1 | B | b | |
| 2 | A | a | |
| **3** | **Rp (paddle der.)** | **—** | **sin equivalente Xbox** |
| 4 | Y | y | |
| 5 | X | x | |
| **6** | **Lp (paddle izq.)** | **—** | **sin equivalente Xbox** |
| 7 | LB | l1 | |
| 8 | RB | r1 | |
| 9 | L2 | trigger l2 | digital (botón, no analógico) |
| 10 | R2 | trigger r2 | digital (botón, no analógico) |
| 11 | Select | select | |
| 12 | Start | start | |
| 13 | Home | home | |
| 14 | L3 | l3 | |
| 15 | R3 | r3 | |
| **17** | **L4** | **—** | **sin equivalente Xbox** |
| **18** | **R4** | **—** | **sin equivalente Xbox** |

---

## Sesión 2026/03/20 (2) — A4.3 F310 D-mode + A4.2 DS4 v2 + fixes + nuevas fases planificadas

### Fix: traza periódica HIDInputSource
- `m_readCount` solo avanzaba cuando llegaban reportes HID con datos nuevos. Mandos que solo envían en cambio de estado (F310, DS4 en reposo) nunca llegaban a 240 → el dump de debug nunca salía.
- Fix: añadido `++m_readCount` en el path de timeout (20ms sin datos) con su propio dump `(no report)`. Ahora la traza sale cada ~2s independientemente de actividad.

### A4.3 — Logitech F310 D-mode ✓
- Verificado via scanner: gatillos **digitales** (botones 7/8, salto 0→1). El "analógico progresivo" observado inicialmente era del test en X-mode por error.
- Layout de ejes real: dwXpos/dwYpos (left stick), dwZpos/dwRpos (right stick). El F310 D-mode usa Z/Rz para el stick derecho, no Rx/Ry como los 8BitDo.
- Configuración final: mode `dinput` (WinMM), igual que Pro 3 D-mode. Ejes Y invertidos (hid_y, hid_ry → false en HID era incorrecto; en WinMM dwYpos/dwRpos `invert: true`).
- Descartado modo HID para F310: botones extra innecesarios, layout más sucio.

### A4.2 — Sony DualShock 4 v2 ✓ (USB + BT)
- VID:054C PID:09CC. Funciona vía WinMM dinput sin configuración especial.
- BT: mismo VID/PID que USB, Windows lo abstrae igual → config única cubre ambos modos.
- Gatillos analógicos en dwUpos (L2) y dwVpos (R2) — independientes y progresivos.
- 13 botones activos: x/a/b/y, l1/r1, select/start, l3/r3, home (PS), touchpad click (_14).
- Botones 7/8 (digital L2/R2) sin mapear — los ejes analógicos los reemplazan.
- Features DS4 avanzadas pendientes para fases futuras: touchpad posición XY, giroscopio, acelerómetro, control de LEDs, rumble. Requieren HID raw parser con report 0x01 (USB) / 0x11 (BT).

### A4.4 — Botones extra Pro 3/Pro 2 D-mode ✓ (cerrado)
- Home mapeado a botón virtual. Lp/Rp/L4/R4 sin equivalente Xbox → solo disponibles como macro/bot en perfiles de juego. No se puede ir más allá con ViGEm Xbox 360.

### Nuevas fases planificadas
- **A7 — Auto-reconexión/Hot-plug**: PadEngine vigila VID/PID tras desconexión y reconecta solo. Especialmente útil para DS4 (batería corta). Para DS4: auto-detectar USB vs BT (report 0x01 vs 0x11).
- **A6 — Drift calibration**: dead zone por eje con rescalado, configurable por perfil de juego. `|v| <= dz → 0`, `|v| > dz → sign(v)*(|v|-dz)/(1-dz)`. Slider en UI.

### Snapshot V12 ✓
- Cubre: perfiles de juego hot-swap (sesión 1) + A4.2 DS4 + A4.3 F310 + A4.4 botones extra.

### Decisiones de arquitectura
- **WinMM queda como legacy** para D-mode. No se elimina.
- **XInput descartado**: no aporta nada que HID no dé, y requiere correlación VID/PID externa.
- **HID para todo**: Pro 3 D/X, Pro 2 D/X, F310 D, DS4 — cualquier mando con algo especial.
- **X-mode vía HID = prototipo**: triggers separados funcionan, pero solo expone 10 botones estándar Xbox (sin Home, L4, R4, Lp, Rp). Firmware del 8BitDo limita X-mode a Xbox 360 estándar. Documentado en controllers.json como `_hid_prototype`. Se mantiene WinMM para X-mode hasta tener botones correlativos completos.
- **D-mode** es el camino para botones extra (Home, L4, R4, Lp, Rp) — los expone todos vía HID.

### Bugs encontrados y corregidos

#### `normalizeHIDAxis` — ejes unsigned `[0, -1]`
- **Síntoma**: sticks y triggers devolvían 0.0f siempre en modo HID.
- **Causa**: el descriptor HID del Pro 3 X-mode reporta `logMax = -1` (LONG) para ejes unsigned, lo que hace `range = logMax - logMin = -1`, y el código tenía `if (range <= 0) return 0.0f`.
- **Fix**: añadido `bitSize` a `ValueRange`. Cuando `logMax < logMin` (unsigned), se usa `uMax = (1 << bitSize) - 1` como denominador.
- **Afecta a**: todos los mandos HID con descriptor unsigned — Pro 2 D-mode y F310 estaban también rotos (sticks siempre 0), aunque coincidía con el valor de centro y no se notaba hasta mover el stick.

#### Hat switch — encoding `[1,8]` vs `[0,8]`
- **Síntoma**: diagonal NW (arriba+izquierda) no funcionaba en Pro 3 X-mode.
- **Causa**: Pro 3 X-mode usa hat `[1,8]` donde **8=NW** y center=0 (fuera de rango). El código anterior trataba `hatValue >= logMax` como center, capturando NW como neutral.
- **Fix**: cambio a detección por rango: si `hatValue < logMin || hatValue > logMax` → neutral. Si dentro del rango → `parseHIDDpad(hatValue - logMin)`.
- **Backward compatible**: Pro 2 D-mode usa `[0,8]` donde 8=center y se maneja correctamente por `parseHIDDpad` internamente.

### Hallazgos técnicos — Pro 3 X-mode HID

**Report format (15 bytes, ReportID=0):**
```
[0]      Report ID = 0x00
[1-2]    Left stick X  (uint16 LE, unsigned, center ~0x8000)
[3-4]    Left stick Y  (uint16 LE, unsigned, center ~0x7FFF)
[5-6]    Right stick X (uint16 LE, unsigned, center ~0x8000)
[7-8]    Right stick Y (uint16 LE, unsigned, center ~0x7FFF)
[9-10]   Trigger Z combinado (uint16 LE, center=0x8000, LT→0, RT→0xFFFF)
[11-12]  Botones (bitfield, HID button usages 1-10)
[13]     Hat switch (0=center, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW)
[14]     Padding / no usado
```

**ValCaps (todos ReportID=0, todos unsigned [0,-1] excepto hat):**
- 0x30 (X), 0x31 (Y), 0x33 (Rx), 0x34 (Ry), 0x32 (Z) — range [0,-1], bitSize=16
- 0x39 (Hat) — range [1,8]

**Botones (10 estándar Xbox, correlativos 1-10):**
| HID | Virtual | Físico (8BitDo) |
|-----|---------|-----------------|
| 1 | a | A (pos. sur) |
| 2 | b | B (pos. este) |
| 3 | x | X (pos. oeste) |
| 4 | y | Y (pos. norte) |
| 5 | l1 | LB |
| 6 | r1 | RB |
| 7 | select | Select/Back |
| 8 | start | Start |
| 9 | l3 | L3 |
| 10 | r3 | R3 |

**Botones NO accesibles en X-mode**: Home, L4, R4, Lp, Rp — no están en el descriptor HID. Solo accesibles en D-mode.

**HID Z trigger combinado**: `invert: true` necesario (LT empuja hacia 0 = negativo sin invertir = triggerR incorrecto).

### Estado al cerrar sesión
- Pro 3 X-mode: WinMM (sin cambios funcionales respecto a V10)
- Pro 2 X-mode: WinMM (sin cambios)
- Pro 2 D-mode: HID — ahora con normalización de ejes correcta (fix unsigned)
- F310 D-mode: HID — ídem
- Snapshot pendiente: V11

---

## Sesión 2026/03/21 — Fase C: salida teclado/ratón ✓

### Reorganización de fases
- Fases A y B cerradas definitivamente.
- Plan renombrado: C (teclado/ratón), D (UI visual), E (drift + hot-plug), F (Steam Controller), Backlog.
- Fase C absorbe la descripción original de "salidas alternativas" (teclado+ratón como output).

### Hardware inventariado para backlog
- Steam Controller: VID:28DE PID:1142 (dongle) / PID:1102 (USB). Solo con Steam cerrado. Lizard mode toggle como acción de botón.
- Wii U Pro Controller (BT, batería agotada), Classic Controller y GameCube (via GBros 8BitDo), Wiimote.
- **Zero 2 colisión VID/PID**: BT D-mode usa VID:2DC8 PID:6006 igual que Pro 2. Si ambos conectados por BT simultáneamente, VirtualPad puede confundirlos. Solución: conectar uno por cable (PID diferente).

### Implementación Fase C

#### Nuevos tipos de acción en botones
- `ButtonActionType::Keyboard` — combo de teclado con `SendInput` al pulsar/soltar. Edge-triggered.
- `ButtonActionType::MouseClick` — click de ratón (left/right/middle). Edge-triggered.
- Ambos skippeados en InputSource (igual que Bot/Macro), dispatch en PadEngine.
- Helpers estáticos: `keyNameToVK`, `sendKeyCombo`, `sendMouseButton`.
- Press: teclas en orden → down. Release: en orden inverso → up. Correcto para modificadores (Alt, Ctrl, Shift).

#### Movimiento de ratón desde stick
- Target de eje nuevo: `mouse_x` / `mouse_y` — nuevos campos en `GamepadState`, poblados por ambos InputSource.
- PadEngine: acumulador sub-píxel (`mouseAccumX/Y`) → `SendInput(MOUSEEVENTF_MOVE)` cada tick.
- `speed` como parámetro de `AxisMapping` (default 15 px/tick a deflexión máxima).
- `setMouseSpeed`/`getMouseSpeed` expuestos en PadEngine — slider en UI pendiente para Fase D.

#### Fixes durante la sesión
- **Zona muerta ratón**: `kMouseDeadZone = 0.12f` — stick Pro 2 derivaba levemente en reposo. Constante hasta Fase E.
- **Inversión eje Y ratón**: `mouse_y` lleva `invert` contrario al `right_y` del mismo eje. Pantalla Y↓ positivo vs mando Y↑ positivo.
- **FinalFantasyX.json**: botón 13 (Home) → `{"type":"keyboard","keys":["alt","tab"]}` en Pro 2 D-mode.

### Verificado en sesión
- Pro 2 D-mode: stick derecho mueve cursor ✓, botón 13 → Alt+Tab ✓

### Pendientes dentro de Fase C
- **Axis overrides en perfiles**: añadir soporte de ejes en `GameProfile::Override` + `ConfigLoader` + `applyProfile`.
- **Sintaxis teclado**: cambiar array `["alt","tab"]` por cadena `"alt+tab"`. Solo afecta `ConfigLoader`.
- **freeze_output**: flag en acción keyboard → output neutral hasta primer input del mando.

### Snapshot V13 ✓
- Cubre: Fase C completa (keyboard, mouse_click, mouse movement, dead zone).
