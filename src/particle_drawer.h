#pragma once

#include <defines.h>
#include <vector>

class ParticleDrawer {
public:
    ParticleDrawer(const int _n, const int _num_triangles);
    std::vector<float> node_coords;
    std::vector<int> triangles;

    void draw(const std::vector<float>& x, const std::vector<float>& y);

private:
    const int n;
    const int num_triangles;
    const float radius = 0.05f;

    void create_triangle_connectivity();
};
