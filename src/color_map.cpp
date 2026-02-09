#include "color_map.h"

#include <stdexcept>
#include <cmath>


ColorMap::ColorMap(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "r");
    if (file == nullptr) {
        throw std::runtime_error("Could not open color map file");
    }
    float r, g, b;
    while (fscanf(file, "%f,%f,%f", &r, &g, &b) == 3) {
        data.push_back({r, g, b});
    }
    fclose(file);
}

std::vector<float> ColorMap::get_color(float theta) const {
    if (theta < 0.0f || theta > 1.0f) {
        throw std::out_of_range("Theta must be between 0 and 1");
    }
    float index = theta * static_cast<float>(data.size()) - 1.0f;
    size_t lower_index = static_cast<size_t>(index);
    size_t upper_index = static_cast<size_t>(ceilf(index));
    if (lower_index == upper_index) {
        return data[lower_index];
    }
    float lower_weight = static_cast<float>(upper_index) - index;
    float upper_weight = index - static_cast<float>(lower_index);
    return {
        lower_weight * data[lower_index][0] + upper_weight * data[upper_index][0],
        lower_weight * data[lower_index][1] + upper_weight * data[upper_index][1],
        lower_weight * data[lower_index][2] + upper_weight * data[upper_index][2]
    };
}
