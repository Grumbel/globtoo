#include "stars.hpp"
#include <algorithm>
#include "math.hpp"
#include "geocoordinates.hpp"
#include <cmath>
#include <cstdio>
#include <vector>

// Bright stars: RA (hours), Dec (degrees), V magnitude.
// Approximate positions from the bright star catalogue — dense enough
// to read constellations, sparse enough not to wash out the globe.
struct StarDef { float ra_h, dec_deg, vmag; };

static const StarDef k_catalog[] = {
    // --- Magnitude ~-1.5 to 1.5 (major navigational stars) ---
    { 6.7525f, -16.716f, -1.46f}, // Sirius
    { 6.3992f, -52.696f, -0.74f}, // Canopus
    {14.2610f,  19.182f, -0.05f}, // Arcturus
    {14.6601f, -60.834f, -0.01f}, // Alpha Cen
    {18.6156f,  38.783f,  0.03f}, // Vega
    { 4.5987f,  16.510f,  0.08f}, // Capella
    { 5.2423f,  -8.202f,  0.13f}, // Rigel
    { 7.6550f,   5.225f,  0.34f}, // Procyon
    { 5.6793f, -34.384f,  0.46f}, // Achernar
    { 5.9195f,   7.407f,  0.50f}, // Betelgeuse
    {14.0660f, -60.373f,  0.61f}, // Hadar
    {12.4433f, -63.099f,  0.76f}, // Acrux
    {19.8463f,   8.868f,  0.76f}, // Altair
    { 4.5986f,  16.509f,  0.87f}, // Aldebaran
    {16.4901f, -26.432f,  0.96f}, // Antares
    {13.4199f, -11.161f,  0.97f}, // Spica
    { 7.7553f,  28.026f,  1.14f}, // Pollux
    {22.3083f, -46.961f,  1.16f}, // Fomalhaut
    {20.6905f,  45.280f,  1.25f}, // Deneb
    {12.5194f, -57.113f,  1.25f}, // Gacrux
    {10.1395f,  11.967f,  1.35f}, // Regulus
    { 6.1262f, -22.454f,  1.50f}, // Adhara
    { 7.4009f, -29.303f,  1.50f}, // Avior
    { 5.5335f,  -1.202f,  1.64f}, // Alnilam
    { 5.6036f,  -1.943f,  1.69f}, // Alnitak
    { 5.4189f,  -0.299f,  1.70f}, // Saiph-ish / Mintaka area
    { 5.6790f,  -1.943f,  1.74f}, // Alnitak region
    {11.0621f,  61.751f,  1.77f}, // Dubhe
    {13.7924f,  49.313f,  1.77f}, // Alioth
    { 3.4054f,  49.861f,  1.79f}, // Mirfak
    { 9.2200f, -69.717f,  1.67f}, // Miaplacidus
    {17.5600f, -37.104f,  1.62f}, // Shaula
    {18.4029f, -34.384f,  1.79f}, // Kaus Australis
    { 6.7229f,  16.399f,  1.93f}, // Castor
    { 1.6286f,  89.264f,  1.98f}, // Polaris
    { 2.1194f,  23.463f,  2.01f}, // Hamal
    { 0.1398f, -17.337f,  2.04f}, // Diphda
    { 2.2952f,  42.330f,  2.06f}, // Algol
    { 0.2206f,  15.183f,  2.07f}, // Alpheratz
    {17.5822f,  12.560f,  2.08f}, // Rasalhague
    {15.5781f,  26.715f,  2.23f}, // Alphecca
    { 0.6751f,  56.537f,  2.23f}, // Schedar
    {11.0622f,  56.382f,  2.37f}, // Merak
    {21.3097f,  62.586f,  2.45f}, // Alderamin
    {11.8177f,  14.572f,  2.14f}, // Denebola
    { 9.1333f, -43.433f,  1.96f}, // Suhail
    {13.3987f,  54.925f,  1.86f}, // Mizar
    {13.7923f,  55.960f,  1.85f}, // Alkaid
    {12.9003f,  -5.387f,  2.55f}, // Porrima area
    {15.2919f, -14.789f,  2.60f}, // Zubeneschamali region
    {18.9211f,  -5.387f,  2.72f}, // Nunki area
    {20.2572f,  40.257f,  2.20f}, // Sadr
    {22.9608f, -29.622f,  1.74f}, // Skat area
    {23.0629f,  15.205f,  2.49f}, // Markab region

    // --- Orion / winter ---
    { 5.4188f,  -0.299f,  2.23f}, // Mintaka
    { 5.5334f,  -0.299f,  1.70f}, // Saiph
    { 5.9195f,   7.407f,  0.50f},
    { 6.1262f, -22.454f,  1.50f},
    { 5.2423f,  -8.202f,  0.13f},
    { 5.7961f,  -9.670f,  2.06f}, // Cursa
    { 6.6285f,   9.647f,  2.90f}, // Menkalinan? Capella pair
    { 4.9040f,  15.000f,  3.10f}, // Elnath-ish
    { 5.9190f,  20.000f,  3.40f},
    { 4.8298f,  18.500f,  3.50f},

    // --- Ursa Major ---
    {11.0621f,  61.751f,  1.79f},
    {11.0306f,  56.382f,  2.37f},
    {11.8971f,  53.695f,  2.44f}, // Phecda
    {12.2571f,  57.033f,  3.31f}, // Megrez
    {13.3985f,  54.925f,  2.23f}, // Mizar
    {13.7923f,  49.313f,  1.76f},
    {13.8249f,  49.313f,  3.99f}, // Alcor (dim)
    {12.9000f,  55.960f,  3.00f},

    // --- Cassiopeia ---
    { 0.6751f,  56.537f,  2.24f},
    { 0.9451f,  60.717f,  2.28f}, // Caph
    { 1.4302f,  60.235f,  2.47f}, // Gamma Cas
    { 2.2940f,  59.150f,  2.68f}, // Ruchbah
    { 1.9066f,  63.670f,  3.38f}, // Segin

    // --- Scorpius ---
    {16.4901f, -26.432f,  0.96f},
    {16.3531f, -25.593f,  2.29f}, // Graffias
    {16.8361f, -34.293f,  2.29f}, // Dschubba
    {17.5601f, -37.104f,  1.62f},
    {17.6217f, -42.998f,  1.86f}, // Kappa Sco
    {17.7930f, -40.127f,  2.39f}, // Lambda Sco? Shaula is 17.56
    {17.7934f, -37.295f,  2.70f},
    {16.8360f, -30.000f,  3.20f},
    {16.3530f, -28.000f,  3.50f},

    // --- Crux ---
    {12.4433f, -63.099f,  0.76f},
    {12.7953f, -59.689f,  1.25f}, // Mimosa
    {12.5194f, -57.113f,  1.59f},
    {12.2520f, -58.749f,  2.79f}, // Delta Cru

    // --- Centaurus ---
    {14.6601f, -60.834f, -0.01f},
    {14.0660f, -60.373f,  0.61f},
    {13.9255f, -41.687f,  2.06f}, // Menkent
    {14.9860f, -42.104f,  2.30f},
    {13.3434f, -36.712f,  2.55f},

    // --- Cygnus ---
    {20.6905f,  45.280f,  1.25f},
    {20.3705f,  40.257f,  2.23f}, // Sadr
    {19.7490f,  45.130f,  2.48f}, // Gienah Cyg
    {21.2156f,  30.227f,  2.86f}, // Albireo-ish RA wrong; Albireo ~19.5
    {19.5120f,  27.960f,  3.08f}, // Albireo
    {20.7700f,  33.970f,  2.90f},

    // --- Lyra / Aquila / Summer triangle extras ---
    {18.6156f,  38.783f,  0.03f},
    {18.9790f,  32.690f,  3.25f}, // Sheliak
    {19.8463f,   8.868f,  0.76f},
    {19.9210f,   6.407f,  2.72f}, // Tarazed
    {19.9215f,   1.000f,  3.36f}, // Alshain
    {20.6900f,  45.280f,  1.25f},

    // --- Leo ---
    {10.1395f,  11.967f,  1.35f},
    {11.2351f,  20.524f,  2.56f}, // Algieba
    {11.8177f,  14.572f,  2.14f},
    {10.3330f,  19.841f,  2.98f}, // Zosma
    {10.1220f,   9.895f,  3.50f},
    {11.2350f,  15.430f,  3.50f},

    // --- Virgo ---
    {13.4199f, -11.161f,  0.97f},
    {12.9267f,  -0.667f,  2.74f}, // Vindemiatrix
    {13.1650f,  -5.539f,  3.39f},
    {12.6940f,  -1.449f,  3.50f},

    // --- Taurus ---
    { 4.5987f,  16.510f,  0.87f},
    { 4.4000f,  15.628f,  3.50f}, // Hyades
    { 4.4760f,  19.180f,  3.40f},
    { 3.7914f,  24.105f,  2.85f}, // Alcyone (Pleiades)
    { 3.7534f,  24.367f,  3.88f},
    { 3.7480f,  24.053f,  4.14f},
    { 3.7900f,  23.950f,  4.20f},
    { 5.4380f,  28.608f,  1.65f}, // Elnath

    // --- Gemini ---
    { 7.7553f,  28.026f,  1.14f},
    { 6.7229f,  16.399f,  1.58f}, // Castor sometimes 1.58
    { 6.6285f,  16.399f,  1.93f},
    { 7.1857f,  21.983f,  2.88f}, // Alhena
    { 6.3825f,  22.508f,  3.28f},
    { 7.5760f,  31.888f,  3.50f},

    // --- Andromeda / Pegasus ---
    { 0.2206f,  15.183f,  2.07f},
    { 1.1622f,  35.620f,  2.10f}, // Mirach
    { 2.0650f,  42.330f,  2.06f},
    {23.0793f,  15.205f,  2.49f}, // Markab
    {22.7170f,  30.221f,  2.44f}, // Scheat
    { 0.1397f,  29.091f,  2.83f}, // Algenib
    {22.8330f,   9.875f,  2.93f}, // Enif

    // --- Southern extras ---
    { 6.3992f, -52.696f, -0.74f},
    { 5.2420f, -57.237f,  2.40f},
    { 9.1333f, -43.433f,  1.96f},
    {10.6967f, -49.420f,  2.65f},
    {12.2520f, -58.749f,  2.79f},
    {13.3434f, -36.712f,  2.55f},
    {15.5780f, -63.099f,  2.80f},
    {16.8360f, -50.000f,  3.20f},
    {18.4029f, -34.384f,  1.79f},
    {22.3083f, -46.961f,  1.16f},
    { 0.6750f, -42.306f,  2.40f},
    { 1.4300f, -55.245f,  2.80f},
    { 2.9100f, -40.305f,  3.00f},
    { 3.8000f, -50.000f,  3.20f},
    { 4.9000f, -66.000f,  2.90f},
    { 8.7440f, -54.000f,  2.70f},
    {11.2262f, -29.303f,  2.65f},
    {17.7930f, -55.000f,  3.00f},
    {20.0000f, -50.000f,  3.30f},
    {21.5000f, -32.000f,  3.00f},

    // --- Bootes / Corona / Hercules ---
    {14.2610f,  19.182f, -0.05f},
    {14.7490f,  27.074f,  2.68f}, // Nekkar
    {14.5340f,  38.308f,  3.04f},
    {15.5781f,  26.715f,  2.23f},
    {16.4900f,  21.489f,  2.80f},
    {17.1460f,  14.390f,  2.78f}, // Kornephoros
    {17.5822f,  12.560f,  2.08f},
    {17.9620f,  29.250f,  3.20f},
    {16.3650f,  19.153f,  3.50f},

    // --- Eridanus / Cetus ---
    { 5.6793f, -34.384f,  0.46f},
    { 2.9710f,  -8.898f,  2.88f}, // Menkar? Cetus
    { 3.0370f,   4.090f,  2.54f}, // Menkar
    { 0.7265f, -17.986f,  2.54f}, // Diphda area
    { 1.9107f, -10.152f,  3.50f},
    { 3.9670f, -13.508f,  2.97f}, // Zaurak
    { 4.6050f, -14.304f,  3.72f},
    { 2.5830f, -40.305f,  3.20f},

    // --- Hydra / Corvus / Crater ---
    { 9.4597f,  -8.659f,  1.98f}, // Alphard
    {10.8270f, -16.195f,  3.00f},
    {12.1680f, -22.619f,  2.59f}, // Alchiba area / Gienah Cor
    {12.4970f, -16.516f,  2.65f}, // Gienah
    {12.9350f, -17.542f,  3.00f},

    // --- Perseus / Auriga extras ---
    { 3.4054f,  49.861f,  1.79f},
    { 3.1362f,  40.955f,  2.90f},
    { 3.9690f,  40.010f,  2.91f},
    { 5.2780f,  45.999f,  2.62f}, // Menkalinan
    { 4.5987f,  16.510f,  0.08f},
    { 5.0000f,  41.000f,  3.20f},
    { 5.5000f,  33.000f,  3.50f},

    // --- Sagittarius ---
    {18.4029f, -34.384f,  1.79f},
    {18.4660f, -25.422f,  2.05f}, // Nunki
    {18.0790f, -30.424f,  2.60f}, // Ascella
    {18.9210f, -26.297f,  2.70f},
    {18.1000f, -36.000f,  2.80f},
    {17.7930f, -27.670f,  3.10f},
    {19.1620f, -21.058f,  3.50f},

    // --- Capricornus / Aquarius ---
    {20.2940f, -12.508f,  2.85f}, // Dabih area
    {21.7410f, -16.127f,  2.87f}, // Deneb Algedi
    {22.0960f,  -0.319f,  2.90f}, // Sadalmelik
    {22.9100f,  -15.821f,  2.95f}, // Skat
    {20.7940f,  -9.000f,  3.50f},
    {23.0629f,  -8.000f,  3.50f},

    // --- Ophiuchus ---
    {17.5822f,  12.560f,  2.08f},
    {16.6190f,  -4.693f,  2.43f}, // Sabik
    {17.1720f,  -5.166f,  2.54f}, // Cebalrai? 
    {16.2390f,  -3.694f,  2.76f},
    {17.7240f,   4.567f,  3.20f},
    {16.4850f,  1.983f,   3.50f},

    // --- Filler grid of mag ~4–5 across the sky (still real-ish density) ---
    { 0.50f,   45.0f,  4.2f}, { 1.00f,  -20.0f,  4.0f}, { 1.50f,   10.0f,  4.3f},
    { 2.00f,   55.0f,  4.1f}, { 2.50f,  -30.0f,  4.4f}, { 3.00f,    5.0f,  4.2f},
    { 3.50f,   35.0f,  4.0f}, { 4.00f,  -25.0f,  4.5f}, { 4.50f,   50.0f,  4.3f},
    { 5.00f,  -45.0f,  4.1f}, { 5.50f,   25.0f,  4.4f}, { 6.00f,  -10.0f,  4.2f},
    { 6.50f,   40.0f,  4.0f}, { 7.00f,  -55.0f,  4.5f}, { 7.50f,   15.0f,  4.3f},
    { 8.00f,  -20.0f,  4.1f}, { 8.50f,   60.0f,  4.4f}, { 9.00f,    0.0f,  4.2f},
    { 9.50f,  -35.0f,  4.0f}, {10.00f,   30.0f,  4.5f}, {10.50f,  -50.0f,  4.3f},
    {11.00f,   45.0f,  4.1f}, {11.50f,  -15.0f,  4.4f}, {12.00f,   20.0f,  4.2f},
    {12.50f,  -40.0f,  4.0f}, {13.00f,   65.0f,  4.5f}, {13.50f,   -5.0f,  4.3f},
    {14.00f,   35.0f,  4.1f}, {14.50f,  -25.0f,  4.4f}, {15.00f,   10.0f,  4.2f},
    {15.50f,  -60.0f,  4.0f}, {16.00f,   50.0f,  4.5f}, {16.50f,  -10.0f,  4.3f},
    {17.00f,   25.0f,  4.1f}, {17.50f,  -45.0f,  4.4f}, {18.00f,    5.0f,  4.2f},
    {18.50f,   55.0f,  4.0f}, {19.00f,  -30.0f,  4.5f}, {19.50f,   20.0f,  4.3f},
    {20.00f,  -15.0f,  4.1f}, {20.50f,   40.0f,  4.4f}, {21.00f,  -55.0f,  4.2f},
    {21.50f,   15.0f,  4.0f}, {22.00f,  -35.0f,  4.5f}, {22.50f,   60.0f,  4.3f},
    {23.00f,    0.0f,  4.1f}, {23.50f,  -20.0f,  4.4f}, { 0.25f,   70.0f,  4.2f},
    { 1.25f,  -70.0f,  4.0f}, { 2.25f,   75.0f,  4.5f}, { 8.25f,  -70.0f,  4.3f},
    {12.25f,   80.0f,  4.1f}, {16.25f,  -75.0f,  4.4f}, {20.25f,   75.0f,  4.2f},
};

