// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "resources.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

namespace {
namespace fs = std::filesystem;
std::string executable_dir;

bool regular_file(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

std::string path_string(const fs::path& path) {
    return path.lexically_normal().string();
}
}

std::string resource_user_data_path(std::string_view relative_path) {
    // Do not translate arbitrary paths into the user's data directory. The
    // installer creates a fixed imagery/<body>/manifest.txt layout.
    bool approved=false;
    for (const char* name : {"moon", "mars", "io"}) {
        const std::string body(name);
        if (relative_path == "assets/" + body + ".jpg" ||
            relative_path == "imagery/" + body + "/manifest.txt" ||
            relative_path == "imagery/" + body + "/preview-small.jpg") {
            approved=true;
            break;
        }
    }
    if (!approved) return {};
    fs::path root;
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) {
        if (*xdg && fs::path(xdg).is_absolute()) root = xdg;
    }
    if (root.empty()) {
        const char* home = std::getenv("HOME");
        if (!home || !*home || !fs::path(home).is_absolute()) return {};
        root = fs::path(home) / ".local" / "share";
    }
    return path_string(root / "globtoo" / relative_path);
}

std::string resource_cache_path(std::string_view relative_path) {
    // Only fixed, catalogued resources may be resolved through this cache.
    // This prevents an arbitrary user-supplied traversal path from being
    // expanded beneath the user's cache directory.
    if (relative_path != "assets/earth.jpg") return {};
    fs::path root;
    if (const char* xdg = std::getenv("XDG_CACHE_HOME")) {
        if (*xdg && fs::path(xdg).is_absolute()) root = xdg;
    }
    if (root.empty()) {
        const char* home = std::getenv("HOME");
        if (!home || !*home || !fs::path(home).is_absolute()) return {};
        root = fs::path(home) / ".cache";
    }
    return path_string(root / "globtoo" / relative_path);
}

void resources_set_executable_dir(std::string directory) {
    executable_dir = std::move(directory);
}

std::string resource_path(std::string_view relative_path) {
    const fs::path relative(relative_path);
    if (relative.is_absolute()) return path_string(relative);

    // Explicit asset directory: useful with portable installs and custom maps.
    if (const char* override_dir = std::getenv("GLOBE_ASSET_DIR")) {
        if (*override_dir) return path_string(fs::path(override_dir) / relative);
    }

    // User-installed maps can override the package's default imagery without
    // attempting to modify the read-only Nix store. The map installer checks
    // manifest integrity before publishing the entire directory.
    const std::string user_data = resource_user_data_path(relative_path);
    if (!user_data.empty() && regular_file(user_data)) return user_data;

    // An audited map installer publishes imagery/<body>/preview-small.jpg,
    // not assets/<body>.jpg. Prefer that preview to any packaged image, so
    // the globe and its detail tiles always come from the SAME map. Limit
    // this special case to the three supported bodies, never arbitrary paths.
    for (const char* name : {"moon", "mars", "io"}) {
        if (relative_path == std::string("assets/") + name + ".jpg") {
            const std::string manifest = resource_user_data_path(
                std::string("imagery/") + name + "/manifest.txt");
            const std::string preview = resource_user_data_path(
                std::string("imagery/") + name + "/preview-small.jpg");
            if (!manifest.empty() && regular_file(manifest) && regular_file(preview))
                return preview;
            break;
        }
    }

    if (!executable_dir.empty()) {
        const fs::path bin = fs::path(executable_dir);
        // Build-tree layout: POST_BUILD copies shaders/assets beside the app.
        if (regular_file(bin / relative)) return path_string(bin / relative);

        // Installed layout. Resolve relative to executable, not process cwd;
        // this also allows the installation prefix to be moved later.
#ifdef GLOBTOO_INSTALL_DATADIR
        const fs::path installed = bin / ".." / GLOBTOO_INSTALL_DATADIR / "globtoo" / relative;
        if (regular_file(installed)) return path_string(installed);
#endif
    }

    // The configured path also handles non-standard absolute install bindirs.
#ifdef GLOBTOO_INSTALL_RESOURCE_DIR
    const fs::path configured = fs::path(GLOBTOO_INSTALL_RESOURCE_DIR) / relative;
    if (regular_file(configured)) return path_string(configured);
#endif

    // Fat/offline installations take precedence over downloaded assets. In a
    // lite installation a verified, explicitly downloaded map is available
    // on subsequent launches without a network request.
    const std::string cached = resource_cache_path(relative_path);
    if (!cached.empty() && regular_file(cached)) return cached;

    // Development fallback only. Never allow a stale source-tree asset to
    // shadow a real packaged texture, even with a nonstandard bindir.
    if (regular_file(relative)) return path_string(relative);

    return path_string(relative); // Diagnostic from caller shows missing file.
}
