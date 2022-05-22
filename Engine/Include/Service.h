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

    /// Installs a service into the global service locator.<br>
    /// After calling, this object is owned by the
    /// service locator and must <b>not</b> be freed manually
    void install() noexcept {
        std::unique_lock lock(mutex);
        services.push_back(this);
    }

    /// \brief Creates a service
    /// \tparam T Type of the service, must inherit from the service class
    /// \tparam Args Type of the parameters for the service's constructor
    /// \param args parameters to pass to the service's constructor
    template<class T, typename... Args>
        requires std::is_base_of_v<Service, T>
    static void init(Args... args) {
        auto ptr = new T(args...);
        ptr->install();
    }

    /// \brief Creates a service with its default constructor
    /// \tparam T the type of the service, must inherit from the Service class
    template<class T>
        requires std::is_base_of_v<Service, T>
    static void init() {
        auto ptr = new T();
        ptr->install();
    }

    /// \brief Gets a reference to a service of the given type<br>
    ///
    /// Will perform a linear search for the service the first time it is called,
    /// but the pointer is cached for future calls to improve performance
    ///
    /// \tparam T Type of the service to get
    /// \return a reference to the service
    template<class T>
        requires std::is_base_of_v<Service, T>
    static T& get() {
        static T* ptr = find<T>();
        return *ptr;
    }

    /// \brief Destroys all active services<br>
    /// Services are destroyed in the reverse of the order they were created
    static void destroyServices() noexcept {
        std::unique_lock lock(mutex);
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
