#ifndef CLEANROOM_MLS_VERIFY_HPP
#define CLEANROOM_MLS_VERIFY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cleanroom_mls {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class PointStatus : std::uint32_t {
    ok = 0,
    invalid_radius = 1,
    empty_neighborhood = 2,
    non_finite_input = 3,
    non_unique_normal = 4,
    numerical_failure = 5,
};

struct PlaneFit {
    Vec3 centroid{};
    Vec3 normal{};
    PointStatus status = PointStatus::numerical_failure;
    std::size_t positive_weight_samples = 0;
};

struct ProjectionOutput {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<PointStatus> status;
};

struct Fixture {
    std::string family;
    std::string metadata_json;
    std::vector<Vec3> samples;
    std::vector<Vec3> queries;
    float radius = 12.0f;
    Vec3 cell_origin{};
    std::uint32_t iterations = 1;
};

struct GoldenRecord {
    Fixture fixture;
    ProjectionOutput output;
};

struct CompareThresholds {
    float max_position = 1.24e-3f;
    float rms_position = 2.2e-4f;
    float max_normal_angle_degrees = 0.006f;
    float weld_position = 0.25f;
};

struct CompareReport {
    std::size_t count = 0;
    std::size_t non_finite_values = 0;
    std::size_t non_ok_statuses = 0;
    float max_position_error = 0.0f;
    float rms_position_error = 0.0f;
    float max_normal_angle_degrees = 0.0f;
    float max_normal_length_error = 0.0f;
    bool parity_pass = false;
    bool weld_pass = false;
};

// Exit values are intentionally stable for automation.
enum class CompareExit : int {
    pass = 0,
    parity_fail_weld_safe = 10,
    weld_fail = 20,
    non_finite_or_characterization = 30,
    input_error = 40,
};

float wendland_c2(float distance, float radius);
PlaneFit fit_local_plane_fp32(const std::vector<Vec3>& samples,
                              Vec3 query,
                              float radius);
ProjectionOutput project_fp32(const std::vector<Vec3>& samples,
                              const std::vector<Vec3>& queries,
                              float radius);

std::vector<std::string> fixture_families();
Fixture make_fixture(const std::string& family);

void write_golden(const std::string& path, const GoldenRecord& record);
GoldenRecord read_golden(const std::string& path);
void write_raw_vec3(const std::string& path, const std::vector<Vec3>& values);
std::vector<Vec3> read_raw_vec3(const std::string& path, std::size_t count);

CompareReport compare_outputs(const ProjectionOutput& expected,
                              const ProjectionOutput& actual,
                              const CompareThresholds& thresholds = {});
CompareExit classify_compare(const CompareReport& report);
std::string format_compare_report(const CompareReport& report,
                                  const CompareThresholds& thresholds = {});

bool finite(Vec3 value);
float length(Vec3 value);
float dot(Vec3 a, Vec3 b);

}  // namespace cleanroom_mls

#endif