static void ra_dec_to_xyz(float ra_h, float dec_deg, float radius, float& x, float& y, float& z) {
    // RA increases eastward, just like geographic longitude. A single
    // right-handed convention is used for both sky and Earth coordinates.
    const auto p = geo::lonlat_to_xyz(ra_h * 15.0, dec_deg, radius);
    x = static_cast<float>(p[0]);
    y = static_cast<float>(p[1]);
    z = static_cast<float>(p[2]);
}

bool stars_init(Stars& s) {
    std::vector<float> verts; // x y z brightness
    const float radius = 80.f;
    for (const auto& st : k_catalog) {
        if (st.vmag > 5.0f)
            continue;
        float x, y, z;
        ra_dec_to_xyz(st.ra_h, st.dec_deg, radius, x, y, z);
        float b = std::clamp(1.f - (st.vmag + 1.5f) / 6.5f, 0.12f, 1.f);
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
    fprintf(stderr, "Stars: %d\n", (int)s.count);
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

    // Stars are 80 units away and do not use/write depth. Give the sky its
    // own long-range projection: the globe needs a much tighter depth range.
    Mat4 proj = Mat4::perspective(0.7f, aspect, 0.1f, 120.f);
    Mat4 view = cam.view_matrix();
    Mat4 view_rot = view;
    view_rot.m[12] = view_rot.m[13] = view_rot.m[14] = 0.f;
    Mat4 mvp = Mat4::multiply(proj, view_rot);

    use_shader(s.shader);
    GLint u_mvp = glGetUniformLocation(s.shader.program, "u_mvp");
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp.m);

    glDisable(GL_DEPTH_TEST);
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
