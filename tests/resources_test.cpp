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
    fs::create_directories(base / "share" / "globtoo" / "imagery" / "mars");
    fs::create_directories(base / "bin" / "imagery" / "moon");
    std::ofstream(base / "bin" / "shaders" / "test.vert") << "build";
    std::ofstream(base / "share" / "globtoo" / "shaders" / "installed.vert") << "installed";
    std::ofstream(base / "bin" / "assets" / "earth.jpg") << "real-build-texture";
    std::ofstream(base / "share" / "globtoo" / "assets" / "earth.jpg") << "real-installed-texture";
    // Planetary images are optional, but when packaged their standard
    // resource paths must work independently of the current directory.
    std::ofstream(base / "share" / "globtoo" / "assets" / "moon.jpg") << "lunar-map";
    std::ofstream(base / "bin" / "assets" / "mars.jpg") << "martian-map";
    std::ofstream(base / "share" / "globtoo" / "imagery" / "mars" / "manifest.txt") << "mars-tiles";
    std::ofstream(base / "bin" / "imagery" / "moon" / "manifest.txt") << "moon-tiles";
    std::ofstream(base / "cwd" / "assets" / "earth.jpg") << "obsolete-placeholder";
    // Give this test a private user-data root, independent of the runner's
    // actual desktop and its installed planetary maps.
    setenv("XDG_DATA_HOME", (base / "user-data").c_str(), 1);
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
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("assets/moon.jpg"),
                          base / "share" / "globtoo" / "assets" / "moon.jpg"));
#endif
    assert(fs::equivalent(resource_path("assets/mars.jpg"),
                          base / "bin" / "assets" / "mars.jpg"));
    assert(fs::equivalent(resource_path("imagery/moon/manifest.txt"),
                          base / "bin" / "imagery" / "moon" / "manifest.txt"));
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("imagery/mars/manifest.txt"),
                          base / "share" / "globtoo" / "imagery" / "mars" / "manifest.txt"));
#endif
    // User-installed planetary maps override the binary and system defaults;
    // they are persistent across immutable Nix package upgrades.
    const auto user_planets = base / "user-data" / "globtoo";
    assert(fs::path(resource_user_data_path("imagery/mars/manifest.txt")) ==
           user_planets / "imagery" / "mars" / "manifest.txt");
    assert(resource_user_data_path("../imagery/mars/manifest.txt").empty());
    assert(resource_user_data_path("imagery/earth/manifest.txt").empty());
    assert(resource_user_data_path("imagery/mars/../moon/manifest.txt").empty());
    assert(resource_user_data_path("assets/earth.jpg").empty());
    fs::create_directories(user_planets / "imagery" / "mars");
    fs::create_directories(user_planets / "assets");
    std::ofstream(user_planets / "imagery" / "mars" / "manifest.txt") << "user-mars";
    std::ofstream(user_planets / "assets" / "moon.jpg") << "user-moon";
    assert(fs::equivalent(resource_path("imagery/mars/manifest.txt"),
                          user_planets / "imagery" / "mars" / "manifest.txt"));
    assert(fs::equivalent(resource_path("assets/moon.jpg"),
                          user_planets / "assets" / "moon.jpg"));
    fs::remove(user_planets / "assets" / "moon.jpg");
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("assets/moon.jpg"),
                          base / "share" / "globtoo" / "assets" / "moon.jpg"));
#endif
    // An explicit asset-dir override still wins over per-user installations.
    fs::create_directories(base / "custom" / "imagery" / "mars");
    std::ofstream(base / "custom" / "imagery" / "mars" / "manifest.txt") << "override";
    setenv("GLOBE_ASSET_DIR", (base / "custom").c_str(), 1);
    assert(fs::equivalent(resource_path("imagery/mars/manifest.txt"),
                          base / "custom" / "imagery" / "mars" / "manifest.txt"));
    unsetenv("GLOBE_ASSET_DIR");
    // A lite install resolves explicitly downloaded textures under XDG.
    fs::create_directories(base / "cache" / "globtoo" / "assets");
    std::ofstream(base / "cache" / "globtoo" / "assets" / "earth.jpg") << "cached";
    setenv("XDG_CACHE_HOME", (base / "cache").c_str(), 1);
    assert(fs::equivalent(resource_cache_path("assets/earth.jpg"),
                          base / "cache" / "globtoo" / "assets" / "earth.jpg"));
    assert(resource_cache_path("../outside").empty());
    assert(resource_cache_path("assets/../earth.jpg").empty());
    // Packaged imagery must win over cached imagery in the fat package.
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("assets/earth.jpg"),
                          base / "share" / "globtoo" / "assets" / "earth.jpg"));
#endif
    fs::remove(base / "share" / "globtoo" / "assets" / "earth.jpg");
    assert(fs::equivalent(resource_path("assets/earth.jpg"),
                          base / "cache" / "globtoo" / "assets" / "earth.jpg"));
    unsetenv("XDG_CACHE_HOME");
    assert(resource_path("/tmp/absolute.txt") == "/tmp/absolute.txt");
    fs::create_directories(base / "custom" / "assets");
    std::ofstream(base / "custom" / "assets" / "earth.jpg") << "override";
    setenv("GLOBE_ASSET_DIR", (base / "custom").c_str(), 1);
    assert(fs::equivalent(resource_path("assets/earth.jpg"), base / "custom" / "assets" / "earth.jpg"));
    unsetenv("GLOBE_ASSET_DIR");
    fs::current_path(original_cwd);
    fs::remove_all(base);
}
