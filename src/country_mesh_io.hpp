// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "country_geometry.hpp"

#include <string>

// Binary, pretriangulated country meshes; generated at build time. Runtime
// loading doesn't require GEOS or GeoJSON parsing. Format is versioned and
// little-endian, with 32-bit counts and IEEE-754 float32 coordinates.
bool write_country_mesh(const std::string& path, const CountryGeometry& mesh,
                        std::string* error = nullptr);
bool read_country_mesh(const std::string& path, CountryGeometry& mesh,
                       std::string* error = nullptr);
