//
// Created by josh on 1/22/24.
//

#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>

namespace dragonfire {

template<typename T, size_t SMALL_SIZE = std::max(40 / sizeof(T), 1ul), typename Alloc = std::allocator<T>>
class SmallVector {
public:
    SmallVector() { endPtr = start = inner.inlineData; }

    explicit SmallVector(const std::span<T> span) : SmallVector()
    {
        reserve(span.size());
        for (auto elem : span) {
            pushBack(elem);
        }
    }

    [[nodiscard]] size_t size() const noexcept { return std::distance(start, endPtr); }

    [[nodiscard]] bool isSpilled() const noexcept
    {
        return reinterpret_cast<intptr_t>(start) != reinterpret_cast<intptr_t>(&inner);
    }

    [[nodiscard]] size_t capacity() const noexcept
    {
        return isSpilled() ? std::distance(start, inner.cap) : SMALL_SIZE;
    }

    void clear() { endPtr = start; }

    T* data() noexcept { return start; }

    operator std::span<T>() const { return std::span<T>(start, size()); }

    const T& operator[](size_t index) const noexcept
    {
        assert(index < size());
        return start[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index < size());
        return start[index];
    }

    template<typename V>
    T& pushBack(const V val)
    {
        grow();
        return *endPtr++ = val;
    }

    template<typename... Args>
    T& emplace(Args... args)
    {
        grow();
        T* ptr = new (endPtr++) T(std::forward<Args>(args)...);
        return *ptr;
    }

    void reserve(const size_t size)
    {
        if (capacity() < size)
            realloc(size);
    }

    [[nodiscard]] bool contains(const T& val) const noexcept
    {
        for (auto i = begin(); i != end(); i++) {
            if (*i == val)
                return true;
        }
        return false;
    }

    SmallVector(const SmallVector& other)
        : start(other.start), endPtr(other.endPtr), allocator(other.allocator)
    {
    }

    SmallVector(SmallVector&& other) noexcept
        : start(other.start), endPtr(other.endPtr), allocator(std::move(other.allocator))
    {
        other.start = nullptr;
    }

    SmallVector& operator=(const SmallVector& other)
    {
        if (this == &other)
            return *this;
        start = other.start;
        endPtr = other.endPtr;
        allocator = other.allocator;
        return *this;
    }

    [[nodiscard]] bool empty() const { return start == endPtr; }

    SmallVector& operator=(SmallVector&& other) noexcept
    {
        if (this == &other)
            return *this;
        start = other.start;
        other.start = nullptr;
        endPtr = other.endPtr;
        allocator = std::move(other.allocator);
        return *this;
    }

    ~SmallVector()
    {
        if (start) {
            const auto s = size();
            for (size_t i = 0; i < s; i++) {
                start[i].~T();
            }
            if (s > 0 && isSpilled())
                allocator.deallocate(start, capacity());
        }
    }

    class Iterator {
        T* ptr;

    public:
        using difference_type = std::ptrdiff_t;// NOLINT(*-identifier-naming)
        using value_type = T;                  // NOLINT(*-identifier-naming)
        using pointer = value_type*;           // NOLINT(*-identifier-naming)
        using reference = value_type&;         // NOLINT(*-identifier-naming)

        Iterator() : ptr(nullptr) {}

        Iterator(T* ptr) : ptr(ptr) {}

        reference operator*() const { return *ptr; }

        pointer operator->() { return ptr; }

        Iterator& operator++()
        {
            ptr++;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator& operator--()
        {
            ptr--;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        auto operator<=>(const Iterator&) const = default;
    };

    [[nodiscard]] Iterator begin() const { return Iterator(start); }

    [[nodiscard]] Iterator end() const { return Iterator(endPtr); }

private:
    static constexpr size_t GROWTH_FACTOR = 2;
    T* start = nullptr;
    T* endPtr = nullptr;
    Alloc allocator;

    union {
        T inlineData[SMALL_SIZE]{};
        T* cap;
    } inner;

    void grow()
    {
        if (isSpilled()) {
            if (endPtr >= inner.cap)
                realloc(capacity() * GROWTH_FACTOR);
        }
        else {
            if (endPtr >= start + SMALL_SIZE)
                realloc(SMALL_SIZE * GROWTH_FACTOR);
        }
    }

    void realloc(const size_t newSize)
        requires(!std::is_trivially_copyable_v<T>)
    {
        auto newPtr = allocator.allocate(newSize);
        const auto current = size();
        for (size_t i = 0; i < current; i++)
            newPtr[i] = std::move(start[i]);
        if (isSpilled())
            allocator.deallocate(start, capacity());
        start = newPtr;
        inner.cap = start + newSize;
        endPtr = start + current;
    }

    void realloc(const size_t newSize)
        requires std::is_trivially_copyable_v<T>
    {
        auto newPtr = allocator.allocate(newSize);
        const auto current = size();
        memcpy(newPtr, start, current * sizeof(T));
        if (isSpilled())
            allocator.deallocate(start, capacity());
        start = newPtr;
        inner.cap = start + newSize;
        endPtr = start + current;
    }
};

static_assert(std::bidirectional_iterator<SmallVector<int>::Iterator>);
}// namespace dragonfire
