# Contexto de sesión — VirtualPad

## Estado actual del proyecto (2026/03/21)
- Cadena funciona: **Pro 3 D/X, Pro 2 D/X, F310 D, DS4 → VirtualPad → ViGEm → Steam ✓**
- **HidHide integrado** ✓, **spdlog integrado** ✓
- **Sistema de perfiles de juego** ✓ — hot-swap en tiempo real, `data/FinalFantasyX.json` operativo
- **Fases A, B, C completadas ✓**
- **Fase C**: keyboard combos, mouse_click, mouse movement desde stick analógico ✓

## Snapshots
- `VirtualPadV1Pro3` — Primer prototipo funcional
- `VirtualPadV2Pro3` — Configuración dinámica por VID/PID
- `VirtualPadV3Pro3` — Bot FFX (LightningBot)
- `VirtualPadV4Pro3` — Pre-refactor (consola pura, macros)
- `VirtualPadV5Pro3` — Refactor de estructura
- `VirtualPadV6Pro3` — A1: ventana ImGui + PadEngine threaded
- `VirtualPadV7Pro3` — A2: PadScanner visual con tabs
- `VirtualPadV8` — A3 completa
- `VirtualPadV9` — A4: HIDInputSource, gatillos analógicos, async HID scan
- `VirtualPadV10` — Fase B: HidHide + spdlog
- `VirtualPadV11` — HIDInputSource fixes (normalizeHIDAxis, hat encoding)
- `VirtualPadV12` ✓ — A4.2 DS4 v2, A4.3 F310, A4.4 botones extra, perfiles hot-swap
- `VirtualPadV13` ✓ — Fase C: keyboard, mouse_click, mouse movement, dead zone

---

## ARQUITECTURA ACTUAL

### Estructura de archivos
```
VirtualPad/
├── VirtualPad.cpp       ← main: init logger + crea PadEngine + AppWindow
├── Log.h                ← spdlog init (consola + fichero rotativo logs/virtualpad.log)
├── PadEngine.h/.cpp     ← hilo de fondo: scan → config → ViGEm → loop 8ms
├── PadScanner.h/.cpp    ← utilidad estática: enumera WinMM + lee inputs raw
├── AppWindow.h/.cpp     ← hilo principal: Win32 + DX11 + ImGui (tabs: Engine / Scanner)
├── GamepadState.h       ← tipo compartido
├── input/
│   ├── IInputSource.h                        ← setConfig() virtual pura
│   ├── ControllerConfig.h
│   ├── EightBitDoInputSource.h/.cpp
│   ├── HIDInputSource.h/.cpp
│   └── HIDScanner.h/.cpp
├── output/
│   ├── ViGEmOutputAdapter.h/.cpp
│   └── HidHideClient.h/.cpp
├── config/
│   └── ConfigLoader.h/.cpp    ← GameProfile, loadGameProfile, applyProfile
├── macros/
│   ├── Macro.h/.cpp
│   └── MacroParser.h/.cpp
├── bots/
│   └── LightningBot.h/.cpp
├── data/
│   ├── controllers.json      ← config base pura (claves _ = sin equivalente Xbox)
│   ├── FinalFantasyX.json    ← perfil de juego: overrides Pro 3 y Pro 2 D-mode
│   ├── macros.json
│   └── virtualpad.json       ← VID/PID del pad virtual + log_level
├── consoleOutput/            ← output de pruebas/debug
├── examples/
├── spdlog/
├── imgui/
└── nlohmann/
```

---

## CONTROLLERS.JSON — configs actuales

| Config | VID | PID | API | Triggers | Estado |
|---|---|---|---|---|---|
| 8BitDo Pro 3 (D-mode) | 2DC8 | 6009 | WinMM | Botones 9/10 digitales | ✓ |
| 8BitDo Pro 3 (X-mode) | 2DC8 | 310B | WinMM | trigger_combined | ✓ |
| 8BitDo Pro 2 (D-mode) | 2DC8 | 6006 | HID | hid_brake/hid_accel analógicos | ✓ |
| 8BitDo Pro 2 (X-mode) | 045E | 02E0 | WinMM | trigger_combined | ✓ |
| Logitech F310 (D-mode) | 046D | C216 | WinMM | Botones 7/8 digitales | ✓ |
| Logitech F310 (X-mode) | 046D | C21D | WinMM | trigger_combined | pendiente prueba |
| Sony DualShock 4 v2 | 054C | 09CC | WinMM | dwUpos/dwVpos analógicos | ✓ |

---

## PLAN DE DESARROLLO

### Fase A — COMPLETADA ✓
### Fase B — COMPLETADA ✓

---

