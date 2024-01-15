//
// Created by josh on 8/10/23.
//

#pragma once
#include <fmt/format.h>
#include <source_location>

namespace dragonfire {

#ifdef NDEBUG
[[noreturn]] void crash(const char* msg);

template<typename... Args>
[[noreturn]] inline void crash(fmt::format_string<Args...> msg, Args&&... args)
{
    crash(fmt::format(msg, std::forward<Args>(args)...).c_str());
}

inline void require(bool condition)
{
    if (!condition)
        crash("Required condition was invalid");
}

template<typename FN, typename R = void>
R crashOnException(FN&& func) noexcept
{
    try {
        return func();
    }
    catch (const std::exception& e) {
        crash("Unhandled exception: {}", e.what());
    }
    catch (...) {
        crash("Unkown exception thrown");
    }
}

#else

[[noreturn]] void crash(
    const char* msg,
    const std::source_location& location = std::source_location::current()
);

template<typename T1>
[[noreturn]] inline void crash(
    fmt::format_string<T1> msg,
    T1 a,
    std::source_location location = std::source_location::current()
)
{
    crash(fmt::format(msg, std::forward<T1>(a)).c_str(), location);
}

template<typename T1, typename T2>
    requires(!std::is_same_v<T2, std::source_location>)
[[noreturn]] inline void crash(
    fmt::format_string<T1, T2> msg,
    T1 a,
    T2 b,
    std::source_location location = std::source_location::current()
)
{
    crash(fmt::format(msg, std::forward<T1>(a), std::forward<T2>(b)).c_str(), location);
}

template<typename T1, typename T2, typename T3>
    requires(!std::is_same_v<T3, std::source_location>)
[[noreturn]] inline void crash(
    fmt::format_string<T1, T2, T3> msg,
    T1 a,
    T2 b,
    T3 c,
    std::source_location location = std::source_location::current()
)
{
    crash(fmt::format(msg, std::forward<T1>(a), std::forward<T2>(b), std::forward<T3>(c)).c_str(), location);
}

template<typename T1, typename T2, typename T3, typename T4>
    requires(!std::is_same_v<T4, std::source_location>)
[[noreturn]] inline void crash(
    fmt::format_string<T1, T2, T3, T4> msg,
    T1 a,
    T2 b,
    T3 c,
    T4 d,
    std::source_location location = std::source_location::current()
)
{
    crash(
        fmt::format(msg, std::forward<T1>(a), std::forward<T2>(b), std::forward<T3>(c), std::forward<T4>(d))
            .c_str(),
        location
    );
}

inline void require(bool condition, std::source_location location = std::source_location::current())
{
    if (!condition)
        crash("Required condition was invalid", location);
}

template<typename FN, typename R = void>
R crashOnException(FN&& func, const std::source_location& location = std::source_location::current()) noexcept
{
    try {
        return func();
    }
    catch (const std::exception& e) {
        crash("Unhandled exception: {}", e.what(), location);
    }
    catch (...) {
        crash("Unkown exception thrown", location);
    }
}

#endif

}// namespace dragonfire