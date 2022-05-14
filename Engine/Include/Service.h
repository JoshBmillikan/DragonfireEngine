//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Engine.h"
#include <concepts>

namespace dragonfire {
class Service {
    EXPORTED static std::vector<Service*> services;

    template<class T>
    static T* find() {
        for (Service* service : services) {
            auto ptr = dynamic_cast<T*>(service);
            if (ptr) return ptr;
        }
        throw std::out_of_range("Could not located requested service");
    }

public:
    void install() noexcept {
        services.push_back(this);
    }

    template<class T, typename ...Args>
    requires std::is_base_of_v<T, Service>
    static void createService(Args... args) {
        auto ptr = new T(std::forward(args...));
        ptr->install();
    }

    template<class T>
    requires std::is_base_of_v<T,Service>
    static inline T& get() {
        static T* ptr = find<T>();
        return *ptr;
    }

    EXPORTED void destroyServices() noexcept;
};

}   // namespace dragonfire
