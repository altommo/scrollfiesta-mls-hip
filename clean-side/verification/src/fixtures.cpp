#include "mls_verify.hpp"

#include <cmath>
#include <stdexcept>

namespace cleanroom_mls {
namespace {

Vec3 add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 multiply(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

Vec3 normalized(Vec3 value) {
    const float magnitude = length(value);
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

std::string metadata_for(const std::string& family, bool characterization) {
    return "{\"schema\":\"cleanroom-mls-fixture-v1\",\"family\":\"" + family +
           "\",\"classification\":\"" +
           (characterization ? "characterization_only" : "clean_side_assumption") +
           "\",\"generator\":\"independent-cpp17-fp32-v1\","
           "\"test_assumptions\":[\"xyz_triples\",\"explicit_sample_coordinates\","
           "\"cell_origin_is_metadata_only\",\"one_projection_per_call\","
           "\"nonnegative_dot_111\"],\"observed_upstream_semantics\":[]}";
}

void append_axis_plane(std::vector<Vec3>& samples, float z, int half_x, int half_y,
                       float spacing = 1.0f) {
    for (int ix = -half_x; ix <= half_x; ++ix) {
        for (int iy = -half_y; iy <= half_y; ++iy) {
            samples.push_back({spacing * static_cast<float>(ix),
                               spacing * static_cast<float>(iy), z});
        }
    }
}

Fixture axis_plane() {
    Fixture fixture;
    fixture.family = "plane_axis";
    fixture.radius = 5.25f;
    append_axis_plane(fixture.samples, 0.0f, 9, 7);
    fixture.queries = {{-2.2f, 1.3f, 2.0f}, {0.25f, -0.4f, -1.75f}, {3.1f, 0.7f, 0.8f}};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture oblique_plane() {
    Fixture fixture;
    fixture.family = "plane_oblique";
    fixture.radius = 5.75f;
    const Vec3 normal = normalized({1.0f, 2.0f, 3.0f});
    const Vec3 tangent_u = normalized({2.0f, -1.0f, 0.0f});
    const Vec3 tangent_v = normalized(cross(normal, tangent_u));
    const Vec3 center = {3.0f, -2.0f, 1.0f};
    for (int iu = -10; iu <= 10; ++iu) {
        for (int iv = -8; iv <= 8; ++iv) {
            fixture.samples.push_back(add(center,
                add(multiply(tangent_u, 0.65f * static_cast<float>(iu)),
                    multiply(tangent_v, 0.8f * static_cast<float>(iv)))));
        }
    }
    fixture.queries = {
        add(center, add(multiply(tangent_u, 1.2f), multiply(normal, 1.7f))),
        add(center, add(multiply(tangent_v, -1.1f), multiply(normal, -1.3f))),
        add(center, add(multiply(tangent_u, -2.0f), multiply(normal, 0.65f)))};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture curved_sheet() {
    Fixture fixture;
    fixture.family = "curved";
    fixture.radius = 5.0f;
    for (int ix = -12; ix <= 12; ++ix) {
        const float x = 0.6f * static_cast<float>(ix);
        for (int iy = -11; iy <= 11; ++iy) {
            const float y = 0.6f * static_cast<float>(iy);
            const float z = 0.025f * x * x + 0.015f * y * y + 0.004f * x * y;
            fixture.samples.push_back({x, y, z});
        }
    }
    fixture.queries = {{-2.1f, 1.4f, 1.9f}, {0.4f, -0.7f, -1.2f}, {2.6f, 1.9f, 2.2f}};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture two_sheet(bool include_both) {
    Fixture fixture;
    fixture.family = include_both ? "two_sheet_combined" : "two_sheet_isolated";
    fixture.radius = include_both ? 5.0f : 1.65f;
    for (int ix = -9; ix <= 9; ++ix) {
        for (int iy = -8; iy <= 8; ++iy) {
            const float x = 0.55f * static_cast<float>(ix);
            const float y = 0.6f * static_cast<float>(iy);
            fixture.samples.push_back({x, y, -1.0f});
            fixture.samples.push_back({x, y, 1.0f});
        }
    }
    fixture.queries = include_both
                          ? std::vector<Vec3>{{0.2f, -0.3f, 0.12f}, {-1.0f, 0.8f, -0.2f}}
                          : std::vector<Vec3>{{0.2f, -0.3f, -1.35f}, {-1.0f, 0.8f, 1.3f}};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture radius_boundary() {
    Fixture fixture;
    fixture.family = "radius_boundary";
    fixture.radius = 4.0f;
    append_axis_plane(fixture.samples, 0.0f, 5, 5, 0.75f);
    fixture.samples.push_back({3.999f, 0.0f, 0.8f});
    fixture.samples.push_back({4.0f, 0.0f, 1.1f});
    fixture.samples.push_back({4.001f, 0.0f, 1.4f});
    fixture.queries = {{0.0f, 0.0f, 0.6f}};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture order_duplicates() {
    Fixture fixture = curved_sheet();
    fixture.family = "order_duplicates";
    fixture.queries = {{2.6f, 1.9f, 2.2f},
                       {-2.1f, 1.4f, 1.9f},
                       {2.6f, 1.9f, 2.2f},
                       {0.4f, -0.7f, -1.2f}};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture translated(bool shifted) {
    Fixture fixture = oblique_plane();
    fixture.family = shifted ? "translation_shifted" : "translation_base";
    if (shifted) {
        const Vec3 translation = {17.25f, -9.5f, 4.75f};
        for (Vec3& sample : fixture.samples) {
            sample = add(sample, translation);
        }
        for (Vec3& query : fixture.queries) {
            query = add(query, translation);
        }
        fixture.cell_origin = translation;
    }
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture neighborhood_change() {
    Fixture fixture;
    fixture.family = "neighborhood_change";
    fixture.radius = 4.0f;
    append_axis_plane(fixture.samples, 0.0f, 7, 7, 0.5f);
    fixture.queries = {{0.35f, -0.4f, 2.5f}};
    fixture.metadata_json = metadata_for(fixture.family, false);
    return fixture;
}

Fixture degenerate(const std::string& family) {
    Fixture fixture;
    fixture.family = family;
    fixture.radius = 3.0f;
    fixture.queries = {{0.0f, 0.0f, 0.0f}};
    if (family == "degenerate_single") {
        fixture.samples = {{0.0f, 0.0f, 0.0f}};
    } else if (family == "degenerate_collinear") {
        fixture.samples = {{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
                           {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}};
    } else if (family == "degenerate_coincident") {
        fixture.samples = {{0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
                           {0.5f, 0.5f, 0.5f}};
        fixture.queries = {{0.5f, 0.5f, 0.5f}};
    } else if (family == "degenerate_isotropic") {
        fixture.samples = {{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                           {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                           {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}};
    } else if (family != "degenerate_empty") {
        throw std::invalid_argument("unknown degenerate fixture: " + family);
    }
    fixture.metadata_json = metadata_for(fixture.family, true);
    return fixture;
}

}  // namespace

std::vector<std::string> fixture_families() {
    return {"plane_axis",          "plane_oblique",       "curved",
            "two_sheet_isolated", "two_sheet_combined",  "neighborhood_change",
            "radius_boundary",    "order_duplicates",    "translation_base",
            "translation_shifted", "degenerate_empty",    "degenerate_single",
            "degenerate_collinear", "degenerate_coincident", "degenerate_isotropic"};
}

Fixture make_fixture(const std::string& family) {
    if (family == "plane_axis") {
        return axis_plane();
    }
    if (family == "plane_oblique") {
        return oblique_plane();
    }
    if (family == "curved") {
        return curved_sheet();
    }
    if (family == "two_sheet_isolated") {
        return two_sheet(false);
    }
    if (family == "two_sheet_combined") {
        return two_sheet(true);
    }
    if (family == "radius_boundary") {
        return radius_boundary();
    }
    if (family == "order_duplicates") {
        return order_duplicates();
    }
    if (family == "translation_base") {
        return translated(false);
    }
    if (family == "translation_shifted") {
        return translated(true);
    }
    if (family == "neighborhood_change") {
        return neighborhood_change();
    }
    if (family.rfind("degenerate_", 0) == 0) {
        return degenerate(family);
    }
    throw std::invalid_argument("unknown fixture family: " + family);
}

}  // namespace cleanroom_mls