### Fase C — Salida teclado/ratón (modo escritorio)
> **Pendiente dentro de C**: extender el sistema de perfiles para que soporte overrides de ejes
> (igual que ya hace con botones). Caso de uso directo: perfil FFX mapea stick derecho a
> `mouse_x`/`mouse_y` para apartar el cursor de pantalla sin afectar al perfil base.
> Afecta: `GameProfile::Override` (añadir `axes`), `ConfigLoader` (parsear axes en overrides),
> `applyProfile` (aplicar axis overrides encima de la base).


Cierra la descripción original de C ("teclado+ratón como salida alternativa").
Todo via `SendInput()`. La dirección contraria (teclado → mando virtual) queda fuera de scope.

#### Acciones de botón (eventos discretos)
- `{"type": "keyboard", "keys": ["alt", "tab"]}` — combo de teclado al pulsar
- `{"type": "mouse_click", "button": "left"|"right"|"middle"}` — click de ratón

#### Movimiento de ratón (continuo, por tick de loop)
- Target de eje especial: `"mouse_x"` / `"mouse_y"` con parámetro `"speed"`
- Cada tick: si deflexión > dead zone → `SendInput(MOUSEEVENTF_MOVE, dx, dy)` relativo
- En reposo → sin movimiento. Dead zone reutiliza la de Fase E.
- Ejemplo: `"dwZpos": { "target": "mouse_x", "speed": 15 }`

#### Calibración de velocidad de ratón (UI)
- Slider en la tab Engine: `Mouse speed: [----o--------] 15`
- Rango: 1–50 (o similar), ajustable en tiempo real mientras se mueve el stick
- El valor se aplica en el siguiente tick — el usuario calibra moviendo el stick y ajustando hasta que la velocidad le resulte cómoda, igual que ajustar DPI en un ratón gaming
- Se guarda en el perfil activo (el mismo JSON de overrides) — cada perfil/juego puede tener su propia velocidad
- X e Y comparten el mismo slider (caso raro querer velocidades distintas por eje)

#### Casos de uso
- Botones sin equivalente Xbox (Home, Lp, Rp, L4, R4) → acciones de sistema reales
- Perfil "Desktop": stick derecho mueve cursor, botones = teclado/clicks → control completo del PC sin ratón físico
- Remote play: navegar escritorio desde el sofá con cualquier mando

#### Archivos afectados
- `ControllerConfig.h` — nuevos `ButtonActionType` + target de eje `mouse_x/y`
- `EightBitDoInputSource`/`HIDInputSource` — dispatch de nuevas acciones
- `PadEngine` — movimiento de ratón en el tick loop (continuo, no por evento)
- Esfuerzo estimado: bajo-medio (una o dos sesiones)

#### Pendiente — freeze output al Alt+Tab
- Flag opcional en acción keyboard: `"freeze_output": true`
- Al disparar: activa `m_frozen` en PadEngine → `output.update(neutral_state)` en el tick
- Descongela: al primer input del mando (cualquier botón o stick fuera de dead zone)
- Evita que el juego reciba inputs del mando mientras está en segundo plano

#### Pendiente — sintaxis de teclado
- Actual: `{"type":"keyboard","keys":["alt","tab"]}` — funciona pero inconsistente
- Propuesta: `{"type":"keyboard","keys":"alt+tab"}` — cadena con `+` como separador, igual que el DSL de macros
- Afecta solo a `ConfigLoader::parseButtonAction` (split por `+`) y a la documentación del README
- Nota de inversión de eje: `mouse_y` siempre lleva `invert` contrario al `right_y` del mismo eje físico

### Fase D — UI visual
- **Panel mando físico**: dibujito del mando activo con botones iluminándose al pulsar (ImDrawList)
- **Panel mando virtual**: segundo dibujito Xbox mostrando qué recibe el juego en tiempo real
- **Editor de mappings**: clic en botón físico → asignar acción → guarda a JSON sin editar texto
- **Editor de macros**: integrado en la misma UI
- Esfuerzo: alto — es la pieza que transforma VirtualPad en herramienta para cualquier usuario

### Fase E — Mejoras operacionales
- **Drift calibration**: dead zone por eje con rescalado, configurable por perfil. Sliders en UI.
  `|v| <= dz → 0`, `|v| > dz → sign(v)*(|v|-dz)/(1-dz)`
  Afecta: `ControllerConfig.h`, ambos InputSource, `ConfigLoader`, `AppWindow`
- **Auto-reconexión / Hot-plug**: PadEngine vigila VID/PID tras desconexión y reconecta solo.
  DS4: auto-detectar USB vs BT. Afecta: PadEngine (WM_DEVICECHANGE), ambos InputSource.

### Fase F — Steam Controller
- Nueva clase `SteamControllerInputSource` (protocolo propietario)
- VID:28DE PID:1142 (dongle inalámbrico) / PID:1102 (USB directo)
- Init: feature report 0x87 → desactiva lizard mode, habilita gamepad mode
- Close: restaura lizard mode
- Toggle lizard↔gamepad en caliente: `{"type": "lizard_toggle"}` asignable a botón Steam
  - Lizard → mando emite ratón+teclado HID nativo; ViGEm pad a neutro
  - Gamepad → VirtualPad lee y reenvía; útil para remote play (navegar ↔ jugar)
