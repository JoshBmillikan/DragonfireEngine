//
// Created by josh on 3/30/24.
//

#include "octree.h"
#include <cassert>

namespace dragonfire {
const Octree::Node& Octree::getNode(const VoxelIndexType index) const noexcept
{
    assert(index > 0);
    return nodes[index - 1];
}
} // dragonfire