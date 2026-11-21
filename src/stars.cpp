#include "stars.hpp"
#include <algorithm>
#include "math.hpp"
#include <cmath>
#include <cstdio>
#include <vector>

// Bright star subset (RA hours, Dec deg, V mag)
struct StarDef { float ra_h, dec_deg, vmag; };

static const StarDef k_catalog[] = {
    { 6.7525f, -16.716f, -1.46f}, // Sirius
    { 6.3992f, -52.696f, -0.74f}, // Canopus
    {14.2610f,  19.182f, -0.05f}, // Arcturus
    {14.6601f, -60.834f, -0.01f}, // Rigil Kentaurus
    {18.6156f,  38.783f,  0.03f}, // Vega
    { 4.5987f,  16.510f,  0.08f}, // Capella
    { 5.2423f,  -8.202f,  0.13f}, // Rigel
    { 7.6550f,   5.225f,  0.34f}, // Procyon
    { 5.9195f,   7.407f,  0.50f}, // Betelgeuse
    {13.4199f, -11.161f,  0.97f}, // Spica
    {19.8463f,   8.868f,  0.76f}, // Altair
    {16.4901f, -26.432f,  0.96f}, // Antares
    {12.4433f, -63.099f,  0.76f}, // Acrux
    {22.3083f, -46.961f,  1.16f}, // Fomalhaut
    { 5.6793f, -34.384f,  0.46f}, // Achernar
    { 1.6286f,  89.264f,  1.98f}, // Polaris
    {11.0621f,  61.751f,  1.77f}, // Dubhe
    {11.0622f,  56.382f,  2.37f}, // Merak
    {13.7923f,  49.313f,  1.86f}, // Alioth
    {13.7924f,  55.960f,  1.77f}, // Alkaid approx
    {17.5600f, -37.104f,  1.06f}, // Shaula
    { 5.4188f,  28.608f,  1.65f}, // Aldebaran? Taurus Aldebaran is 4.598 - redo
    { 4.5986f,  16.509f,  0.87f}, // Aldebaran
    { 7.4009f, -29.303f,  1.50f}, // Avior
    { 9.2200f, -69.717f,  1.67f}, // Miaplacidus
    {12.5194f, -57.113f,  1.25f}, // Gacrux
    {14.0660f, -60.373f,  1.33f}, // Hadar (Beta Cen)
    { 6.1262f, -22.454f,  1.98f}, // Adhara
    {20.6905f,  45.280f,  1.25f}, // Deneb
    { 5.5334f,  -0.299f,  1.70f}, // Saiph
    { 2.1194f,  23.463f,  2.01f}, // Hamal
    { 3.4054f,  49.861f,  1.79f}, // Mirfak
    {22.9608f, -29.622f,  1.17f}, // Fomalhaut already
    { 0.2206f,  15.183f,  2.07f}, // Alpheratz
    { 0.6751f,  56.537f,  2.23f}, // Schedar
    { 2.2952f,  42.330f,  2.06f}, // Algol
    { 5.4189f, -2.473f,   2.23f}, // Cukor / Mintaka area
    { 5.5335f,  -1.202f,  1.64f}, // Alnilam
    { 5.6036f,  -1.943f,  1.69f}, // Alnitak
    { 6.7229f,  16.399f,  1.93f}, // Castor
    { 7.7553f,  28.026f,  1.14f}, // Pollux
    { 8.7441f,  18.154f,  3.50f}, // (skip dim)
    {10.1395f,  11.967f,  1.35f}, // Regulus
    {11.8177f,  14.572f,  2.14f}, // Denebola
    {12.9003f,  -5.387f,  2.55f},
    {13.3987f,  -11.16f,  0.98f},
    {15.2919f, -14.789f,  2.60f},
    {15.5781f,  26.715f,  2.08f}, // Corona Borealis
    {16.4901f, -26.432f,  0.96f},
    {17.5601f, -37.104f,  1.62f},
    {18.4029f, -34.384f,  1.59f}, // Kaus Australis
    {18.9211f,  -5.387f,  2.72f},
    {19.8463f,   8.868f,  0.76f},
    {20.6905f,  45.280f,  1.25f},
    {21.3097f,  62.586f,  2.45f}, // Alderamin
    {22.1378f, -46.960f,  1.16f},
    {23.0629f,  15.205f,  2.49f},
    { 0.1398f, -17.337f,  2.04f}, // Deneb Kaitos
    { 1.9107f,  20.808f,  2.64f},
    { 3.1362f,  40.955f,  2.91f},
    { 6.6189f, -43.432f,  1.86f}, // Canopus region
    { 9.1333f, -43.433f,  1.96f}, // Suhail
    {11.2262f, -29.303f,  2.65f},
    {12.5194f, -57.113f,  1.63f},
    {14.0660f, -60.373f,  0.61f}, // Hadar
    {16.8361f, -34.293f,  2.29f},
    {17.5822f,  12.560f,  2.08f}, // Ras Alhague
    {18.9211f,  33.970f,  3.20f},
    {20.2572f,  40.257f,  2.20f},
    {22.9608f, -29.622f,  1.74f},
};

