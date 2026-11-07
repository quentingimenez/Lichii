// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>

namespace VideoCommon::Nx2Perf {

inline bool Verify() {
    static const bool verify = [] {
        const char* value = std::getenv("EDEN_NX2_PERF_VERIFY");
        return value != nullptr && value[0] == '1';
    }();
    return verify;
}

// EDEN_NX2_PERF_VERIFY_MASK selects individual #194 fast paths for bisection.
// bit 0 texture_cache/util, 1 device_memory_manager, 2-5 buffer_cache,
// 6 word_manager, 7 usage_tracker, 8-9 memory_manager, 10-11 maxwell_3d,
// 12 vk_pipeline_cache, 13 vk_buffer_cache. EDEN_NX2_PERF_VERIFY=1 still
// enables every path.
inline bool Verify(unsigned path) {
    static const unsigned mask = [] {
        const char* value = std::getenv("EDEN_NX2_PERF_VERIFY_MASK");
        return value != nullptr ? static_cast<unsigned>(std::strtoul(value, nullptr, 0)) : 0U;
    }();
    return Verify() || ((mask >> path) & 1U) != 0;
}

} // namespace VideoCommon::Nx2Perf
