#pragma once

#include <typeindex>
#include <unordered_map>
#include <vector>

struct EventListenerBase {
    virtual void dummy() {}; // allows dynamic_cast to be used
};

template<typename T>
struct EventListener : virtual EventListenerBase {
    virtual void onEvent(T& event) = 0;
};

struct EventManager {
    std::unordered_map<std::type_index, std::vector<EventListenerBase*>> listeners;

    template<typename T>
    void Post(T& event) {
        using derived_t = EventListener<T>;
        auto it = listeners.find(std::type_index{typeid(derived_t)});
        if (it != listeners.end()) {
            for (auto* base : it->second) {
                dynamic_cast<derived_t*>(base)->onEvent(event);
            }
        }
    }

    template<typename T>
    void Subscribe(EventListener<T>& listener) {
        listeners[std::type_index{typeid(EventListener<T>)}].push_back(&listener);
    }
};

struct KeyDownEvent {
    int sc;
};

struct KeyUpEvent {
    int sc;
};