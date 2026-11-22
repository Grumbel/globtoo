#include "mesh.hpp"
#include "geocoordinates.hpp"
#include <cmath>
#include <cstdio>
#include <array>

Mesh create_sphere(int slices, int stacks) {
    std::vector<float> verts; // pos.xyz + uv.st
    std::vector<unsigned short> indices;

    // Vertex positions and UVs must use the same geographic transform as
    // GeoJSON. Reversing longitude flips the triangle winding as well.
    for (int i = 0; i <= stacks; ++i) {
        const float v = (float)i / stacks;
        for (int j = 0; j <= slices; ++j) {
            const float u = (float)j / slices;
            const auto pos = geo::uv_to_xyz(u, v);
            const float x = static_cast<float>(pos[0]);
            const float y = static_cast<float>(pos[1]);
            const float z = static_cast<float>(pos[2]);

            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            verts.push_back(u);
            verts.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int a = i * (slices + 1) + j;
            int b = a + slices + 1;
            // CCW from outside
            indices.push_back((unsigned short)a);
            indices.push_back((unsigned short)b);
            indices.push_back((unsigned short)(a + 1));
            indices.push_back((unsigned short)(a + 1));
            indices.push_back((unsigned short)b);
            indices.push_back((unsigned short)(b + 1));
        }
    }

    // The old indexed vertices cannot hold a per-triangle anchor. Build a
    // separate projected VBO with one common reference for all three corners
    // so rotating the projection seam cannot stretch a triangle across 360°.
    std::vector<float> projected;
    projected.reserve(indices.size()*8);
    for (size_t i=0;i<indices.size();i+=3) {
        const auto at=[&](size_t j,size_t k){ return verts[indices[i+j]*5+k]; };
        std::array<float,3> anchor{};
        for (size_t j=0;j<3;j++)
            for (size_t k=0;k<3;k++) anchor[k]+=at(j,k);
        const float len=std::sqrt(anchor[0]*anchor[0]+anchor[1]*anchor[1]+anchor[2]*anchor[2]);
        if (len>1e-8f) for (float& v:anchor) v/=len;
        else for (size_t k=0;k<3;k++) anchor[k]=at(0,k);
        for (size_t j=0;j<3;j++) {
            for (size_t k=0;k<5;k++) projected.push_back(at(j,k));
            projected.insert(projected.end(),anchor.begin(),anchor.end());
        }
    }

    Mesh m{};
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short),
                 indices.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.projected_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,m.projected_vbo);
    glBufferData(GL_ARRAY_BUFFER,projected.size()*sizeof(float),projected.data(),GL_STATIC_DRAW);
    m.projected_count=static_cast<GLsizei>(projected.size()/8);
    m.index_count = (GLsizei)indices.size();
    return m;
}

void destroy_mesh(Mesh& m) {
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.ibo) glDeleteBuffers(1, &m.ibo);
    if (m.projected_vbo) glDeleteBuffers(1,&m.projected_vbo);
    m = {};
}

void draw_mesh(const Mesh& m, bool reprojected) {
    glBindBuffer(GL_ARRAY_BUFFER,reprojected ? m.projected_vbo : m.vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(3);
    if (reprojected) {
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
        glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(5*sizeof(float)));
        glDrawArrays(GL_TRIANGLES,0,m.projected_count);
    } else {
        // Globe mode has no moving seam: keep the much lighter indexed mesh.
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
        // When u_morph=0 the anchor is irrelevant; use the position as it.
        glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ibo);
        glDrawElements(GL_TRIANGLES,m.index_count,GL_UNSIGNED_SHORT,0);
    }
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(3);
}
