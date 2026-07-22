#include "mls_verify.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cleanroom_mls {
namespace {

constexpr char kMagic[8] = {'M', 'L', 'S', 'G', 'O', 'L', 'D', '\0'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kHeaderBytes = 128;
constexpr std::uint32_t kEndianMarker = 0x01020304u;
constexpr std::uint32_t kAllSections = 0x1fu;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

void append_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

void append_f32(std::vector<std::uint8_t>& bytes, float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "binary32 float required");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("truncated u32 field");
    }
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index) {
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (8 * index);
    }
    return value;
}

std::uint64_t get_u64(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 8) {
        throw std::runtime_error("truncated u64 field");
    }
    std::uint64_t value = 0;
    for (unsigned index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (8 * index);
    }
    return value;
}

float get_f32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    const std::uint32_t bits = get_u32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = kFnvOffset;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= kFnvPrime;
    }
    return hash;
}

void append_vec3(std::vector<std::uint8_t>& bytes, const std::vector<Vec3>& values) {
    for (Vec3 value : values) {
        append_f32(bytes, value.x);
        append_f32(bytes, value.y);
        append_f32(bytes, value.z);
    }
}

std::vector<Vec3> extract_vec3(const std::vector<std::uint8_t>& bytes,
                               std::size_t& offset,
                               std::uint64_t byte_count,
                               std::uint64_t expected_count) {
    if (byte_count != expected_count * 12u) {
        throw std::runtime_error("Vec3 section size does not match count");
    }
    if (byte_count > static_cast<std::uint64_t>(bytes.size() - offset)) {
        throw std::runtime_error("truncated Vec3 section");
    }
    std::vector<Vec3> values;
    values.reserve(static_cast<std::size_t>(expected_count));
    for (std::uint64_t index = 0; index < expected_count; ++index) {
        values.push_back({get_f32(bytes, offset), get_f32(bytes, offset + 4),
                          get_f32(bytes, offset + 8)});
        offset += 12;
    }
    return values;
}

void write_bytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot open output: " + path);
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("failed writing output: " + path);
    }
}

std::vector<std::uint8_t> read_bytes(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open input: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

}  // namespace

void write_golden(const std::string& path, const GoldenRecord& record) {
    const std::size_t query_count = record.fixture.queries.size();
    if (record.output.positions.size() != query_count ||
        record.output.normals.size() != query_count ||
        record.output.status.size() != query_count) {
        throw std::invalid_argument("output array counts must match query count");
    }

    std::vector<std::uint8_t> payload;
    payload.insert(payload.end(), record.fixture.metadata_json.begin(),
                   record.fixture.metadata_json.end());
    append_vec3(payload, record.fixture.samples);
    append_vec3(payload, record.fixture.queries);
    append_vec3(payload, record.output.positions);
    append_vec3(payload, record.output.normals);
    for (PointStatus status : record.output.status) {
        append_u32(payload, static_cast<std::uint32_t>(status));
    }

    std::vector<std::uint8_t> header;
    header.insert(header.end(), std::begin(kMagic), std::end(kMagic));
    append_u32(header, kVersion);
    append_u32(header, kHeaderBytes);
    append_u32(header, kEndianMarker);
    append_u32(header, kAllSections);
    append_u64(header, record.fixture.samples.size());
    append_u64(header, query_count);
    append_u32(header, record.fixture.iterations);
    append_u32(header, 0);
    append_f32(header, record.fixture.radius);
    append_f32(header, record.fixture.cell_origin.x);
    append_f32(header, record.fixture.cell_origin.y);
    append_f32(header, record.fixture.cell_origin.z);
    append_u64(header, record.fixture.metadata_json.size());
    append_u64(header, record.fixture.samples.size() * 12u);
    append_u64(header, query_count * 12u);
    append_u64(header, query_count * 12u);
    append_u64(header, query_count * 12u);
    append_u64(header, query_count * 4u);
    append_u64(header, fnv1a(payload.data(), payload.size()));
    append_u64(header, static_cast<std::uint64_t>(kHeaderBytes) + payload.size());
    if (header.size() != kHeaderBytes) {
        throw std::logic_error("internal golden header size mismatch");
    }
    header.insert(header.end(), payload.begin(), payload.end());
    write_bytes(path, header);
}

