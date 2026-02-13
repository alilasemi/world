#include <particle_drawer.h>

#include <cmath>

using std::size_t;


ParticleDrawer::ParticleDrawer(const int _n, const int _num_triangles): n(_n), num_triangles(_num_triangles) {
    node_coords.resize(static_cast<size_t>(n * (num_triangles + 1)) * 3);
    triangles.resize(static_cast<size_t>(num_triangles * n * 3));
    create_triangle_connectivity();
}

void ParticleDrawer::create_triangle_connectivity() {
    for (int i = 0; i < n; ++i) {
        int start_index = i * (num_triangles + 1);
        int center_index = start_index + num_triangles;
        for (size_t i_tri = 0; i_tri < static_cast<size_t>(num_triangles - 1); ++i_tri) {
            triangles[i * num_triangles * 3 + i_tri * 3 + 0] = start_index + i_tri;
            triangles[i * num_triangles * 3 + i_tri * 3 + 1] = start_index + i_tri + 1;
            triangles[i * num_triangles * 3 + i_tri * 3 + 2] = center_index;
        }
        triangles[i * num_triangles * 3 + (num_triangles - 1) * 3 + 0] = start_index + num_triangles - 1;
        triangles[i * num_triangles * 3 + (num_triangles - 1) * 3 + 1] = start_index;
        triangles[i * num_triangles * 3 + (num_triangles - 1) * 3 + 2] = center_index;
    }
}

void ParticleDrawer::draw(const std::vector<float>& x, const std::vector<float>& y) {
    for (int i = 0; i < n; ++i) {
        int start_index = i * (num_triangles + 1);
        for (size_t i_tri = 0; i_tri < static_cast<size_t>(num_triangles); ++i_tri) {
            float angle = static_cast<float>(i_tri) / static_cast<float>(num_triangles) * 2.0f * 3.14159265f;
            node_coords[(start_index + i_tri) * 3 + 0] = x[i] + cosf(angle) * radius;
            node_coords[(start_index + i_tri) * 3 + 1] = y[i] + sinf(angle) * radius;
            node_coords[(start_index + i_tri) * 3 + 2] = 0.0f;
        }
        node_coords[(start_index + num_triangles) * 3 + 0] = x[i];
        node_coords[(start_index + num_triangles) * 3 + 1] = y[i];
        node_coords[(start_index + num_triangles) * 3 + 2] = 0.0f;
    }
}
