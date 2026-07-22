#include "mls_verify.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace cleanroom_mls {
namespace {

Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 multiply(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 divide(Vec3 value, float scale) {
    return {value.x / scale, value.y / scale, value.z / scale};
}

Vec3 failure_normal() {
    const float value = std::numeric_limits<float>::quiet_NaN();
    return {value, value, value};
}

struct EigenResult {
    std::array<float, 3> values{};
    std::array<Vec3, 3> vectors{};
    bool finite = false;
};

EigenResult jacobi_symmetric_fp32(const float covariance[3][3]) {
    float a[3][3] = {};
    float vectors[3][3] = {};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            a[row][column] = covariance[row][column];
            vectors[row][column] = row == column ? 1.0f : 0.0f;
        }
    }

    // A cyclic Jacobi solve is deliberately independent of any GPU implementation.
    // Fixed sweeps keep the reference deterministic and use FP32 at every update.
    constexpr std::array<std::pair<int, int>, 3> pairs = {
        std::pair<int, int>{0, 1}, {0, 2}, {1, 2}};
    for (int sweep = 0; sweep < 16; ++sweep) {
        for (const auto& [p, q] : pairs) {
            const float apq = a[p][q];
            const float scale = std::fabs(a[p][p]) + std::fabs(a[q][q]) + 1.0f;
            if (std::fabs(apq) <= std::numeric_limits<float>::epsilon() * scale) {
                continue;
            }

            const float angle = 0.5f * std::atan2(2.0f * apq, a[q][q] - a[p][p]);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float app = a[p][p];
            const float aqq = a[q][q];

            a[p][p] = c * c * app - 2.0f * s * c * apq + s * s * aqq;
            a[q][q] = s * s * app + 2.0f * s * c * apq + c * c * aqq;
            a[p][q] = 0.0f;
            a[q][p] = 0.0f;

            for (int row = 0; row < 3; ++row) {
                if (row == p || row == q) {
                    continue;
                }
                const float arp = a[row][p];
                const float arq = a[row][q];
                const float rotated_p = c * arp - s * arq;
                const float rotated_q = s * arp + c * arq;
                a[row][p] = rotated_p;
                a[p][row] = rotated_p;
                a[row][q] = rotated_q;
                a[q][row] = rotated_q;
            }

            for (int row = 0; row < 3; ++row) {
                const float vrp = vectors[row][p];
                const float vrq = vectors[row][q];
                vectors[row][p] = c * vrp - s * vrq;
                vectors[row][q] = s * vrp + c * vrq;
            }
        }
    }

    EigenResult result;
    result.values = {a[0][0], a[1][1], a[2][2]};
    result.vectors = {{{vectors[0][0], vectors[1][0], vectors[2][0]},
                       {vectors[0][1], vectors[1][1], vectors[2][1]},
                       {vectors[0][2], vectors[1][2], vectors[2][2]}}};
    result.finite = true;
    for (float value : result.values) {
        result.finite = result.finite && std::isfinite(value);
    }
    for (Vec3 vector : result.vectors) {
        result.finite = result.finite && finite(vector);
    }
    return result;
}

}  // namespace

bool finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(Vec3 value) {
    return std::sqrt(dot(value, value));
}

float wendland_c2(float distance, float radius) {
    if (!std::isfinite(distance) || !std::isfinite(radius) || radius <= 0.0f ||
        distance < 0.0f || distance >= radius) {
        return 0.0f;
    }
    const float q = distance / radius;
    const float one_minus_q = 1.0f - q;
    const float square = one_minus_q * one_minus_q;
    return square * square * (4.0f * q + 1.0f);
}

