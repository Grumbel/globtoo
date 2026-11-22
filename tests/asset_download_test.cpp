// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "asset_download.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;

int main() {
    const auto* earth=find_download_asset("earth");
    assert(earth && earth->maximum_bytes>1000000);
    assert(earth->sha256_hex.size()==64);
    assert(find_download_asset("mars")==nullptr);
    const auto root=fs::temp_directory_path()/
        ("globtoo-download-test-"+std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path input=root/"fixture.txt";
    const fs::path cached=root/"cache"/"assets"/"earth.jpg";
    std::ofstream(input,std::ios::binary)<<"abc";
    // NIST SHA-256 example vector; fixture is local, no network needed.
    const std::string expected="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const std::string url="file://"+input.string();
    DownloadAsset spec{"test",url,expected,16};
    std::string report;
    assert(download_asset(spec,cached,report)==DownloadResult::Failed);
    assert(!fs::exists(cached)); // file:// is not part of the production protocol policy
    assert(download_asset(spec,cached,report,true)==DownloadResult::Downloaded);
    assert(fs::file_size(cached)==3);
    assert(download_asset(spec,cached,report,true)==DownloadResult::AlreadyCached);

    // A corrupt cache entry is replaced only after verifying the new payload.
    std::ofstream(cached,std::ios::binary | std::ios::trunc)<<"xyz";
    assert(download_asset(spec,cached,report,true)==DownloadResult::Downloaded);
    {
        std::ifstream file(cached,std::ios::binary);
        assert(std::string(std::istreambuf_iterator<char>(file),{})=="abc");
    }

    // Wrong hash must not overwrite an already verified destination.
    DownloadAsset wrong{"test",url,std::string_view("0000000000000000000000000000000000000000000000000000000000000000"),16};
    assert(download_asset(wrong,cached,report,true)==DownloadResult::Failed);
    assert(report.find("SHA-256 mismatch")!=std::string::npos);
    std::ifstream good(cached,std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(good),{})=="abc");
    good.close();

    // Verify the callback aborts oversized transfers even without Content-Length.
    DownloadAsset small{"test",url,expected,2};
    assert(download_asset(small,cached,report,true)==DownloadResult::Failed);
    assert(report.find("size limit")!=std::string::npos);
    assert(fs::file_size(cached)==3);

    // A missing local source leaves the prior cached file intact, and the
    // temporary download file must not survive a failure.
    DownloadAsset missing{"test","file:///does-not-exist-globtoo-test",wrong.sha256_hex,16};
    assert(download_asset(missing,cached,report,true)==DownloadResult::Failed);
    assert(fs::file_size(cached)==3);
    for (const auto& entry:fs::directory_iterator(cached.parent_path()))
        assert(entry.path().filename()=="earth.jpg");
    fs::remove_all(root);
}
