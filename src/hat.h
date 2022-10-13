#pragma once

#include <linux/input.h>

#include "event.h"

struct Hat : EventListener<KeyDownEvent>, EventListener<KeyUpEvent> {
    WinProcess* proc;
    EventManager events;
    bool keyDown[KEY_MAX]{};

    explicit Hat(WinProcess* rust): proc(rust) {
        events.Subscribe<KeyDownEvent>(*this);
        events.Subscribe<KeyUpEvent>(*this);
    }

    void onEvent(KeyDownEvent&);
    void onEvent(KeyUpEvent&);
};