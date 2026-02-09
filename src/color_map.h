#pragma once

#include <string>
#include <vector>


class ColorMap {
public:
    ColorMap(const std::string& filename);
    std::vector<float> get_color(float theta) const;
private:
    std::vector<std::vector<float>> data;
};