PlaneFit fit_local_plane_fp32(const std::vector<Vec3>& samples,
                              Vec3 query,
                              float radius) {
    PlaneFit result;
    result.normal = failure_normal();
    if (!std::isfinite(radius) || radius <= 0.0f) {
        result.status = PointStatus::invalid_radius;
        return result;
    }
    if (!finite(query)) {
        result.status = PointStatus::non_finite_input;
        return result;
    }

    std::vector<float> weights(samples.size(), 0.0f);
    float total_weight = 0.0f;
    Vec3 weighted_sum{};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const Vec3 sample = samples[index];
        if (!finite(sample)) {
            result.status = PointStatus::non_finite_input;
            return result;
        }
        const Vec3 delta = subtract(sample, query);
        const float distance = length(delta);
        const float weight = wendland_c2(distance, radius);
        weights[index] = weight;
        if (weight > 0.0f) {
            ++result.positive_weight_samples;
            total_weight += weight;
            weighted_sum.x += weight * sample.x;
            weighted_sum.y += weight * sample.y;
            weighted_sum.z += weight * sample.z;
        }
    }
    if (!(total_weight > 0.0f) || !std::isfinite(total_weight)) {
        result.status = PointStatus::empty_neighborhood;
        return result;
    }

    result.centroid = divide(weighted_sum, total_weight);
    float covariance[3][3] = {};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const float weight = weights[index];
        if (!(weight > 0.0f)) {
            continue;
        }
        const Vec3 centered = subtract(samples[index], result.centroid);
        const float values[3] = {centered.x, centered.y, centered.z};
        for (int row = 0; row < 3; ++row) {
            for (int column = row; column < 3; ++column) {
                covariance[row][column] += weight * values[row] * values[column];
            }
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = row; column < 3; ++column) {
            covariance[row][column] /= total_weight;
            covariance[column][row] = covariance[row][column];
        }
    }

    if (result.positive_weight_samples < 4) {
        result.status = PointStatus::non_unique_normal;
        return result;
    }

    const EigenResult eigen = jacobi_symmetric_fp32(covariance);
    if (!eigen.finite) {
        result.status = PointStatus::numerical_failure;
        return result;
    }
    std::array<std::pair<float, int>, 3> ordered = {
        std::pair<float, int>{eigen.values[0], 0},
        {eigen.values[1], 1},
        {eigen.values[2], 2}};
    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        if (left.first == right.first) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    Vec3 normal = eigen.vectors[ordered[0].second];
    const float normal_length = length(normal);
    if (!(normal_length > 0.0f) || !std::isfinite(normal_length)) {
        result.status = PointStatus::numerical_failure;
        return result;
    }
    normal = divide(normal, normal_length);

    const float sign_dot = normal.x + normal.y + normal.z;
    if (sign_dot < -1.0e-9f) {
        normal = multiply(normal, -1.0f);
    } else if (sign_dot < 1.0e-9f) {
        const float components[3] = {normal.x, normal.y, normal.z};
        int largest = 0;
        for (int component = 1; component < 3; ++component) {
            if (std::fabs(components[component]) > std::fabs(components[largest])) {
                largest = component;
            }
        }
        if (components[largest] < 0.0f) {
            normal = multiply(normal, -1.0f);
        }
    }

    result.normal = normal;
    result.status = PointStatus::ok;
    return result;
}

ProjectionOutput project_fp32(const std::vector<Vec3>& samples,
                              const std::vector<Vec3>& queries,
                              float radius) {
    ProjectionOutput output;
    output.positions.reserve(queries.size());
    output.normals.reserve(queries.size());
    output.status.reserve(queries.size());

    for (Vec3 query : queries) {
        const PlaneFit fit = fit_local_plane_fp32(samples, query, radius);
        PointStatus status = fit.status;
        Vec3 current = query;
        if (status == PointStatus::ok) {
            const Vec3 from_centroid = subtract(query, fit.centroid);
            const float signed_distance = dot(fit.normal, from_centroid);
            current = subtract(query, multiply(fit.normal, signed_distance));
            if (!finite(current)) status = PointStatus::numerical_failure;
        }
        output.positions.push_back(current);
        output.normals.push_back(status == PointStatus::ok ? fit.normal : failure_normal());
        output.status.push_back(status);
    }
    return output;
}

}  // namespace cleanroom_mls
