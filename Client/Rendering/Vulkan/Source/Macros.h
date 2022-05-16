//
// Created by Josh on 5/14/2022.
// Macros needed to be defined before including vulkan.hpp
// Since vulkan.hpp is in the precompiled header, I created this file
// to be put in the precompiled header before it
// Global defines from the compiler command line would also have worked

#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1