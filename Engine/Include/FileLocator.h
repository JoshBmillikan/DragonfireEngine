//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Service.h"
#include <filesystem>

namespace dragonfire {
class FileLocator : public Service {
public:
    EXPORTED FileLocator();
    const std::filesystem::path
            /// The base dir is the applications root directory, i.e. the directory where it's installed
            baseDir,

            /// The asset dir where static assets can be loaded from
            assetDir,

            /// The data dir for putting data that persists between runs, e.g. save data, caches, etc.
            dataDir,

            /// The directory to put config files, default configs are stored elsewhere and should never be modified,
            /// user saved config options go here
            configDir;
};
}   // namespace dragonfire
