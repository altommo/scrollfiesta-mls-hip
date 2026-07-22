#include "mls_verify.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {

using cleanroom_mls::GoldenRecord;
using cleanroom_mls::PointStatus;

void usage(std::ostream& output) {
    output <<
        "Usage:\n"
        "  mls_verify list-families\n"
        "  mls_verify generate FAMILY OUTPUT.mlsg\n"
        "  mls_verify generate-all OUTPUT_DIRECTORY\n"
        "  mls_verify oracle INPUT.mlsg OUTPUT.mlsg\n"
        "  mls_verify compare EXPECTED.mlsg ACTUAL.mlsg\n"
        "  mls_verify export-input INPUT.mlsg OUTPUT_PREFIX\n"
        "  mls_verify export-output INPUT.mlsg OUTPUT_PREFIX\n"
        "  mls_verify pack-results TEMPLATE.mlsg POSITIONS.f32 NORMALS.f32 OUTPUT.mlsg\n"
        "  mls_verify inspect INPUT.mlsg\n";
}

GoldenRecord calculate(const cleanroom_mls::Fixture& fixture) {
    GoldenRecord record;
    record.fixture = fixture;
    record.output = cleanroom_mls::project_fp32(
        fixture.samples, fixture.queries, fixture.radius);
    return record;
}

std::uint32_t float_bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void write_params(const std::string& path, const GoldenRecord& record) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot open params output: " + path);
    }
    output << std::setprecision(9)
           << "format=raw-little-endian-fp32-xyz-triples\n"
           << "sample_count=" << record.fixture.samples.size() << '\n'
           << "query_count=" << record.fixture.queries.size() << '\n'
           << "iterations=" << record.fixture.iterations << '\n'
           << "radius=" << record.fixture.radius << '\n'
           << "radius_bits=0x" << std::hex << std::setw(8) << std::setfill('0')
           << float_bits(record.fixture.radius) << std::dec << '\n'
           << "cell_origin=" << record.fixture.cell_origin.x << ','
           << record.fixture.cell_origin.y << ',' << record.fixture.cell_origin.z << '\n'
           << "metadata_json=" << record.fixture.metadata_json << '\n';
    if (!output) {
        throw std::runtime_error("failed writing params output: " + path);
    }
}

int run(int argc, char** argv) {
    if (argc < 2) {
        usage(std::cerr);
        return static_cast<int>(cleanroom_mls::CompareExit::input_error);
    }
    const std::string command = argv[1];
    if (command == "list-families" && argc == 2) {
        for (const std::string& family : cleanroom_mls::fixture_families()) {
            std::cout << family << '\n';
        }
        return 0;
    }
    if (command == "generate" && argc == 4) {
        const auto fixture = cleanroom_mls::make_fixture(argv[2]);
        cleanroom_mls::write_golden(argv[3], calculate(fixture));
        std::cout << "generated " << argv[3] << '\n';
        return 0;
    }
    if (command == "generate-all" && argc == 3) {
        const std::filesystem::path directory = argv[2];
        std::filesystem::create_directories(directory);
        for (const std::string& family : cleanroom_mls::fixture_families()) {
            const std::filesystem::path path = directory / (family + ".mlsg");
            cleanroom_mls::write_golden(path.string(),
                                        calculate(cleanroom_mls::make_fixture(family)));
            std::cout << "generated " << path.string() << '\n';
        }
        return 0;
    }
    if (command == "oracle" && argc == 4) {
        GoldenRecord record = cleanroom_mls::read_golden(argv[2]);
        if (record.fixture.iterations != 1) {
            throw std::invalid_argument("oracle input must describe one projection per call");
        }
        record.output = cleanroom_mls::project_fp32(
            record.fixture.samples, record.fixture.queries, record.fixture.radius);
        cleanroom_mls::write_golden(argv[3], record);
        std::cout << "wrote oracle output " << argv[3] << '\n';
        return 0;
    }
    if (command == "compare" && argc == 4) {
        const GoldenRecord expected = cleanroom_mls::read_golden(argv[2]);
        const GoldenRecord actual = cleanroom_mls::read_golden(argv[3]);
        if (expected.fixture.queries.size() != actual.fixture.queries.size()) {
            throw std::invalid_argument("golden and actual query counts differ");
        }
        const auto report = cleanroom_mls::compare_outputs(expected.output, actual.output);
        std::cout << cleanroom_mls::format_compare_report(report);
        return static_cast<int>(cleanroom_mls::classify_compare(report));
    }
    if (command == "export-input" && argc == 4) {
        const GoldenRecord record = cleanroom_mls::read_golden(argv[2]);
        const std::string prefix = argv[3];
        cleanroom_mls::write_raw_vec3(prefix + ".samples.f32", record.fixture.samples);
        cleanroom_mls::write_raw_vec3(prefix + ".queries.f32", record.fixture.queries);
        write_params(prefix + ".params.txt", record);
        std::cout << "wrote " << prefix << ".{samples.f32,queries.f32,params.txt}\n";
        return 0;
    }
    if (command == "export-output" && argc == 4) {
        const GoldenRecord record = cleanroom_mls::read_golden(argv[2]);
        const std::string prefix = argv[3];
        cleanroom_mls::write_raw_vec3(prefix + ".positions.f32", record.output.positions);
        cleanroom_mls::write_raw_vec3(prefix + ".normals.f32", record.output.normals);
        std::cout << "wrote " << prefix << ".{positions.f32,normals.f32}\n";
        return 0;
    }
    if (command == "pack-results" && argc == 6) {
        GoldenRecord record = cleanroom_mls::read_golden(argv[2]);
        const std::size_t count = record.fixture.queries.size();
        record.output.positions = cleanroom_mls::read_raw_vec3(argv[3], count);
        record.output.normals = cleanroom_mls::read_raw_vec3(argv[4], count);
        record.output.status.assign(count, PointStatus::ok);
        record.fixture.metadata_json =
            "{\"schema\":\"cleanroom-mls-fixture-v1\","
            "\"classification\":\"external_output\","
            "\"source\":\"raw_little_endian_fp32_xyz\","
            "\"observed_upstream_semantics\":[]}";
        cleanroom_mls::write_golden(argv[5], record);
        std::cout << "packed " << argv[5] << '\n';
        return 0;
    }
    if (command == "inspect" && argc == 3) {
        const GoldenRecord record = cleanroom_mls::read_golden(argv[2]);
        std::size_t non_ok = 0;
        for (PointStatus status : record.output.status) {
            non_ok += status == PointStatus::ok ? 0u : 1u;
        }
        std::cout << "family=" << record.fixture.family << '\n'
                  << "samples=" << record.fixture.samples.size() << '\n'
                  << "queries=" << record.fixture.queries.size() << '\n'
                  << "iterations=" << record.fixture.iterations << '\n'
                  << "radius=" << std::setprecision(9) << record.fixture.radius << '\n'
                  << "cell_origin=" << record.fixture.cell_origin.x << ','
                  << record.fixture.cell_origin.y << ',' << record.fixture.cell_origin.z
                  << '\n'
                  << "non_ok_statuses=" << non_ok << '\n'
                  << "metadata_json=" << record.fixture.metadata_json << '\n';
        return 0;
    }

    usage(std::cerr);
    return static_cast<int>(cleanroom_mls::CompareExit::input_error);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return static_cast<int>(cleanroom_mls::CompareExit::input_error);
    }
}
