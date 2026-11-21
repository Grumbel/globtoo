#include "country_mesh_io.hpp"

#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr char magic[8] = {'G', 'L', 'O', 'B', 'G', 'M', '0', '1'};

void put32(std::ostream& file, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) file.put(static_cast<char>(value >> (8 * i)));
}

bool get32(std::istream& file, uint32_t& value) {
    value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const int byte = file.get();
        if (byte == EOF) return false;
        value |= static_cast<uint32_t>(byte) << (8 * i);
    }
    return true;
}

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

bool write_floats(std::ostream& file, const std::vector<float>& values) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    if constexpr (std::endian::native == std::endian::little) {
        file.write(reinterpret_cast<const char*>(values.data()),
                   static_cast<std::streamsize>(values.size() * sizeof(float)));
    } else {
        for (float v : values) put32(file, std::bit_cast<uint32_t>(v));
    }
    return file.good();
}

bool read_floats(std::istream& file, std::vector<float>& values) {
    if constexpr (std::endian::native == std::endian::little) {
        return static_cast<bool>(file.read(reinterpret_cast<char*>(values.data()),
                                           static_cast<std::streamsize>(values.size() * sizeof(float))));
    } else {
        for (auto& v : values) {
            uint32_t bits;
            if (!get32(file, bits)) return false;
            v = std::bit_cast<float>(bits);
        }
        return true;
    }
}
} // namespace

bool write_country_mesh(const std::string& path, const CountryGeometry& mesh,
                        std::string* error) {
    const auto fits = [](std::size_t n) {
        return n <= std::numeric_limits<uint32_t>::max();
    };
    if (!fits(mesh.border_xyz.size()) || !fits(mesh.fill_xyz_rgb.size()) ||
        !fits(mesh.border_strips.size()) || !fits(mesh.feature_count) ||
        !fits(mesh.polygon_count) || !fits(mesh.triangulation_failures) ||
        mesh.border_xyz.size() % 3 != 0 || mesh.fill_xyz_rgb.size() % 18 != 0)
        return fail(error, "invalid mesh sizes");
    for (auto s : mesh.border_strips) {
        if (s.first < 0 || s.count < 2 ||
            static_cast<std::size_t>(s.first) + static_cast<std::size_t>(s.count) >
                mesh.border_xyz.size() / 3)
            return fail(error, "invalid border strip");
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return fail(error, "cannot create mesh file");
    file.write(magic, sizeof(magic));
    put32(file, static_cast<uint32_t>(mesh.border_xyz.size()));
    put32(file, static_cast<uint32_t>(mesh.border_strips.size()));
    put32(file, static_cast<uint32_t>(mesh.fill_xyz_rgb.size()));
    put32(file, static_cast<uint32_t>(mesh.feature_count));
    put32(file, static_cast<uint32_t>(mesh.polygon_count));
    put32(file, static_cast<uint32_t>(mesh.triangulation_failures));
    if (!write_floats(file, mesh.border_xyz)) return fail(error, "failed writing borders");
    for (auto s : mesh.border_strips) {
        put32(file, static_cast<uint32_t>(s.first));
        put32(file, static_cast<uint32_t>(s.count));
    }
    if (!write_floats(file, mesh.fill_xyz_rgb)) return fail(error, "failed writing fills");
    return file.good() ? true : fail(error, "failed writing mesh");
}

bool read_country_mesh(const std::string& path, CountryGeometry& mesh,
                       std::string* error) {
    mesh = {};
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return fail(error, "cannot open mesh file");
    const auto bytes = file.tellg();
    if (bytes < static_cast<std::streamoff>(sizeof(magic) + 6 * 4))
        return fail(error, "truncated mesh header");
    file.seekg(0);
    char header[sizeof(magic)];
    if (!file.read(header, sizeof(header)) ||
        std::string_view(header, sizeof(header)) != std::string_view(magic, sizeof(magic)))
        return fail(error, "unsupported mesh format");
    uint32_t borders, strips, fills, features, polygons, failures;
    if (!get32(file, borders) || !get32(file, strips) || !get32(file, fills) ||
        !get32(file, features) || !get32(file, polygons) || !get32(file, failures))
        return fail(error, "truncated mesh header");
    const uint64_t expected = sizeof(magic) + 6 * 4 +
        static_cast<uint64_t>(borders) * 4 + static_cast<uint64_t>(strips) * 8 +
        static_cast<uint64_t>(fills) * 4;
    if (expected != static_cast<uint64_t>(bytes) || borders % 3 != 0 || fills % 18 != 0 ||
        // GLsizei and GLint must be able to address these vertices.
        borders / 3 > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        fills / 6 > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return fail(error, "invalid mesh length or counts");
    CountryGeometry parsed;
    parsed.border_xyz.resize(borders);
    parsed.border_strips.resize(strips);
    parsed.fill_xyz_rgb.resize(fills);
    parsed.feature_count = features;
    parsed.polygon_count = polygons;
    parsed.triangulation_failures = failures;
    if (!read_floats(file, parsed.border_xyz)) return fail(error, "truncated border data");
    for (auto& s : parsed.border_strips) {
        uint32_t first, count;
        if (!get32(file, first) || !get32(file, count) || count < 2 ||
            static_cast<uint64_t>(first) + count > borders / 3 ||
            first > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
            count > static_cast<uint32_t>(std::numeric_limits<int>::max()))
            return fail(error, "invalid border strip");
        s = {static_cast<int>(first), static_cast<int>(count)};
    }
    if (!read_floats(file, parsed.fill_xyz_rgb)) return fail(error, "truncated fill data");
    mesh = std::move(parsed);
    return true;
}
