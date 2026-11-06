// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan.h"

namespace Vulkan {

inline bool BuildsUnoptimized(VkDriverId driver_id, u64 shader_hash) {
    // The Legend of Zelda: Ocarina of Time (0400486024DEC000), driver 2.0.353 on an RX 6600 XT
    static constexpr std::array<u64, 10> amd_proprietary{
        0xeff3ccbe1438679dULL, // compute shader built as the AV1 intro starts: 6 min (#200)
        0xf9ea14a22394f5b1ULL, // compute shader built as the AV1 intro starts: 18 min (#200)
        // Compute pipelines Ocarina of Time builds as a save loads, immediately before AMD's
        // Windows driver device-losses. Keep them out of the driver's optimizer to test whether
        // its compiler is what stalls or hangs.
        0xd9ee412f00ce399aULL,
        0xe40f5efb5480e692ULL,
        0xfc10c1232f7ac3f3ULL,
        0xda1246f26b1e83e7ULL,
        0xa107e12b6771be48ULL,
        0xce9f15d1fe2e4101ULL, // Ocarina's vegetation GPU culling, last pipeline in build-88's loss
        // The neon-green light/fog volumes from issue #285: the world pass reads their output
        // green-heavy before fog and post-processing. Disable the driver optimizer on them too.
        0x75d4369f3b1f5868ULL, // 64x36x96 B10G11R11 light volume writer
        0x780c0536b89fc89eULL, // fog integration, green-heavy output
    };
    return driver_id == VK_DRIVER_ID_AMD_PROPRIETARY &&
           std::ranges::find(amd_proprietary, shader_hash) != amd_proprietary.end();
}

} // namespace Vulkan
