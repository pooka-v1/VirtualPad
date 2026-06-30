#pragma once

#include "../GamepadState.h"

// Output port (hexagonal architecture): the abstraction PadEngine pushes the
// final GamepadState to, without knowing where it actually goes.
//
// Each concrete destination (a virtual Xbox 360 pad, a virtual DualShock 4, and
// later possibly a network/uinput sink) is an ADAPTER that implements this port.
// The engine only ever holds an IOutputSink, so adapters stay decoupled from the
// engine and from each other; the concrete type is chosen once at start-up
// (composition root) from the user config.
class IOutputSink {
public:
    // True once the underlying device was created and is ready to receive frames.
    virtual bool isReady() const = 0;

    // Push the current virtual pad state to the destination, once per frame.
    virtual void update(const GamepadState& state) = 0;

    // Virtual: the engine owns adapters through an IOutputSink pointer, so the
    // base destructor MUST be virtual or deleting via the base pointer would be
    // undefined behaviour (the adapter's own destructor would never run).
    virtual ~IOutputSink() = default;
};
