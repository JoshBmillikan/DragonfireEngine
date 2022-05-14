//
// Created by Josh on 5/14/2022.
//

#include "Service.h"
using namespace dragonfire;

EXPORTED std::vector<Service*> services;

EXPORTED void Service::destroyServices() noexcept {
    std::for_each(::services.rbegin(), ::services.rend(), [](auto ptr) { delete ptr; });
}
