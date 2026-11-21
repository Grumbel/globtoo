// Catch GLES2 shader link failures caused by precision mismatches between
// vertex and fragment declarations. In ESSL 1.00 vertex floats default to
// highp, while our fragments declare a mediump float default.
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <regex>
#include <string>

struct Declaration {
    std::string type;
    std::string precision;
};
using Declarations = std::map<std::string, Declaration>;

static std::string source(const std::string& filename) {
    std::ifstream file(filename);
    assert(file && "missing shader source");
    return std::string(std::istreambuf_iterator<char>(file), {});
}

static Declarations declarations(const std::string& source, bool vertex,
                                 const std::string& storage) {
    // This is an interface regression test, not a complete GLSL parser.
    // All current cross-stage declarations are float/vectors/matrices.
    static const std::regex pattern(
        R"(\b(uniform|varying)\s+(?:(lowp|mediump|highp)\s+)?(float|vec[234]|mat[234])\s+(\w+)\s*;)"
    );
    const std::string default_precision = vertex ? "highp" : "mediump";
    Declarations result;
    for (auto it = std::sregex_iterator(source.begin(), source.end(), pattern);
         it != std::sregex_iterator(); ++it) {
        const auto& match = *it;
        if (match[1] == storage) {
            result[match[4]] = {match[3], match[2].matched ? match[2].str() : default_precision};
        }
    }
    return result;
}

static bool check_pair(const std::string& dir, const std::string& name) {
    std::string vert = source(dir + "/" + name + ".vert");
    const std::string marker = "#include \"projection.glsl\"";
    if (const auto at = vert.find(marker); at != std::string::npos)
        vert.replace(at, marker.size(), source(dir + "/projection.glsl"));
    const std::string frag = source(dir + "/" + name + ".frag");
    bool ok = true;
    for (const auto* storage : {"uniform", "varying"}) {
        const auto in_vertex = declarations(vert, true, storage);
        const auto in_fragment = declarations(frag, false, storage);
        for (const auto& [symbol, f] : in_fragment) {
            // A uniform used only in a fragment stage has no VS counterpart.
            auto v = in_vertex.find(symbol);
            if (v == in_vertex.end()) {
                if (std::string(storage) == "varying") {
                    std::fprintf(stderr, "%s: fragment varying %s missing in vertex\n",
                                 name.c_str(), symbol.c_str());
                    ok = false;
                }
                continue;
            }
            if (v->second.type != f.type || v->second.precision != f.precision) {
                std::fprintf(stderr, "%s: %s %s has vertex %s %s, fragment %s %s\n",
                             name.c_str(), storage, symbol.c_str(),
                             v->second.precision.c_str(), v->second.type.c_str(),
                             f.precision.c_str(), f.type.c_str());
                ok = false;
            }
        }
    }
    return ok;
}

int main() {
    const std::string dir = GLOBTOO_SHADER_SOURCE_DIR;
    bool ok = true;
    for (const auto* name : {"globe", "fill", "borders", "stars", "ui"})
        ok = check_pair(dir, name) && ok;
    if (ok) std::puts("GLES2 shader precision interfaces match across all programs");
    return ok ? 0 : 1;
}
