# VirtualPad

Lee mandos físicos (WinMM, HID, XInput) y los reenvía como un mando Xbox 360 virtual via ViGEm.
Soporta macros, bots y configuración por JSON sin tocar el código.
Interfaz gráfica con Dear ImGui (Win32 + DirectX 11).

---

## Requisitos

### Para ejecutar

| Dependencia | Motivo |
|---|---|
| Windows 10/11 | API requeridas: WinMM, HID, DirectX 11 |
| [ViGEmBus driver](https://github.com/nefarius/ViGEmBus/releases) | Crea el mando Xbox 360 virtual |
| [HidHide driver](https://github.com/nefarius/HidHide/releases) | Oculta el mando físico a los juegos para evitar doble input |

> **ViGEmBus** y **HidHide** son del mismo autor (Nefarius). VirtualPad los controlará automáticamente.

### Para compilar

- Visual Studio 2022 (con soporte C++17 y Windows SDK)
- `nlohmann/json`, `imgui/`, `spdlog/` incluidos en el repositorio

---

## Añadir un mando nuevo — `data/controllers.json`

| mode | API | Cuándo usarlo |
|---|---|---|
| `"dinput"` | WinMM | Tab Scanner → WinMM |
| `"xinput"` | WinMM compat | Mando XInput en modo compatibilidad |
| `"hid"` | HID raw | Tab Scanner → HID-only |

### Ejes WinMM: `"dwXpos"`, `"dwYpos"`, `"dwZpos"`, `"dwRpos"`, `"dwUpos"`, `"dwVpos"`
### Ejes HID: `"hid_x/y/z/rz"`, `"hid_brake"` (0xC4), `"hid_accel"` (0xC5)

---

## Tipos de acción para botones

```json
"N": "a"
"N": { "type": "trigger", "target": "l2" }
"N": { "type": "macro",   "name": "NombreMacro" }
"N": { "type": "bot",     "name": "LightningBot" }
```

Ver [MACROS.md](MACROS.md) para la sintaxis completa de macros.

---

## Perfiles de juego

`controllers.json` es la config base pura. Los macros, bots y asignaciones especiales van en un JSON separado por juego.

```json
{
  "profile_name": "Final Fantasy X",
  "overrides": [
    {
      "vid": "2DC8", "pid": "6009",
      "buttons": {
        "3":  { "type": "macro", "name": "BanishingBlade" },
        "6":  { "type": "bot",   "name": "LightningBot" },
        "15": { "type": "macro", "name": "LuluOverdrive" }
      }
    }
  ]
}
```

Solo se declaran los botones que cambian. El perfil se selecciona desde la UI o `virtualpad.json`. Hot-swap en tiempo real.

---

## Mando de botones — mandos soportados

### 8BitDo Pro 3 — D-mode (VID:2DC8 PID:6009)

| WinMM | Físico | Virtual |
|---|---|---|
| 1 | B | b | 2 | A | a | **3** | **Rp** | **—** | 4 | Y | y | 5 | X | x | **6** | **Lp** | **—** |
| 7 | LB | l1 | 8 | RB | r1 | 9 | L2 | trigger l2 | 10 | R2 | trigger r2 |
| 11 | Select | select | 12 | Start | start | 13 | Home | home | 14 | L3 | l3 | 15 | R3 | r3 |
| **17** | **L4** | **—** | **18** | **R4** | **—** |

### Logitech F310 — D-mode (VID:046D PID:C216)

Gatillos digitales (botones 7/8). Sin Home ni botones extra.

### Sony DualShock 4 v2 (VID:054C PID:09CC)

L2/R2 analógicos (dwUpos/dwVpos). USB y BT comparten VID/PID.

---

## Archivos de datos

| Archivo | Descripción |
|---|---|
| `data/controllers.json` | Configuración base de mandos físicos |
| `data/FinalFantasyX.json` | Perfil de juego para FFX |
| `data/macros.json` | Biblioteca de macros reutilizables |
| `data/virtualpad.json` | VID/PID del mando virtual + nivel de log |

Ver [MACROS.md](MACROS.md) para la sintaxis completa de macros.
