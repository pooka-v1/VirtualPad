# VirtualPad

Lee mandos físicos (WinMM, HID) y los reenvía como un mando Xbox 360 virtual via ViGEm.
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
| `"dinput"` | WinMM | Aparece en Tab Scanner → WinMM |
| `"xinput"` | WinMM compat | Mando XInput en modo compatibilidad |
| `"hid"` | HID raw | Aparece en Tab Scanner → HID-only |

### Ejes WinMM (dinput/xinput)
`"dwXpos"`, `"dwYpos"`, `"dwZpos"`, `"dwRpos"`, `"dwUpos"`, `"dwVpos"` → targets `left_x/y`, `right_x/y`, `trigger_l/r`, `trigger_combined`.

### Ejes HID
| Nombre | Usage | Uso típico |
|---|---|---|
| `"hid_x"` / `"hid_y"` | 0x30/0x31 | Stick izquierdo |
| `"hid_z"` / `"hid_rz"` | 0x32/0x35 | Stick derecho |
| `"hid_brake"` | 0xC4 | Gatillo L2 analógico |
| `"hid_accel"` | 0xC5 | Gatillo R2 analógico |

D-pad HID: `"dpad": "hid_hat"`

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

## Archivos de datos

| Archivo | Descripción |
|---|---|
| `data/controllers.json` | Configuración base de mandos físicos |
| `data/macros.json` | Biblioteca de macros reutilizables |
| `data/virtualpad.json` | VID/PID del mando virtual + nivel de log |
