// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "asset_download.hpp"

#include <curl/curl.h>
#include <openssl/evp.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
namespace fs = std::filesystem;

volatile std::sig_atomic_t cancelled=0;
void cancel_transfer(int) { cancelled=1; }

struct InterruptGuard {
    using Handler=void (*)(int);
    Handler previous_int;
    Handler previous_term;
    InterruptGuard() {
        cancelled=0;
        previous_int=std::signal(SIGINT,cancel_transfer);
        previous_term=std::signal(SIGTERM,cancel_transfer);
    }
    ~InterruptGuard() {
        std::signal(SIGINT,previous_int);
        std::signal(SIGTERM,previous_term);
    }
};

constexpr DownloadAsset earth{
    "earth",
    // Same immutable payload and Nix fetchurl digest as flake.nix. The URL
    // and digest must always be updated together after verifying new bytes.
    "https://raw.githubusercontent.com/turban/webgl-earth/master/images/2_no_clouds_4k.jpg",
    "61cd3d27489d80ed025d7954539a6d90b50d4433bbdd27e8c1af8a6f15f71e86",
    12u * 1024u * 1024u
};

std::string hex_digest(const unsigned char* digest, unsigned length) {
    constexpr char hex[]="0123456789abcdef";
    std::string result;
    for (unsigned i=0;i<length;++i) {
        result+=hex[digest[i]>>4];
        result+=hex[digest[i]&15];
    }
    return result;
}

bool valid_digest(std::string_view s) {
    if (s.size()!=64) return false;
    for (const char c : s) {
        if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
    }
    return true;
}

std::string digest_file(const fs::path& filename, std::size_t max_bytes) {
    std::error_code ec;
    if (!fs::is_regular_file(filename,ec) || fs::file_size(filename,ec)>max_bytes || ec)
        return {};
    std::ifstream input(filename,std::ios::binary);
    if (!input) return {};
    using Context=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>;
    Context ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)!=1) return {};
    std::array<char,64*1024> buffer{};
    std::size_t count=0;
    while (input) {
        input.read(buffer.data(),buffer.size());
        const auto n=input.gcount();
        if (n<0 || static_cast<std::size_t>(n)>max_bytes-count) return {};
        count+=static_cast<std::size_t>(n);
        if (n && EVP_DigestUpdate(ctx.get(),buffer.data(),static_cast<std::size_t>(n))!=1)
            return {};
    }
    if (!input.eof()) return {};
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned size=0;
    if (EVP_DigestFinal_ex(ctx.get(),digest,&size)!=1 || size!=32) return {};
    return hex_digest(digest,size);
}

struct TemporaryFile {
    fs::path path;
    FILE* stream=nullptr;
    ~TemporaryFile() {
        if (stream) std::fclose(stream);
        if (!path.empty()) {
            std::error_code ignored;
            fs::remove(path,ignored);
        }
    }
};

struct Transfer {
    FILE* stream=nullptr;
    std::size_t count=0;
    std::size_t limit=0;
    bool exceeded_limit=false;
    bool write_failed=false;
    bool report_progress=true;
    std::chrono::steady_clock::time_point last_progress=std::chrono::steady_clock::now();
};

std::size_t write_chunk(char* data, std::size_t size, std::size_t count, void* context) {
    auto& state=*static_cast<Transfer*>(context);
    if (cancelled) return 0;
    if (size && count > std::numeric_limits<std::size_t>::max()/size) {
        state.exceeded_limit=true;
        return 0;
    }
    const auto length=size*count;
    if (length>state.limit-state.count) {
        state.exceeded_limit=true;
        return 0;
    }
    const std::size_t written=std::fwrite(data,1,length,state.stream);
    state.count+=written;
    if (written!=length) state.write_failed=true;
    return written;
}

int show_progress(void* context, curl_off_t total, curl_off_t now,
                  curl_off_t, curl_off_t) {
    auto& state=*static_cast<Transfer*>(context);
    if (cancelled) return 1;
    if (!state.report_progress) return 0;
    const auto time=std::chrono::steady_clock::now();
    if (time-state.last_progress>=std::chrono::seconds(2)) {
        if (total>0) std::fprintf(stderr,"globtoo: downloading: %lld / %lld bytes\n",
                                  static_cast<long long>(now),static_cast<long long>(total));
        else std::fprintf(stderr,"globtoo: downloading: %lld bytes\n",
                          static_cast<long long>(now));
        state.last_progress=time;
    }
    return 0;
}
} // namespace

