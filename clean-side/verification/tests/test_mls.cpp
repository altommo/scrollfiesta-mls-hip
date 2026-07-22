#include "mls_verify.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using cleanroom_mls::Fixture;
using cleanroom_mls::PointStatus;
using cleanroom_mls::ProjectionOutput;
using cleanroom_mls::Vec3;

int failures = 0;

#define CHECK(condition)                                                         \
    do {                                                                         \
        if (!(condition)) {                                                       \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "      \
                      << #condition << '\n';                                      \
            ++failures;                                                           \
        }                                                                         \
    } while (false)

void check_near(float actual, float expected, float tolerance, int line) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        std::cerr << __FILE__ << ':' << line << ": expected " << actual << " ~= "
                  << expected << " within " << tolerance << '\n';
        ++failures;
    }
}

#define CHECK_NEAR(actual, expected, tolerance) \
    check_near((actual), (expected), (tolerance), __LINE__)

Vec3 add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 multiply(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 normalized(Vec3 value) {
    return multiply(value, 1.0f / cleanroom_mls::length(value));
}

bool same(Vec3 a, Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

void require_ok(const ProjectionOutput& output) {
    for (std::size_t index = 0; index < output.positions.size(); ++index) {
        CHECK(output.status[index] == PointStatus::ok);
        CHECK(cleanroom_mls::finite(output.positions[index]));
        CHECK(cleanroom_mls::finite(output.normals[index]));
        CHECK_NEAR(cleanroom_mls::length(output.normals[index]), 1.0f, 2.0e-5f);
    }
}

void test_public_operator_is_one_projection_per_call() {
    for (const std::string& family : cleanroom_mls::fixture_families()) {
        const Fixture fixture = cleanroom_mls::make_fixture(family);
        CHECK(fixture.iterations == 1);
    }
}

void test_wendland_and_radius_boundary() {
    const float radius = 4.0f;
    CHECK_NEAR(cleanroom_mls::wendland_c2(0.0f, radius), 1.0f, 0.0f);
    CHECK(cleanroom_mls::wendland_c2(std::nextafter(radius, 0.0f), radius) > 0.0f);
    CHECK(cleanroom_mls::wendland_c2(radius, radius) == 0.0f);
    CHECK(cleanroom_mls::wendland_c2(std::nextafter(radius, 5.0f), radius) == 0.0f);

    Fixture plane = cleanroom_mls::make_fixture("plane_axis");
    const Vec3 query{0.0f, 0.0f, 0.0f};
    const auto baseline = cleanroom_mls::fit_local_plane_fp32(plane.samples, query, radius);
    plane.samples.push_back({0.0f, 0.0f, radius});
    plane.samples.push_back({0.0f, 0.0f, std::nextafter(radius, 5.0f)});
    const auto boundary = cleanroom_mls::fit_local_plane_fp32(plane.samples, query, radius);
    CHECK(baseline.status == PointStatus::ok);
    CHECK(boundary.status == PointStatus::ok);
    CHECK(same(baseline.centroid, boundary.centroid));
    CHECK(same(baseline.normal, boundary.normal));
}

void test_axis_plane() {
    const Fixture fixture = cleanroom_mls::make_fixture("plane_axis");
    const ProjectionOutput output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    require_ok(output);
    for (std::size_t index = 0; index < output.positions.size(); ++index) {
        CHECK_NEAR(output.positions[index].x, fixture.queries[index].x, 2.0e-5f);
        CHECK_NEAR(output.positions[index].y, fixture.queries[index].y, 2.0e-5f);
        CHECK_NEAR(output.positions[index].z, 0.0f, 2.0e-5f);
        CHECK_NEAR(output.normals[index].x, 0.0f, 2.0e-5f);
        CHECK_NEAR(output.normals[index].y, 0.0f, 2.0e-5f);
        CHECK_NEAR(output.normals[index].z, 1.0f, 2.0e-5f);
    }
}

void test_oblique_plane_selects_smallest_eigenvector() {
    const Fixture fixture = cleanroom_mls::make_fixture("plane_oblique");
    const ProjectionOutput output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    require_ok(output);
    const Vec3 expected_normal = normalized({1.0f, 2.0f, 3.0f});
    const Vec3 center{3.0f, -2.0f, 1.0f};
    for (std::size_t index = 0; index < output.positions.size(); ++index) {
        CHECK_NEAR(cleanroom_mls::dot(subtract(output.positions[index], center),
                                     expected_normal),
                   0.0f, 1.0e-4f);
        CHECK(cleanroom_mls::dot(output.normals[index], expected_normal) > 0.99999f);
        const float input_distance =
            cleanroom_mls::dot(subtract(fixture.queries[index], center), expected_normal);
        const Vec3 analytic =
            subtract(fixture.queries[index], multiply(expected_normal, input_distance));
        CHECK(cleanroom_mls::length(subtract(output.positions[index], analytic)) < 2.0e-4f);
    }
}

void test_curved_sheet() {
    const Fixture fixture = cleanroom_mls::make_fixture("curved");
    const ProjectionOutput output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    require_ok(output);
    for (std::size_t index = 0; index < output.positions.size(); ++index) {
        CHECK(cleanroom_mls::length(subtract(output.positions[index], fixture.queries[index])) >
              0.2f);
        CHECK(output.normals[index].z > 0.9f);
    }
}

void test_two_sheets() {
    const Fixture isolated = cleanroom_mls::make_fixture("two_sheet_isolated");
    const ProjectionOutput isolated_output = cleanroom_mls::project_fp32(
        isolated.samples, isolated.queries, isolated.radius);
    require_ok(isolated_output);
    CHECK_NEAR(isolated_output.positions[0].z, -1.0f, 2.0e-5f);
    CHECK_NEAR(isolated_output.positions[1].z, 1.0f, 2.0e-5f);

    const Fixture combined = cleanroom_mls::make_fixture("two_sheet_combined");
    const ProjectionOutput combined_output = cleanroom_mls::project_fp32(
        combined.samples, combined.queries, combined.radius);
    require_ok(combined_output);
    CHECK(combined_output.positions[0].z > -0.9f && combined_output.positions[0].z < 0.9f);
    CHECK(combined_output.positions[1].z > -0.9f && combined_output.positions[1].z < 0.9f);
}

void test_single_projection_uses_input_neighborhood() {
    const Fixture fixture = cleanroom_mls::make_fixture("neighborhood_change");
    const ProjectionOutput output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    require_ok(output);
    const auto fit = cleanroom_mls::fit_local_plane_fp32(
        fixture.samples, fixture.queries[0], fixture.radius);
    CHECK(fit.status == PointStatus::ok);
    const Vec3 delta = subtract(fixture.queries[0], fit.centroid);
    const Vec3 expected = subtract(
        fixture.queries[0], multiply(fit.normal, cleanroom_mls::dot(fit.normal, delta)));
    CHECK(cleanroom_mls::length(subtract(output.positions[0], expected)) < 2.0e-5f);
    CHECK(cleanroom_mls::dot(output.normals[0], fit.normal) > 0.99999f);
}

void test_order_and_duplicates() {
    const Fixture fixture = cleanroom_mls::make_fixture("order_duplicates");
    const ProjectionOutput output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    require_ok(output);
    CHECK(same(output.positions[0], output.positions[2]));
    CHECK(same(output.normals[0], output.normals[2]));

    const std::vector<std::size_t> permutation = {2, 0, 3, 1};
    std::vector<Vec3> permuted_queries;
    for (std::size_t index : permutation) {
        permuted_queries.push_back(fixture.queries[index]);
    }
    const ProjectionOutput permuted = cleanroom_mls::project_fp32(
        fixture.samples, permuted_queries, fixture.radius);
    require_ok(permuted);
    for (std::size_t index = 0; index < permutation.size(); ++index) {
        CHECK(same(permuted.positions[index], output.positions[permutation[index]]));
        CHECK(same(permuted.normals[index], output.normals[permutation[index]]));
    }
}

void test_translation_equivariance_assumption() {
    const Fixture base = cleanroom_mls::make_fixture("translation_base");
    const Fixture shifted = cleanroom_mls::make_fixture("translation_shifted");
    const ProjectionOutput base_output = cleanroom_mls::project_fp32(
        base.samples, base.queries, base.radius);
    const ProjectionOutput shifted_output = cleanroom_mls::project_fp32(
        shifted.samples, shifted.queries, shifted.radius);
    require_ok(base_output);
    require_ok(shifted_output);
    const Vec3 translation{17.25f, -9.5f, 4.75f};
    for (std::size_t index = 0; index < base_output.positions.size(); ++index) {
        CHECK(cleanroom_mls::length(subtract(
                  shifted_output.positions[index], add(base_output.positions[index], translation))) <
              4.0e-4f);
        CHECK(cleanroom_mls::dot(shifted_output.normals[index], base_output.normals[index]) >
              0.99999f);
    }
}

void test_degeneracy_characterization() {
    const std::vector<std::string> failure_families = {
        "degenerate_empty", "degenerate_single", "degenerate_coincident"};
    for (const std::string& family : failure_families) {
        const Fixture fixture = cleanroom_mls::make_fixture(family);
        const ProjectionOutput output = cleanroom_mls::project_fp32(
            fixture.samples, fixture.queries, fixture.radius);
        CHECK(output.status[0] != PointStatus::ok);
        CHECK(!cleanroom_mls::finite(output.normals[0]));
    }
    for (const char* family : {"degenerate_collinear", "degenerate_isotropic"}) {
        const Fixture fixture = cleanroom_mls::make_fixture(family);
        const ProjectionOutput output = cleanroom_mls::project_fp32(
            fixture.samples, fixture.queries, fixture.radius);
        require_ok(output);
    }
    const Fixture plane = cleanroom_mls::make_fixture("plane_axis");
    CHECK(cleanroom_mls::project_fp32(plane.samples, plane.queries, 0.0f).status[0] ==
          PointStatus::invalid_radius);
    CHECK(cleanroom_mls::project_fp32(plane.samples, plane.queries, -1.0f).status[0] ==
          PointStatus::invalid_radius);
    CHECK(cleanroom_mls::project_fp32(
              plane.samples, plane.queries, std::numeric_limits<float>::quiet_NaN())
              .status[0] == PointStatus::invalid_radius);
}

void test_binary_roundtrip_and_determinism() {
    const Fixture fixture = cleanroom_mls::make_fixture("plane_oblique");
    cleanroom_mls::GoldenRecord record;
    record.fixture = fixture;
    record.output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    const std::filesystem::path first = "test_roundtrip_a.mlsg";
    const std::filesystem::path second = "test_roundtrip_b.mlsg";
    cleanroom_mls::write_golden(first.string(), record);
    const cleanroom_mls::GoldenRecord loaded = cleanroom_mls::read_golden(first.string());
    cleanroom_mls::write_golden(second.string(), loaded);
    CHECK(std::filesystem::file_size(first) == std::filesystem::file_size(second));
    const auto report = cleanroom_mls::compare_outputs(record.output, loaded.output);
    CHECK(cleanroom_mls::classify_compare(report) == cleanroom_mls::CompareExit::pass);
    CHECK(loaded.fixture.metadata_json == fixture.metadata_json);
    CHECK(loaded.fixture.samples.size() == fixture.samples.size());
    std::filesystem::remove(first);
    std::filesystem::remove(second);
}

void test_comparator_exit_codes() {
    ProjectionOutput expected;
    expected.positions = {{0.0f, 0.0f, 0.0f}};
    expected.normals = {{0.0f, 0.0f, 1.0f}};
    expected.status = {PointStatus::ok};
    ProjectionOutput actual = expected;
    CHECK(cleanroom_mls::classify_compare(
              cleanroom_mls::compare_outputs(expected, actual)) ==
          cleanroom_mls::CompareExit::pass);
    actual.normals[0].z = std::nextafter(1.0f, 0.0f);
    CHECK(cleanroom_mls::classify_compare(
              cleanroom_mls::compare_outputs(expected, actual)) ==
          cleanroom_mls::CompareExit::pass);
    actual = expected;
    actual.positions[0].x = 0.01f;
    CHECK(cleanroom_mls::classify_compare(
              cleanroom_mls::compare_outputs(expected, actual)) ==
          cleanroom_mls::CompareExit::parity_fail_weld_safe);
    actual.positions[0].x = 0.25f;
    CHECK(cleanroom_mls::classify_compare(
              cleanroom_mls::compare_outputs(expected, actual)) ==
          cleanroom_mls::CompareExit::weld_fail);
    actual = expected;
    actual.normals[0].z = std::numeric_limits<float>::quiet_NaN();
    CHECK(cleanroom_mls::classify_compare(
              cleanroom_mls::compare_outputs(expected, actual)) ==
          cleanroom_mls::CompareExit::non_finite_or_characterization);
}

}  // namespace

int main() {
    test_public_operator_is_one_projection_per_call();
    test_wendland_and_radius_boundary();
    test_axis_plane();
    test_oblique_plane_selects_smallest_eigenvector();
    test_curved_sheet();
    test_two_sheets();
    test_single_projection_uses_input_neighborhood();
    test_order_and_duplicates();
    test_translation_equivariance_assumption();
    test_degeneracy_characterization();
    test_binary_roundtrip_and_determinism();
    test_comparator_exit_codes();
    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "all clean-room MLS oracle tests passed\n";
    return 0;
}
