//
// Created by Josh on 5/14/2022.
//

#pragma once

#ifdef _WIN32
    #ifdef _DLL
        #define EXPORTED __declspec(dllexport)
    #else
        #define EXPORTED __declspec(dllimport)
    #endif
#else
    #define EXPORTED
#endif

#ifndef APP_NAME
    #define APP_NAME "DragonfireEngine"
#endif