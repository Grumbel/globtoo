// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bodies.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Coordinates are north-positive/east-positive degrees in the viewer frame.
// Earth uses the approximate WGS84 sphere; Moon/Mars/Io markers refer to the
// selected body (and require source datum normalization for precision).
// All markers follow the same screen projection and globe occlusion.
struct Marker {
    double latitude = 0.0;
    double longitude = 0.0;
    std::string label;
    std::uint32_t color = 0xffc54b; // RGB, no alpha
    float radius = 7.f;             // screen pixels (independent of zoom)
};

enum class CommandAction { Run, Help, Version, DownloadAsset, ListFeatures };
struct CommandLine {
    CommandAction action = CommandAction::Run;
    std::string download_asset;
    std::string focus_name;
    std::string texture_path;
    BodyId body = BodyId::Earth;
    // Optional textures indexed by BodyId. The selected body's --texture
    // overrides this table, without assigning Earth imagery to other bodies.
    std::array<std::string,body_catalog.size()> body_textures{};
    std::string surface_layer = "imagery";
    std::string imagery_dir;
    std::array<std::string,body_catalog.size()> body_imagery_dirs{};
    std::string relief_dir;
    std::string elevation_dir;
    std::string population_dir;
    std::vector<Marker> markers;
    // Explicit initial viewpoint, [longitude, latitude]; otherwise timezone.
    std::optional<std::pair<double,double>> center;
};

enum class CoordinateOrder { LatitudeLongitude, LongitudeLatitude };

// Surface coordinates (altitude in geo: URIs is ignored). Accept decimal,
// hemisphere-suffixed/prefixed, DMS, decimal minutes, and ISO 6709 pairs.
// Coordinates may be separated by commas or whitespace. Every token is
// consumed and all angle components and ranges are checked.
bool parse_coordinates(std::string_view text, CoordinateOrder order,
                       double& latitude, double& longitude, std::string& error);

// Arguments are deliberately parsed before SDL_Init so --help, --version,
// and malformed input work on headless systems.
bool parse_command_line(int argc, const char* const* argv,
                        CommandLine& output, std::string& error);
std::string command_line_help(std::string_view program);
