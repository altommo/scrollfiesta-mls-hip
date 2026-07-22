#include "mls_verify.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cleanroom_mls {

CompareReport compare_outputs(const ProjectionOutput& expected,
                              const ProjectionOutput& actual,
                              const CompareThresholds& thresholds) {
    const std::size_t count = expected.positions.size();
    if (expected.normals.size() != count || expected.status.size() != count ||
        actual.positions.size() != count || actual.normals.size() != count ||
        actual.status.size() != count) {
        throw std::invalid_argument("expected and actual output counts differ");
    }

    CompareReport report;
    report.count = count;
    double squared_position_sum = 0.0;
    constexpr float radians_to_degrees = 57.29577951308232f;
    for (std::size_t index = 0; index < count; ++index) {
        const Vec3 expected_position = expected.positions[index];
        const Vec3 actual_position = actual.positions[index];
        const Vec3 expected_normal = expected.normals[index];
        const Vec3 actual_normal = actual.normals[index];

        const Vec3 values[4] = {expected_position, actual_position, expected_normal,
                                actual_normal};
        bool row_finite = true;
        for (Vec3 value : values) {
            const bool value_finite = finite(value);
            row_finite = row_finite && value_finite;
            if (!value_finite) {
                ++report.non_finite_values;
            }
        }
        if (expected.status[index] != PointStatus::ok ||
            actual.status[index] != PointStatus::ok) {
            ++report.non_ok_statuses;
        }
        if (!row_finite) {
            continue;
        }

        const float dx = actual_position.x - expected_position.x;
        const float dy = actual_position.y - expected_position.y;
        const float dz = actual_position.z - expected_position.z;
        const float position_error = std::sqrt(dx * dx + dy * dy + dz * dz);
        report.max_position_error = std::max(report.max_position_error, position_error);
        squared_position_sum += static_cast<double>(position_error) * position_error;

        const float actual_normal_length = length(actual_normal);
        const float expected_normal_length = length(expected_normal);
        float angle_degrees = 180.0f;
        if (actual_normal_length > 0.0f && expected_normal_length > 0.0f) {
            const float normal_dot = std::clamp(
                dot(actual_normal, expected_normal) /
                    (actual_normal_length * expected_normal_length),
                -1.0f, 1.0f);
            angle_degrees = std::acos(normal_dot) * radians_to_degrees;
        }
        report.max_normal_angle_degrees =
            std::max(report.max_normal_angle_degrees, angle_degrees);
        report.max_normal_length_error =
            std::max(report.max_normal_length_error,
                     std::fabs(actual_normal_length - 1.0f));
    }
    if (count != 0) {
        report.rms_position_error =
            static_cast<float>(std::sqrt(squared_position_sum / static_cast<double>(count)));
    }
    const bool clean = report.non_finite_values == 0 && report.non_ok_statuses == 0;
    report.weld_pass = clean && report.max_position_error < thresholds.weld_position;
    report.parity_pass =
        clean && report.max_position_error <= thresholds.max_position &&
        report.rms_position_error <= thresholds.rms_position &&
        report.max_normal_angle_degrees <= thresholds.max_normal_angle_degrees;
    return report;
}

CompareExit classify_compare(const CompareReport& report) {
    if (report.non_finite_values != 0 || report.non_ok_statuses != 0) {
        return CompareExit::non_finite_or_characterization;
    }
    if (!report.weld_pass) {
        return CompareExit::weld_fail;
    }
    if (!report.parity_pass) {
        return CompareExit::parity_fail_weld_safe;
    }
    return CompareExit::pass;
}

std::string format_compare_report(const CompareReport& report,
                                  const CompareThresholds& thresholds) {
    std::ostringstream output;
    output << std::scientific << std::setprecision(9)
           << "vertices=" << report.count << '\n'
           << "non_finite_values=" << report.non_finite_values << '\n'
           << "non_ok_statuses=" << report.non_ok_statuses << '\n'
           << "max_position_error_vox=" << report.max_position_error
           << " threshold=" << thresholds.max_position << '\n'
           << "rms_position_error_vox=" << report.rms_position_error
           << " threshold=" << thresholds.rms_position << '\n'
           << "max_normal_angle_deg=" << report.max_normal_angle_degrees
           << " threshold=" << thresholds.max_normal_angle_degrees << '\n'
           << "max_normal_length_error=" << report.max_normal_length_error << '\n'
           << "weld_max_position_error_vox=" << report.max_position_error
           << " strict_threshold=" << thresholds.weld_position << '\n'
           << "parity=" << (report.parity_pass ? "PASS" : "FAIL") << '\n'
           << "weld_safety=" << (report.weld_pass ? "PASS" : "FAIL") << '\n'
           << "exit_code=" << static_cast<int>(classify_compare(report)) << '\n';
    return output.str();
}

}  // namespace cleanroom_mls
