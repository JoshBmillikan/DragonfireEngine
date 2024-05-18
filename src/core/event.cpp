//
// Created by josh on 5/1/24.
//

#include "event.h"

namespace dragonfire::event {
void EventBus::signalEvent(const std::string_view eventId)
{
    const auto range = events.equal_range(eventId);
    for (auto item = range.first; item != range.second; ++item) {
        auto& [id, event] = *item;
        
    }
}
}// namespace dragonfire
