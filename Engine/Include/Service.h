//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Engine.h"
#include <concepts>

namespace dragonfire {
class Service {
public:
    virtual ~Service() noexcept = default;

    /// Installs a service into the service locator.<br>
    /// After calling, this object is owned by the
    /// service locator and must <b>not</b> be freed manually
    inline void install() noexcept {
        services.push_back(this);
    }

    template<class T, typename ...Args>
    requires std::is_base_of_v<Service, T>
    static void init(Args... args) {
        auto ptr = new T(std::forward(args...));
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

    EXPORTED static void destroyServices() noexcept;
private:
    EXPORTED static std::vector<Service*> services;

    template<class T>
    static T* find() {
        for (Service* service : services) {
            auto ptr = dynamic_cast<T*>(service);
            if (ptr) return ptr;
        }
        throw std::out_of_range("Could not located requested service");
    }

};

}   // namespace dragonfire