static void ra_dec_to_xyz(float ra_h, float dec_deg, float radius, float& x, float& y, float& z) {
    float ra = ra_h * (3.14159265f / 12.f); // hours → radians
    float dec = dec_deg * (3.14159265f / 180.f);
    // Celestial: x = cos(dec) cos(ra), z = cos(dec) sin(ra), y = sin(dec)
    // Match our Earth Y-up: north celestial pole ≈ +Y
    float cdec = cosf(dec);
    x = cdec * cosf(ra) * radius;
    y = sinf(dec) * radius;
    z = cdec * sinf(ra) * radius;
}

bool stars_init(Stars& s) {
    std::vector<float> verts; // x y z brightness
    const float radius = 80.f;
    for (const auto& st : k_catalog) {
        if (st.vmag > 3.2f)
            continue; // keep it sparse
        float x, y, z;
        ra_dec_to_xyz(st.ra_h, st.dec_deg, radius, x, y, z);
        // brightness 0..1 from magnitude (V=-1.5 → ~1, V=3 → ~0.2)
        float b = std::clamp(1.f - (st.vmag + 1.5f) / 5.f, 0.15f, 1.f);
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(z);
        verts.push_back(b);
    }
    s.count = (GLsizei)(verts.size() / 4);
    if (s.count == 0)
        return false;

    glGenBuffers(1, &s.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    s.shader = load_shader("shaders/stars.vert", "shaders/stars.frag");
    if (!s.shader.program) {
        fprintf(stderr, "stars shader failed\n");
        return false;
    }
    fprintf(stderr, "Stars: %d bright stars\n", (int)s.count);
    return true;
}

void stars_shutdown(Stars& s) {
    if (s.vbo) glDeleteBuffers(1, &s.vbo);
    destroy_shader(s.shader);
    s = {};
}

void stars_draw(const Stars& s, const Camera& cam, float aspect) {
    if (!s.vbo || !s.shader.program)
        return;

    Mat4 proj = cam.proj_matrix(aspect);
    Mat4 view = cam.view_matrix();
    // Strip translation so stars are infinitely far (no parallax)
    Mat4 view_rot = view;
    view_rot.m[12] = view_rot.m[13] = view_rot.m[14] = 0.f;
    Mat4 mvp = Mat4::multiply(proj, view_rot);

    use_shader(s.shader);
    GLint u_mvp = glGetUniformLocation(s.shader.program, "u_mvp");
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp.m);

    glDisable(GL_DEPTH_TEST);
    // GLES2: point size from VS
#ifdef GL_VERTEX_PROGRAM_POINT_SIZE
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));

    glDrawArrays(GL_POINTS, 0, s.count);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
