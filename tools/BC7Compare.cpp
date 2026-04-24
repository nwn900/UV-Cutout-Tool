#include "codec/BCDecoder.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using uvc::codec::Block4x4;

std::string trim(std::string s) {
    size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) ++first;
    size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) --last;
    return s.substr(first, last - first);
}

std::string strip_separators(std::string s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (std::isxdigit(static_cast<unsigned char>(ch))) out.push_back(ch);
    }
    return out;
}

std::array<uint8_t, 16> parse_block_hex(const std::string& text) {
    const std::string hex = strip_separators(text);
    if (hex.size() != 32) {
        throw std::runtime_error("Expected exactly 16 bytes (32 hex chars) for a BC7 block.");
    }

    std::array<uint8_t, 16> block{};
    for (size_t i = 0; i < block.size(); ++i) {
        const std::string byte_text = hex.substr(i * 2, 2);
        block[i] = static_cast<uint8_t>(std::stoul(byte_text, nullptr, 16));
    }
    return block;
}

std::vector<std::string> load_input_lines(const char* path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to open input file.");

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (!line.empty() && line[0] == '#') continue;
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> split_fields(const std::string& line, char delim) {
    std::vector<std::string> out;
    std::string current;
    for (char ch : line) {
        if (ch == delim) {
            out.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    out.push_back(trim(current));
    return out;
}

void dump_block(std::ostream& os, const Block4x4& block) {
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const auto& px = block[y][x];
            os << std::setw(3) << int(px[0]) << ' '
               << std::setw(3) << int(px[1]) << ' '
               << std::setw(3) << int(px[2]) << ' '
               << std::setw(3) << int(px[3]);
            if (x != 3) os << "   |   ";
        }
        os << '\n';
    }
}

std::string block_signature(const Block4x4& block) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const auto& px = block[y][x];
            for (int c = 0; c < 4; ++c)
                out << std::setw(2) << int(px[c]);
        }
    }
    return out.str();
}

void decode_and_dump(const std::string& label, const std::array<uint8_t, 16>& raw_block) {
    Block4x4 decoded{};
    uvc::codec::unpack_bc7_block(raw_block.data(), decoded);

    std::cout << "=== " << label << " ===\n";
    std::cout << "Input:";
    for (uint8_t b : raw_block) {
        std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << int(b);
    }
    std::cout << std::dec << std::setfill(' ') << "\n";
    std::cout << "Signature: " << block_signature(decoded) << "\n";
    dump_block(std::cout, decoded);
    std::cout << '\n';
}

bool decode_and_compare(const std::string& label,
                        const std::array<uint8_t, 16>& raw_block,
                        const std::string& expected_signature) {
    Block4x4 decoded{};
    uvc::codec::unpack_bc7_block(raw_block.data(), decoded);
    const std::string actual = block_signature(decoded);
    const bool match = (actual == expected_signature);

    std::cout << (match ? "[PASS] " : "[FAIL] ") << label << '\n';
    std::cout << "  expected: " << expected_signature << '\n';
    std::cout << "  actual:   " << actual << '\n';
    if (!match) {
        dump_block(std::cout, decoded);
        std::cout << '\n';
    }
    return match;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr
                << "Usage:\n"
                << "  bc7-compare <32-hex-char-block>\n"
                << "  bc7-compare --file <path-to-text-file>\n\n"
                << "  bc7-compare --expect <path-to-text-file>\n\n"
                << "Examples:\n"
                << "  bc7-compare 200102030405060708090A0B0C0D0E0F\n"
                << "  bc7-compare --file bc7_blocks.txt\n"
                << "  bc7-compare --expect bc7_expected.txt\n\n"
                << "Expected-file format:\n"
                << "  label | 32-hex-char-block | 128-hex-char-rgba-signature\n";
            return 1;
        }

        if (std::string(argv[1]) == "--file") {
            if (argc < 3) throw std::runtime_error("Missing file path after --file.");
            const auto lines = load_input_lines(argv[2]);
            for (size_t i = 0; i < lines.size(); ++i) {
                decode_and_dump("block " + std::to_string(i + 1), parse_block_hex(lines[i]));
            }
            std::cout << "Decoded " << lines.size() << " block(s).\n";
            return 0;
        }

        if (std::string(argv[1]) == "--expect") {
            if (argc < 3) throw std::runtime_error("Missing file path after --expect.");
            const auto lines = load_input_lines(argv[2]);
            size_t pass_count = 0;
            size_t fail_count = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                const auto fields = split_fields(lines[i], '|');
                if (fields.size() != 3) {
                    throw std::runtime_error("Expected 3 pipe-delimited fields per line in --expect file.");
                }
                const std::string expected = strip_separators(fields[2]);
                if (expected.size() != 128) {
                    throw std::runtime_error("Expected RGBA signature must contain exactly 128 hex chars.");
                }
                const std::string label = fields[0].empty() ? ("block " + std::to_string(i + 1)) : fields[0];
                const bool ok = decode_and_compare(label, parse_block_hex(fields[1]), expected);
                if (ok) ++pass_count;
                else    ++fail_count;
            }
            std::cout << "Summary: " << pass_count << " passed, " << fail_count << " failed.\n";
            return fail_count == 0 ? 0 : 2;
        }

        decode_and_dump("block 1", parse_block_hex(argv[1]));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "bc7-compare error: " << e.what() << '\n';
        return 1;
    }
}