- Datos disponibles: trackpads L/R como ejes, stick, gatillos analógicos, grips L4/R4
- **Limitación documentada**: solo funciona con Steam cerrado

---

### Pendiente transversal — Git / GitHub (PRÓXIMA SESIÓN)
Plan acordado: crear repo con historia completa a partir de los snapshots existentes.

**Orden de trabajo:**
1. `git init` en `G:\C++\VirtualPad\` (raíz, no en la subcarpeta del proyecto)
2. Crear `.gitignore`: excluir `x64/`, `logs/`, `*.user`, carpetas de snapshots (`VirtualPadV*/`)
3. Para cada snapshot en orden cronológico: copiar contenido → `git add` → `git commit` → `git tag`
4. Commit final con el estado actual (`VirtualPad/`)
5. Crear repo en GitHub y hacer push con `--tags`
6. Tras verificar que todo subió: borrar carpetas de snapshots locales

**Snapshots y tags sugeridos** (las fechas son orientativas, no importa que no encajen):
| Carpeta | Tag | Mensaje |
|---|---|---|
| VirtualPadV1Pro3 | v0.1 | Initial prototype: WinMM read → ViGEm forward |
| VirtualPadV2Pro3 | v0.2 | Dynamic config by VID/PID |
| VirtualPadV3Pro3 | v0.3 | LightningBot (FFX thunder dodge) |
| VirtualPadV4Pro3 | v0.4 | Macro engine (console) |
| VirtualPadV5Pro3 | v0.5 | Refactor: modular directories |
| VirtualPadV6Pro3 | v0.6 | ImGui GUI + PadEngine threaded |
| VirtualPadV7Pro3 | v0.7 | PadScanner visual |
| virtualPadv8     | v0.8 | data/ directory + dual-API prep |
| VirtualPadV9     | v0.9 | HIDInputSource + HIDScanner |
| VirtualPadV10    | v0.10 | spdlog + HidHide (Fase B) |
| VirtualPadV11    | v0.11 | HIDInputSource fixes (unsigned axes, hat) |
| VirtualPadV12    | v0.12 | DS4, F310, botones extra, perfiles hot-swap |
| VirtualPadV13    | v0.13 | Fase C: keyboard/mouse output |
| VirtualPad (actual) | v0.14 / main | Estado actual |

**Notas:**
- Las fechas de los commits pueden ser las reales de hoy — no importa, es nuestro repo
- README, BITACORA, SESSION_CONTEXT, MACROS.md van en el repo (raíz)
- El archivo `.slnx` también va (abre el proyecto en VS)

### Backlog — cuando todo lo demás esté hecho
- **Joy-Con L/R** — BT, protocolo Nintendo propietario, cada Joy-Con dispositivo independiente
- **Wii U Pro Controller** — BT, protocolo Nintendo (batería agotada, pendiente hardware)
- **Classic Controller (via GBros 8BitDo)** — USB HID, VID/PID a descubrir con scanner
- **GameCube controller (via GBros 8BitDo)** — igual que Classic Controller
- **Wiimote** — BT, protocolo Nintendo + extensiones (Nunchuck, Classic)
- **Feedback (vibración + LEDs)** — ViGEm notification callback → HID output reports
- **DS4 avanzado** — touchpad XY, giroscopio, acelerómetro, control LEDs, rumble (HID raw)
- **DInput/vJoy output** — para juegos que no aceptan XInput (resto de la Fase C original)

---

## Problemas conocidos / pendientes
- **trigger_combined limitación**: L2+R2 simultáneos imposibles en X-mode — aceptado
- **pwsh.exe warning** en build: post-build step — inofensivo
- **Scanner HID**: solo muestra estado si el engine tiene ese mando activo
- **F310 D-mode**: ¿tiene hid_brake/hid_accel? sin verificar (usa WinMM en su lugar)
- **Botón A trace**: spdlog::info en PadEngine.cpp — TODO quitarlo
- **F310 X-mode**: pendiente verificar in-game
- **Pro 2 D-mode**: Lp/Rp en botones 3/6 verificados ✓ (igual que Pro 3) — no documentados en controllers.json con prefijo `_`

---

> **NOTA PARA CLAUDE**: Este fichero es contexto vivo — solo información útil para continuar trabajando.
> - **No incluir histórico**: todo lo que ya se hizo va a `BITACORA.md`, no aquí.
> - **Al final de cada sesión**: reescribir con el estado real — qué funciona, qué queda pendiente, en qué estado exacto está el proyecto. Usar fechas YYYY/MM/DD.
> - **BITACORA**: añadir siempre al final (las entradas más recientes quedan abajo). Mismo día → numeración (1), (2)… en orden cronológico dentro del día.
