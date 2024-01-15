//
// Created by josh on 1/2/24.
//

#pragma once
#include <fmt/core.h>
#include <stdexcept>

namespace dragonfire {

class FormattedError : public std::runtime_error {
public:
    template<typename... Args>
    explicit FormattedError(fmt::format_string<Args...> msg, Args&&... args)
        : runtime_error(fmt::format(msg, std::forward<Args>(args)...))
    {
    }
};

}// namespace dragonfire