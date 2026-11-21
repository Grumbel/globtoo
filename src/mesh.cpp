#include "mesh.hpp"
#include <cmath>
#include <cstdio>

Mesh create_sphere(int slices, int stacks) {
    std::vector<float> verts; // pos.xyz + uv.st
    std::vector<unsigned short> indices;

    for (int i = 0; i <= stacks; ++i) {
        float v = (float)i / stacks;
        float phi = v * 3.14159265f;
        float y = cosf(phi);
        float r = sinf(phi);

        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices;
            float theta = u * 2.f * 3.14159265f;
            float x = r * cosf(theta);
            float z = r * sinf(theta);

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
            indices.push_back((unsigned short)a);
            indices.push_back((unsigned short)b);
            indices.push_back((unsigned short)(a + 1));
            indices.push_back((unsigned short)(a + 1));
            indices.push_back((unsigned short)b);
            indices.push_back((unsigned short)(b + 1));
        }
    }

    Mesh m{};
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), indices.data(), GL_STATIC_DRAW);

    m.index_count = (GLsizei)indices.size();
    return m;
}

void destroy_mesh(Mesh& m) {
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.ibo) glDeleteBuffers(1, &m.ibo);
    m = {};
}

void draw_mesh(const Mesh& m) {
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glDrawElements(GL_TRIANGLES, m.index_count, GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}
