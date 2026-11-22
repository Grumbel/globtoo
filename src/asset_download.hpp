// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

// Optional, separately linked libcurl + OpenSSL component. The ordinary fat
// viewer does not compile or link this module, and never makes HTTP requests.
struct DownloadAsset {
    std::string_view name;
    std::string_view url;
    std::string_view sha256_hex;
    std::size_t maximum_bytes;
};

enum class DownloadResult { AlreadyCached, Downloaded, Failed };

// A small source-controlled catalog: no remote manifest, unchecked URL, or
// arbitrary user-supplied endpoint can change the expected download hash.
const DownloadAsset* find_download_asset(std::string_view name);

// The cache pathname must be local and on a writable filesystem. Existing
// valid files are reused. Other files are atomically replaced only after the
// complete payload has passed the exact size bound and SHA-256 check.
// allow_test_file_scheme is ONLY for hermetic file:// test fixtures; public
// CLI callers use the HTTPS-only default.
DownloadResult download_asset(const DownloadAsset& asset,
                              const std::filesystem::path& cache_path,
                              std::string& message,
                              bool allow_test_file_scheme = false);