const DownloadAsset* find_download_asset(std::string_view name) {
    return name==earth.name ? &earth : nullptr;
}

DownloadResult download_asset(const DownloadAsset& asset,
                              const fs::path& cache_path,
                              std::string& message,
                              bool allow_test_file_scheme) {
    message.clear();
    if (cache_path.empty() || asset.maximum_bytes==0 ||
        !valid_digest(asset.sha256_hex) ||
        !(asset.url.starts_with("https://") ||
          (allow_test_file_scheme && asset.url.starts_with("file://")))) {
        message="invalid or untrusted download specification";
        return DownloadResult::Failed;
    }
    if (digest_file(cache_path,asset.maximum_bytes)==asset.sha256_hex) {
        message="already cached and SHA-256 verified: "+cache_path.string();
        return DownloadResult::AlreadyCached;
    }
    std::error_code ec;
    fs::create_directories(cache_path.parent_path(),ec);
    if (ec) { message="cannot create cache directory: "+ec.message(); return DownloadResult::Failed; }

    // mkstemp prevents predictable-name and symlink attacks; writing beside
    // the target makes the final rename atomic even across mount points.
    std::string template_path=cache_path.string()+".tmp.XXXXXX";
    std::vector<char> buffer(template_path.begin(),template_path.end());
    buffer.push_back('\0');
    int fd=mkstemp(buffer.data());
    if (fd<0) { message="cannot create temporary download file"; return DownloadResult::Failed; }
    TemporaryFile temp;
    temp.path=buffer.data();
    temp.stream=fdopen(fd,"wb");
    if (!temp.stream) {
        close(fd);
        message="cannot open temporary download stream";
        return DownloadResult::Failed;
    }

    using Handle=std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>;
    Handle curl(curl_easy_init(),curl_easy_cleanup);
    if (!curl) { message="libcurl initialization failed"; return DownloadResult::Failed; }
    InterruptGuard interrupts;
    Transfer transfer{temp.stream,0,asset.maximum_bytes};
    transfer.report_progress=!allow_test_file_scheme;
    std::string url(asset.url);
    curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS_STR,
                     allow_test_file_scheme ? "https,file" : "https");
    curl_easy_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl.get(),CURLOPT_MAXREDIRS,4L);
    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,20L);
    curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,180L);
    curl_easy_setopt(curl.get(),CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(curl.get(),CURLOPT_MAXFILESIZE_LARGE,
                     static_cast<curl_off_t>(asset.maximum_bytes));
    curl_easy_setopt(curl.get(),CURLOPT_USERAGENT,"globtoo/0.3 asset fetcher");
    curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,write_chunk);
    curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&transfer);
    curl_easy_setopt(curl.get(),CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl.get(),CURLOPT_XFERINFOFUNCTION,show_progress);
    curl_easy_setopt(curl.get(),CURLOPT_XFERINFODATA,&transfer);
    const CURLcode code=curl_easy_perform(curl.get());
    if (cancelled) {
        message="download interrupted; no cache file was published";
        return DownloadResult::Failed;
    }
    if (code!=CURLE_OK) {
        if (transfer.exceeded_limit || code==CURLE_FILESIZE_EXCEEDED)
            message="download exceeded configured size limit";
        else if (transfer.write_failed) message="disk write failed during download";
        else message=std::string("download failed: ")+curl_easy_strerror(code);
        return DownloadResult::Failed;
    }
    if (std::fflush(temp.stream)!=0 || fsync(fileno(temp.stream))!=0) {
        message="cannot flush downloaded file to disk";
        return DownloadResult::Failed;
    }
    if (std::fclose(temp.stream)!=0) {
        temp.stream=nullptr;
        message="cannot close downloaded file";
        return DownloadResult::Failed;
    }
    temp.stream=nullptr;
    if (digest_file(temp.path,asset.maximum_bytes)!=asset.sha256_hex) {
        message="SHA-256 mismatch: downloaded bytes differ from pinned manifest";
        return DownloadResult::Failed;
    }
    if (cancelled) {
        message="download interrupted; no cache file was published";
        return DownloadResult::Failed;
    }
    fs::rename(temp.path,cache_path,ec);
    if (ec) { message="cannot publish verified cache entry: "+ec.message(); return DownloadResult::Failed; }
    temp.path.clear();
    message="downloaded and SHA-256 verified: "+cache_path.string();
    return DownloadResult::Downloaded;
}