GoldenRecord read_golden(const std::string& path) {
    const std::vector<std::uint8_t> bytes = read_bytes(path);
    if (bytes.size() < kHeaderBytes ||
        !std::equal(std::begin(kMagic), std::end(kMagic), bytes.begin())) {
        throw std::runtime_error("not an MLSGOLD file: " + path);
    }
    if (get_u32(bytes, 8) != kVersion || get_u32(bytes, 12) != kHeaderBytes ||
        get_u32(bytes, 16) != kEndianMarker || get_u32(bytes, 20) != kAllSections) {
        throw std::runtime_error("unsupported MLSGOLD header");
    }
    const std::uint64_t sample_count = get_u64(bytes, 24);
    const std::uint64_t query_count = get_u64(bytes, 32);
    const std::uint64_t metadata_bytes = get_u64(bytes, 64);
    const std::uint64_t sample_bytes = get_u64(bytes, 72);
    const std::uint64_t query_bytes = get_u64(bytes, 80);
    const std::uint64_t position_bytes = get_u64(bytes, 88);
    const std::uint64_t normal_bytes = get_u64(bytes, 96);
    const std::uint64_t status_bytes = get_u64(bytes, 104);
    const std::uint64_t expected_checksum = get_u64(bytes, 112);
    const std::uint64_t declared_file_bytes = get_u64(bytes, 120);
    if (declared_file_bytes != bytes.size()) {
        throw std::runtime_error("MLSGOLD file length mismatch");
    }
    const std::uint64_t payload_bytes = metadata_bytes + sample_bytes + query_bytes +
                                        position_bytes + normal_bytes + status_bytes;
    if (payload_bytes != bytes.size() - kHeaderBytes) {
        throw std::runtime_error("MLSGOLD section lengths do not sum to file length");
    }
    if (fnv1a(bytes.data() + kHeaderBytes, bytes.size() - kHeaderBytes) !=
        expected_checksum) {
        throw std::runtime_error("MLSGOLD payload checksum mismatch");
    }
    if (metadata_bytes > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("metadata too large for host");
    }

    GoldenRecord record;
    record.fixture.iterations = get_u32(bytes, 40);
    record.fixture.radius = get_f32(bytes, 48);
    record.fixture.cell_origin = {get_f32(bytes, 52), get_f32(bytes, 56),
                                  get_f32(bytes, 60)};
    std::size_t offset = kHeaderBytes;
    record.fixture.metadata_json.assign(
        reinterpret_cast<const char*>(bytes.data() + offset),
        static_cast<std::size_t>(metadata_bytes));
    offset += static_cast<std::size_t>(metadata_bytes);
    record.fixture.samples = extract_vec3(bytes, offset, sample_bytes, sample_count);
    record.fixture.queries = extract_vec3(bytes, offset, query_bytes, query_count);
    record.output.positions = extract_vec3(bytes, offset, position_bytes, query_count);
    record.output.normals = extract_vec3(bytes, offset, normal_bytes, query_count);
    if (status_bytes != query_count * 4u || status_bytes > bytes.size() - offset) {
        throw std::runtime_error("status section size does not match query count");
    }
    record.output.status.reserve(static_cast<std::size_t>(query_count));
    for (std::uint64_t index = 0; index < query_count; ++index) {
        const std::uint32_t raw_status = get_u32(bytes, offset);
        if (raw_status > static_cast<std::uint32_t>(PointStatus::numerical_failure)) {
            throw std::runtime_error("unknown point status in MLSGOLD file");
        }
        record.output.status.push_back(static_cast<PointStatus>(raw_status));
        offset += 4;
    }
    if (offset != bytes.size()) {
        throw std::runtime_error("unconsumed bytes at end of MLSGOLD file");
    }

    const std::string marker = "\"family\":\"";
    const std::size_t family_start = record.fixture.metadata_json.find(marker);
    if (family_start != std::string::npos) {
        const std::size_t value_start = family_start + marker.size();
        const std::size_t value_end = record.fixture.metadata_json.find('"', value_start);
        if (value_end != std::string::npos) {
            record.fixture.family =
                record.fixture.metadata_json.substr(value_start, value_end - value_start);
        }
    }
    return record;
}

void write_raw_vec3(const std::string& path, const std::vector<Vec3>& values) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 12u);
    append_vec3(bytes, values);
    write_bytes(path, bytes);
}

std::vector<Vec3> read_raw_vec3(const std::string& path, std::size_t count) {
    const std::vector<std::uint8_t> bytes = read_bytes(path);
    if (bytes.size() != count * 12u) {
        throw std::runtime_error("raw FP32 triple file has wrong byte count: " + path);
    }
    std::size_t offset = 0;
    return extract_vec3(bytes, offset, bytes.size(), count);
}

}  // namespace cleanroom_mls
