// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "resources.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() /
        ("globtoo-resource-lookup-test-" + std::to_string(getpid()));
    fs::remove_all(base);
    fs::create_directories(base / "bin" / "shaders");
    fs::create_directories(base / "share" / "globtoo" / "shaders");
    fs::create_directories(base / "bin" / "assets");
    fs::create_directories(base / "share" / "globtoo" / "assets");
    fs::create_directories(base / "cwd" / "assets");
    std::ofstream(base / "bin" / "shaders" / "test.vert") << "build";
    std::ofstream(base / "share" / "globtoo" / "shaders" / "installed.vert") << "installed";
    std::ofstream(base / "bin" / "assets" / "earth.jpg") << "real-build-texture";
    std::ofstream(base / "share" / "globtoo" / "assets" / "earth.jpg") << "real-installed-texture";
    std::ofstream(base / "cwd" / "assets" / "earth.jpg") << "obsolete-placeholder";
    const auto original_cwd = fs::current_path();
    fs::current_path(base / "cwd");
    resources_set_executable_dir((base / "bin").string());
    assert(fs::equivalent(resource_path("shaders/test.vert"), base / "bin" / "shaders" / "test.vert"));
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("shaders/installed.vert"), base / "share" / "globtoo" / "shaders" / "installed.vert"));
#endif
    // A leftover source-tree placeholder may not override the application's
    // packaged image, even when launched from that source directory.
    assert(fs::equivalent(resource_path("assets/earth.jpg"),
                          base / "bin" / "assets" / "earth.jpg"));
    fs::remove(base / "bin" / "assets" / "earth.jpg");
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("assets/earth.jpg"),
                          base / "share" / "globtoo" / "assets" / "earth.jpg"));
#endif
    assert(resource_path("/tmp/absolute.txt") == "/tmp/absolute.txt");
    fs::create_directories(base / "custom" / "assets");
    std::ofstream(base / "custom" / "assets" / "earth.jpg") << "override";
    setenv("GLOBE_ASSET_DIR", (base / "custom").c_str(), 1);
    assert(fs::equivalent(resource_path("assets/earth.jpg"), base / "custom" / "assets" / "earth.jpg"));
    unsetenv("GLOBE_ASSET_DIR");
    fs::current_path(original_cwd);
    fs::remove_all(base);
}
