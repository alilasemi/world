#include <cmath>
#include <vector>

class ExampleODE {
public:
    std::vector<float> state = {1.0f};
    float time = 0.0f;

    void compute_rhs(std::vector<float>& rhs, const std::vector<float>& u, const float t) const {
        rhs[0] = u[0] * std::cos(t);
    }

    void compute_jacobian(std::vector<float>& jac, const std::vector<float>& u, const float t) const {
        jac[0] = std::cos(t);
    }

    void compute_exact_solution(const std::vector<float> t, std::vector<float>& u) {
        u.resize(t.size());
        for (size_t i = 0; i < t.size(); ++i) {
            u[i] = std::exp(std::sin(t[i]));
        }
    }
};
