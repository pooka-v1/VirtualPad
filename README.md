# VirtualPad

Lee mandos físicos (WinMM) y los reenvía como un mando Xbox 360 virtual via ViGEm.

---

## Requisitos

| Dependencia | Motivo |
|---|---|
| Windows 10/11 | API requeridas: WinMM |
| [ViGEmBus driver](https://github.com/nefarius/ViGEmBus/releases) | Crea el mando Xbox 360 virtual |

### Para compilar

- Visual Studio 2022 (con soporte C++17 y Windows SDK)

---

## Cómo funciona

1. Escanea los puertos WinMM buscando joysticks conectados.
2. Lee el estado del mando físico (`joyGetPosEx`): ejes, botones, D-pad.
3. Normaliza el estado a un `GamepadState` interno.
4. Envía el `GamepadState` al mando Xbox 360 virtual creado por ViGEmBus.

El loop corre en consola. El mando físico queda emulado como Xbox 360 para cualquier juego.

---

## Arquitectura

| Componente | Rol |
|---|---|
| `IInputSource` | Interfaz abstracta de lectura (strategy pattern) |
| `EightBitDoInputSource` | Lee mando físico vía WinMM, normaliza a `GamepadState` |
| `GamepadState` | Estado normalizado compartido (sticks, triggers, botones, dpad) |
| `ViGEmOutputAdapter` | Crea y actualiza el mando Xbox 360 virtual vía ViGEmBus |
| `VirtualPad.cpp` | Loop principal: scan → lectura → forwarding |
