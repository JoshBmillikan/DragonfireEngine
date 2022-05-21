//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Engine.h"
#include <concepts>
#include <shared_mutex>

namespace dragonfire {
class EXPORTED Service {
public:
    virtual ~Service() noexcept = default;

    /// Installs a service into the service locator.<br>
    /// After calling, this object is owned by the
    /// service locator and must <b>not</b> be freed manually
    void install() noexcept {
        std::unique_lock lock(mutex);
        services.push_back(this);
    }

    template<class T, typename... Args>
        requires std::is_base_of_v<Service, T>
    static void init(Args... args) {
        auto ptr = new T(args...);
        ptr->install();
    }

    template<class T>
        requires std::is_base_of_v<Service, T>
    static void init() {
        auto ptr = new T();
        ptr->install();
    }

    template<class T>
        requires std::is_base_of_v<Service, T>
    static T& get() {
        static T* ptr = find<T>();
        return *ptr;
    }

    static void destroyServices() noexcept {
        std::for_each(services.rbegin(), services.rend(), [](auto ptr) { delete ptr; });
        services.clear();
    }

private:
    inline static std::vector<Service*> services;
    inline static std::shared_mutex mutex;
    template<class T>
        requires std::is_base_of_v<Service, T>
    static T* find() {
        std::shared_lock lock(mutex);
        for (Service* service : services) {
            auto ptr = dynamic_cast<T*>(service);
            if (ptr)
                return ptr;
        }
        throw std::out_of_range("Could not located requested service");
    }
};

}   // namespace dragonfire
