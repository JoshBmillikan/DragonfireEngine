//
// Created by josh on 5/1/24.
//

#pragma once
#include "utility/string_hash.h"
#include "utility/frame_allocator.h"
#include <any>
#include <functional>

namespace dragonfire::event {

class Event {
public:
    virtual ~Event() = default;

};

struct AxisEvent : Event {

};

struct ButtonEvent : Event {

};

struct AnyEvent : Event {
    std::any data;
};

class EventBus {
public:
    void signalEvent(std::string_view eventId);

private:
    StringMultiMap<Event*, FrameAllocator<std::pair<const std::string, Event*>>> events;

};

} // dragonfire
