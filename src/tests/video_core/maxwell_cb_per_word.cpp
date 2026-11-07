// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "common/common_types.h"

namespace {

// The words a draw macro writes to c[0][0x640]. The actual values only need to be
// distinct so that a partially visible run is unambiguous.
constexpr std::array<u32, 4> Words{
    0x11111111,
    0x22222222,
    0x33333333,
    0x44444444,
};

struct Checkpoint {
    std::array<u32, 4> memory{};
    u32 offset{};
};

// Mirrors the write ordering in Maxwell3D::CallConstBufferWords.
//
// The batch path handles every word first and uploads the run afterwards
// (WriteWordsCached), so a method that runs between those handles sees the old
// contents. The EDEN_NX2_CB_PER_WORD path calls CallMethod for each word, whose
// ProcessCBData writes that word before the next method is handled.
Checkpoint AtCheckpoint(bool per_word, u32 completed_words) {
    Checkpoint checkpoint;
    for (u32 i = 0; i < completed_words; ++i) {
        if (per_word) {
            checkpoint.memory[i] = Words[i];
            checkpoint.offset += sizeof(u32);
        }
    }
    if (!per_word && completed_words == Words.size()) {
        checkpoint.memory = Words;
        checkpoint.offset = static_cast<u32>(Words.size() * sizeof(u32));
    }
    return checkpoint;
}

std::string CompactWhitespace(std::string_view text) {
    std::string compact;
    compact.reserve(text.size());
    for (const char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            compact.push_back(c);
        }
    }
    return compact;
}

std::optional<std::filesystem::path> FindUpwards(std::filesystem::path directory) {
    for (u32 depth = 0; depth < 12; ++depth) {
        std::error_code error;
        const auto candidate =
            directory / "src" / "video_core" / "engines" / "maxwell_3d.cpp";
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
        const auto parent = directory.parent_path();
        if (parent.empty() || parent == directory) {
            break;
        }
        directory = parent;
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> FindMaxwell3DSource() {
    const std::filesystem::path file = __FILE__;
    if (const auto found = FindUpwards(file.parent_path())) {
        return found;
    }
    return FindUpwards(std::filesystem::current_path());
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    REQUIRE(file.is_open());
    return std::string{std::istreambuf_iterator<char>{file},
                       std::istreambuf_iterator<char>{}};
}

} // Anonymous namespace

TEST_CASE("EDEN_NX2_CB_PER_WORD: per-word writes are visible before the batch upload point",
          "[video_core][maxwell_cb_per_word]") {
    // Before the run starts, both orders see the previous constant buffer contents.
    REQUIRE(AtCheckpoint(false, 0).memory == std::array<u32, 4>{});
    REQUIRE(AtCheckpoint(true, 0).memory == std::array<u32, 4>{});

    // In the middle of the run, the batch path has not touched memory yet while
    // the per-word path has published every word handled so far.
    for (u32 completed = 1; completed < Words.size(); ++completed) {
        CAPTURE(completed);
        const auto batch = AtCheckpoint(false, completed);
        REQUIRE(batch.memory == std::array<u32, 4>{});
        REQUIRE(batch.offset == 0);

        const auto per_word = AtCheckpoint(true, completed);
        for (u32 i = 0; i < Words.size(); ++i) {
            REQUIRE(per_word.memory[i] == (i < completed ? Words[i] : 0));
        }
        REQUIRE(per_word.offset == completed * sizeof(u32));
    }

    // Once the run is handled, both orders have the same contents and offset: the
    // difference is only whether an earlier method could observe a partial run.
    const auto batch = AtCheckpoint(false, static_cast<u32>(Words.size()));
    const auto per_word = AtCheckpoint(true, static_cast<u32>(Words.size()));
    REQUIRE(batch.memory == Words);
    REQUIRE(per_word.memory == Words);
    REQUIRE(batch.offset == Words.size() * sizeof(u32));
    REQUIRE(per_word.offset == Words.size() * sizeof(u32));
}

TEST_CASE("EDEN_NX2_CB_PER_WORD: the per-word branch stays before the batch upload",
          "[video_core][maxwell_cb_per_word][source]") {
    const auto path = FindMaxwell3DSource();
    if (!path) {
        SKIP("maxwell_3d.cpp not found from "
             << std::filesystem::current_path().string() << " (" << __FILE__ << ')');
    }

    const std::string source = CompactWhitespace(ReadText(*path));

    constexpr std::string_view function_start = "voidMaxwell3D::CallConstBufferWords";
    constexpr std::string_view function_end = "voidMaxwell3D::CallMethodSlow";
    const auto start = source.find(function_start);
    REQUIRE(start != std::string::npos);
    const auto end = source.find(function_end, start);
    REQUIRE(end != std::string::npos);
    const std::string body = source.substr(start, end - start);

    // The environment switch is read once, with the same 0/1 convention as the
    // other Nx2Perf switches.
    const auto switch_position = body.find("EDEN_NX2_CB_PER_WORD");
    REQUIRE(switch_position != std::string::npos);
    REQUIRE(body.find("staticconstboolper_word=[]", switch_position) !=
            std::string::npos);
    REQUIRE(body.find("value!=nullptr&&value[0]=='1'", switch_position) !=
            std::string::npos);

    const auto verify_position = body.find("constboolverify=VideoCommon::Nx2Perf::Verify();");
    REQUIRE(verify_position != std::string::npos);

    // Both verification and the per-word switch take the per-word path.
    const auto branch_position = body.find("if(verify||per_word){", verify_position);
    REQUIRE(branch_position != std::string::npos);
    REQUIRE(switch_position < branch_position);

    const auto call_position =
        body.find("CallMethod(system,methods[i],arguments[i],true);", branch_position);
    REQUIRE(call_position != std::string::npos);

    // EDEN_NX2_CB_PER_WORD returns without the verify readback; the batch upload
    // must stay after that return so the per-word branch cannot fall through to it.
    const auto per_word_return = body.find("if(!verify){return;}", call_position);
    REQUIRE(per_word_return != std::string::npos);
    const auto readback =
        body.find("memory_manager.ReadBlockUnsafe(address+offset,written.data(),bytes);",
                  per_word_return);
    REQUIRE(readback != std::string::npos);
    const auto batch_write = body.find(
        "memory_manager.WriteWordsCached(address+offset,values.data(),count);",
        per_word_return);
    REQUIRE(batch_write != std::string::npos);

    REQUIRE(branch_position < call_position);
    REQUIRE(call_position < per_word_return);
    REQUIRE(per_word_return < readback);
    REQUIRE(per_word_return < batch_write);
}
