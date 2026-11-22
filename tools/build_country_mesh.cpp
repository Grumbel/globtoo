// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "country_geometry.hpp"
#include "country_mesh_io.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: country_mesh_compiler INPUT.geojson OUTPUT.gmesh\n");
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) { fprintf(stderr, "Cannot read %s\n", argv[1]); return 1; }
    std::ostringstream text;
    text << file.rdbuf();
    CountryGeometry mesh;
    std::string error;
    if (!build_country_geometry(text.str(), mesh, &error) || mesh.triangulation_failures) {
        fprintf(stderr, "Country geometry failure: %s (%zu triangulation failures)\n",
                error.c_str(), mesh.triangulation_failures);
        return 1;
    }
    if (!write_country_mesh(argv[2], mesh, &error)) {
        fprintf(stderr, "Cannot write %s: %s\n", argv[2], error.c_str());
        return 1;
    }
    fprintf(stderr, "%s: %zu features, %zu polygons, %zu rings, %zu fill vertices\n",
            argv[2], mesh.feature_count, mesh.polygon_count,
            mesh.border_strips.size(), mesh.fill_xyz_rgb.size() / 6);
}
