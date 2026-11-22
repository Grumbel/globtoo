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
    std::ofstream(base / "bin" / "shaders" / "test.vert") << "build";
    std::ofstream(base / "share" / "globtoo" / "shaders" / "installed.vert") << "installed";
    resources_set_executable_dir((base / "bin").string());
    assert(fs::equivalent(resource_path("shaders/test.vert"), base / "bin" / "shaders" / "test.vert"));
#ifdef GLOBTOO_INSTALL_DATADIR
    assert(fs::equivalent(resource_path("shaders/installed.vert"), base / "share" / "globtoo" / "shaders" / "installed.vert"));
#endif
    assert(resource_path("/tmp/absolute.txt") == "/tmp/absolute.txt");
    fs::create_directories(base / "custom" / "assets");
    std::ofstream(base / "custom" / "assets" / "earth.jpg") << "override";
    setenv("GLOBE_ASSET_DIR", (base / "custom").c_str(), 1);
    assert(fs::equivalent(resource_path("assets/earth.jpg"), base / "custom" / "assets" / "earth.jpg"));
    unsetenv("GLOBE_ASSET_DIR");
    fs::remove_all(base);
}
