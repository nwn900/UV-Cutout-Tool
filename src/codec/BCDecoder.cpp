#include "BCDecoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uvc::codec {

namespace {

inline uint8_t scale_5_to_8(uint32_t v) { return static_cast<uint8_t>((v * 255u + 15u) / 31u); }
inline uint8_t scale_6_to_8(uint32_t v) { return static_cast<uint8_t>((v * 255u + 31u) / 63u); }
inline uint8_t float01_to_8(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(v * 255.0f));
}

inline uint16_t rd_u16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t rd_u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline uint64_t rd_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (8 * i);
    return v;
}

struct BitReader {
    const uint8_t* data;
    int offset = 0;
    uint32_t read(int n) {
        uint32_t out = 0;
        int written = 0;
        while (written < n) {
            int byte_bit = offset & 7;
            int byte_idx = offset >> 3;
            int take = std::min(n - written, 8 - byte_bit);
            uint32_t bits = (uint32_t(data[byte_idx]) >> byte_bit) & ((1u << take) - 1u);
            out |= bits << written;
            written += take;
            offset += take;
        }
        return out;
    }
};

} // namespace

static void unpack_bc1_colors(const uint8_t* block, Block4x4& out, bool allow_transparent_index) {
    uint16_t low  = rd_u16(block + 0);
    uint16_t high = rd_u16(block + 2);
    uint32_t sel  = rd_u32(block + 4);

    uint32_t r0_5 = (low  >> 11) & 31;
    uint32_t g0_6 = (low  >> 5)  & 63;
    uint32_t b0_5 = low         & 31;
    uint32_t r1_5 = (high >> 11) & 31;
    uint32_t g1_6 = (high >> 5)  & 63;
    uint32_t b1_5 = high        & 31;

    const float rf0 = float(r0_5) / 31.0f;
    const float gf0 = float(g0_6) / 63.0f;
    const float bf0 = float(b0_5) / 31.0f;
    const float rf1 = float(r1_5) / 31.0f;
    const float gf1 = float(g1_6) / 63.0f;
    const float bf1 = float(b1_5) / 31.0f;

    uint8_t r0 = float01_to_8(rf0);
    uint8_t g0 = float01_to_8(gf0);
    uint8_t b0 = float01_to_8(bf0);
    uint8_t r1 = float01_to_8(rf1);
    uint8_t g1 = float01_to_8(gf1);
    uint8_t b1 = float01_to_8(bf1);

    uint8_t colors[4][4];
    colors[0][0] = r0; colors[0][1] = g0; colors[0][2] = b0; colors[0][3] = 255;
    colors[1][0] = r1; colors[1][1] = g1; colors[1][2] = b1; colors[1][3] = 255;

    if (!allow_transparent_index || low > high) {
        colors[2][0] = float01_to_8((rf0 * 2.0f + rf1) * (1.0f / 3.0f));
        colors[2][1] = float01_to_8((gf0 * 2.0f + gf1) * (1.0f / 3.0f));
        colors[2][2] = float01_to_8((bf0 * 2.0f + bf1) * (1.0f / 3.0f));
        colors[2][3] = 255;
        colors[3][0] = float01_to_8((rf0 + rf1 * 2.0f) * (1.0f / 3.0f));
        colors[3][1] = float01_to_8((gf0 + gf1 * 2.0f) * (1.0f / 3.0f));
        colors[3][2] = float01_to_8((bf0 + bf1 * 2.0f) * (1.0f / 3.0f));
        colors[3][3] = 255;
    } else {
        colors[2][0] = float01_to_8((rf0 + rf1) * 0.5f);
        colors[2][1] = float01_to_8((gf0 + gf1) * 0.5f);
        colors[2][2] = float01_to_8((bf0 + bf1) * 0.5f);
        colors[2][3] = 255;
        colors[3][0] = 0; colors[3][1] = 0; colors[3][2] = 0; colors[3][3] = 0;
    }

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            uint32_t s = (sel >> ((y * 4 + x) * 2)) & 3u;
            out[y][x][0] = colors[s][0];
            out[y][x][1] = colors[s][1];
            out[y][x][2] = colors[s][2];
            out[y][x][3] = colors[s][3];
        }
    }
}

void unpack_bc1_block(const uint8_t* block, Block4x4& out) {
    unpack_bc1_colors(block, out, true);
}

void unpack_bc2_block(const uint8_t* block, Block4x4& out) {
    const uint64_t alpha = rd_u64(block);
    unpack_bc1_colors(block + 8, out, false);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const uint32_t a4 = uint32_t((alpha >> ((y * 4 + x) * 4)) & 0xFu);
            out[y][x][3] = uint8_t((a4 << 4) | a4);
        }
    }
}

static void unpack_bc4_channel(const uint8_t* block, uint8_t out[4][4]) {
    uint8_t a0 = block[0];
    uint8_t a1 = block[1];
    uint8_t alpha[8];
    alpha[0] = a0;
    alpha[1] = a1;
    if (a0 > a1) {
        for (int i = 0; i < 6; ++i)
            alpha[i + 2] = uint8_t(((a0 * (6 - i) + a1 * (i + 1)) + 3) / 7);
    } else {
        for (int i = 0; i < 4; ++i)
            alpha[i + 2] = uint8_t(((a0 * (4 - i) + a1 * (i + 1)) + 2) / 5);
        alpha[6] = 0;
        alpha[7] = 255;
    }
    uint64_t sel = rd_u64(block) >> 16;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            uint32_t s = uint32_t((sel >> ((y * 4 + x) * 3)) & 7u);
            out[y][x] = alpha[s];
        }
    }
}

void unpack_bc4_block(const uint8_t* block, Block4x4& out) {
    uint8_t ch[4][4];
    unpack_bc4_channel(block, ch);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            out[y][x][0] = ch[y][x];
            out[y][x][1] = ch[y][x];
            out[y][x][2] = ch[y][x];
            out[y][x][3] = 255;
        }
}

void unpack_bc3_block(const uint8_t* block, Block4x4& out) {
    uint8_t alpha[4][4];
    unpack_bc4_channel(block, alpha);
    unpack_bc1_colors(block + 8, out, false);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            out[y][x][3] = alpha[y][x];
}

void unpack_bc5_block(const uint8_t* block, Block4x4& out) {
    uint8_t red[4][4];
    uint8_t green[4][4];
    unpack_bc4_channel(block + 0, red);
    unpack_bc4_channel(block + 8, green);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            out[y][x][0] = red[y][x];
            out[y][x][1] = green[y][x];
            out[y][x][2] = 0;
            out[y][x][3] = 255;
        }
}

// ──────────────────────────────────────────────────────────────────────────
//  BC7
// ──────────────────────────────────────────────────────────────────────────

static const int bc7_weights2[4]  = {0, 21, 43, 64};
static const int bc7_weights3[8]  = {0, 9, 18, 27, 37, 46, 55, 64};
static const int bc7_weights4[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

static const uint8_t bc7_partition3[64 * 16] = {
    0,0,1,1,0,0,1,1,0,2,2,1,2,2,2,2, 0,0,0,1,0,0,1,1,2,2,1,1,2,2,2,1,
    0,0,0,0,2,0,0,1,2,2,1,1,2,2,1,1, 0,2,2,2,0,0,2,2,0,0,1,1,0,1,1,1,
    0,0,0,0,0,0,0,0,1,1,2,2,1,1,2,2, 0,0,1,1,0,0,1,1,0,0,2,2,0,0,2,2,
    0,0,2,2,0,0,2,2,1,1,1,1,1,1,1,1, 0,0,1,1,0,0,1,1,2,2,1,1,2,2,1,1,
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2, 0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,
    0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2, 0,0,1,2,0,0,1,2,0,0,1,2,0,0,1,2,
    0,1,1,2,0,1,1,2,0,1,1,2,0,1,1,2, 0,1,2,2,0,1,2,2,0,1,2,2,0,1,2,2,
    0,0,1,1,0,1,1,2,1,1,2,2,1,2,2,2, 0,0,1,1,2,0,0,1,2,2,0,0,2,2,2,0,
    0,0,0,1,0,0,1,1,0,1,1,2,1,1,2,2, 0,1,1,1,0,0,1,1,2,0,0,1,2,2,0,0,
    0,0,0,0,1,1,2,2,1,1,2,2,1,1,2,2, 0,0,2,2,0,0,2,2,0,0,2,2,1,1,1,1,
    0,1,1,1,0,1,1,1,0,2,2,2,0,2,2,2, 0,0,0,1,0,0,0,1,2,2,2,1,2,2,2,1,
    0,0,0,0,0,0,1,1,0,1,2,2,0,1,2,2, 0,0,0,0,1,1,0,0,2,2,1,0,2,2,1,0,
    0,1,2,2,0,1,2,2,0,0,1,1,0,0,0,0, 0,0,1,2,0,0,1,2,1,1,2,2,2,2,2,2,
    0,1,1,0,1,2,2,1,1,2,2,1,0,1,1,0, 0,0,0,0,0,1,1,0,1,2,2,1,1,2,2,1,
    0,0,2,2,1,1,0,2,1,1,0,2,0,0,2,2, 0,1,1,0,0,1,1,0,2,0,0,2,2,2,2,2,
    0,0,1,1,0,1,2,2,0,1,2,2,0,0,1,1, 0,0,0,0,2,0,0,0,2,2,1,1,2,2,2,1,
    0,0,0,0,0,0,0,2,1,1,2,2,1,2,2,2, 0,2,2,2,0,0,2,2,0,0,1,2,0,0,1,1,
    0,0,1,1,0,0,1,2,0,0,2,2,0,2,2,2, 0,1,2,0,0,1,2,0,0,1,2,0,0,1,2,0,
    0,0,0,0,1,1,1,1,2,2,2,2,0,0,0,0, 0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,
    0,1,2,0,2,0,1,2,1,2,0,1,0,1,2,0, 0,0,1,1,2,2,0,0,1,1,2,2,0,0,1,1,
    0,0,1,1,1,1,2,2,2,2,0,0,0,0,1,1, 0,1,0,1,0,1,0,1,2,2,2,2,2,2,2,2,
    0,0,0,0,0,0,0,0,2,1,2,1,2,1,2,1, 0,0,2,2,1,1,2,2,0,0,2,2,1,1,2,2,
    0,0,2,2,0,0,1,1,0,0,2,2,0,0,1,1, 0,2,2,0,1,2,2,1,0,2,2,0,1,2,2,1,
    0,1,0,1,2,2,2,2,2,2,2,2,0,1,0,1, 0,0,0,0,2,1,2,1,2,1,2,1,2,1,2,1,
    0,1,0,1,0,1,0,1,0,1,0,1,2,2,2,2, 0,2,2,2,0,1,1,1,0,2,2,2,0,1,1,1,
    0,0,0,2,1,1,1,2,0,0,0,2,1,1,1,2, 0,0,0,0,2,1,1,2,2,1,1,2,2,1,1,2,
    0,2,2,2,0,1,1,1,0,1,1,1,0,2,2,2, 0,0,0,2,1,1,1,2,1,1,1,2,0,0,0,2,
    0,1,1,0,0,1,1,0,0,1,1,0,2,2,2,2, 0,0,0,0,0,0,0,0,2,1,1,2,2,1,1,2,
    0,1,1,0,0,1,1,0,2,2,2,2,2,2,2,2, 0,0,2,2,0,0,1,1,0,0,1,1,0,0,2,2,
    0,0,2,2,1,1,2,2,1,1,2,2,0,0,2,2, 0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,2,
    0,0,0,2,0,0,0,1,0,0,0,2,0,0,0,1, 0,2,2,2,1,2,2,2,0,2,2,2,1,2,2,2,
    0,1,0,1,2,2,2,2,2,2,2,2,2,2,2,2, 0,1,1,1,2,0,1,1,2,2,0,1,2,2,2,0,
};

static const uint8_t bc7_partition2[64 * 16] = {
    0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1, 0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,
    0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1, 0,0,0,1,0,0,1,1,0,0,1,1,0,1,1,1,
    0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1, 0,0,1,1,0,1,1,1,0,1,1,1,1,1,1,1,
    0,0,0,1,0,0,1,1,0,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,1,0,0,1,1,0,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1, 0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,1,0,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,
    0,0,0,1,0,1,1,1,1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
    0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,
    0,0,0,0,1,0,0,0,1,1,1,0,1,1,1,1, 0,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,0, 0,1,1,1,0,0,1,1,0,0,0,1,0,0,0,0,
    0,0,1,1,0,0,0,1,0,0,0,0,0,0,0,0, 0,0,0,0,1,0,0,0,1,1,0,0,1,1,1,0,
    0,0,0,0,0,0,0,0,1,0,0,0,1,1,0,0, 0,1,1,1,0,0,1,1,0,0,1,1,0,0,0,1,
    0,0,1,1,0,0,0,1,0,0,0,1,0,0,0,0, 0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,
    0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0, 0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,
    0,0,0,1,0,1,1,1,1,1,1,0,1,0,0,0, 0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,
    0,1,1,1,0,0,0,1,1,0,0,0,1,1,1,0, 0,0,1,1,1,0,0,1,1,0,0,1,1,1,0,0,
    0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1, 0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,
    0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0, 0,0,1,1,0,0,1,1,1,1,0,0,1,1,0,0,
    0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0, 0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0,
    0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1, 0,1,0,1,1,0,1,0,1,0,1,0,0,1,0,1,
    0,1,1,1,0,0,1,1,1,1,0,0,1,1,1,0, 0,0,0,1,0,0,1,1,1,1,0,0,1,0,0,0,
    0,0,1,1,0,0,1,0,0,1,0,0,1,1,0,0, 0,0,1,1,1,0,1,1,1,1,0,1,1,1,0,0,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0, 0,0,1,1,1,1,0,0,1,1,0,0,0,0,1,1,
    0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1, 0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,
    0,1,0,0,1,1,1,0,0,1,0,0,0,0,0,0, 0,0,1,0,0,1,1,1,0,0,1,0,0,0,0,0,
    0,0,0,0,0,0,1,0,0,1,1,1,0,0,1,0, 0,0,0,0,0,1,0,0,1,1,1,0,0,1,0,0,
    0,1,1,0,1,1,0,0,1,0,0,1,0,0,1,1, 0,0,1,1,0,1,1,0,1,1,0,0,1,0,0,1,
    0,1,1,0,0,0,1,1,1,0,0,1,1,1,0,0, 0,0,1,1,1,0,0,1,1,1,0,0,0,1,1,0,
    0,1,1,0,1,1,0,0,1,1,0,0,1,0,0,1, 0,1,1,0,0,0,1,1,0,0,1,1,1,0,0,1,
    0,1,1,1,1,1,1,0,1,0,0,0,0,0,0,1, 0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,1,
    0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1, 0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,
    0,0,1,0,0,0,1,0,1,1,1,0,1,1,1,0, 0,1,0,0,0,1,0,0,0,1,1,1,0,1,1,1,
};

static const uint8_t bc7_anchor_second[64] = {
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15, 2, 8, 2, 2, 8, 8,15, 2, 8, 2, 2, 8, 8, 2, 2,
    15,15, 6, 8, 2, 8,15,15, 2, 8, 2, 2, 2,15,15, 6,
     6, 2, 6, 8,15,15, 2, 2,15,15,15,15,15, 2, 2,15
};

static const uint8_t bc7_anchor_third_1[64] = {
     3, 3,15,15, 8, 3,15,15, 8, 8, 6, 6, 6, 5, 3, 3,
     3, 3, 8,15, 3, 3, 6,10, 5, 8, 8, 6, 8, 5,15,15,
     8,15, 3, 5, 6,10, 8,15,15, 3,15, 5,15,15,15,15,
     3,15, 5, 5, 5, 8, 5,10, 5,10, 8,13,15,12, 3, 3
};

static const uint8_t bc7_anchor_third_2[64] = {
    15, 8, 8, 3,15,15, 3, 8,15,15,15,15,15,15,15, 8,
    15, 8,15, 3,15, 8,15, 8, 3,15, 6,10,15,15,10, 8,
    15, 3,15,10,10, 8, 9,10, 6,15, 8,15, 3, 6, 6, 8,
    15, 3,15,15,15,15,15,15,15,15,15,15, 3,15,15, 8
};

static inline int bc7_dequant(int val, int pbit, int val_bits) {
    int total_bits = val_bits + 1;
    val = (val << 1) | pbit;
    val <<= (8 - total_bits);
    val |= (val >> total_bits);
    return val;
}

static inline int bc7_dequant_no_pbit(int val, int val_bits) {
    val <<= (8 - val_bits);
    val |= (val >> val_bits);
    return val;
}

template <size_t EndpointCount, size_t ComponentCount>
static inline void bc7_apply_unique_pbits(int (&endpoints)[EndpointCount][ComponentCount],
                                          const int* pbits,
                                          int endpoint_bits) {
    for (size_t e = 0; e < EndpointCount; ++e)
        for (size_t c = 0; c < ComponentCount; ++c)
            endpoints[e][c] = bc7_dequant(endpoints[e][c], pbits[e], endpoint_bits);
}

template <size_t EndpointCount, size_t ComponentCount>
static inline void bc7_apply_shared_pbits(int (&endpoints)[EndpointCount][ComponentCount],
                                          const int* pbits,
                                          int endpoint_bits) {
    for (size_t e = 0; e < EndpointCount; ++e)
        for (size_t c = 0; c < ComponentCount; ++c)
            endpoints[e][c] = bc7_dequant(endpoints[e][c], pbits[e >> 1], endpoint_bits);
}

template <size_t EndpointCount, size_t ComponentCount>
static inline void bc7_dequantize_all_components(int (&endpoints)[EndpointCount][ComponentCount],
                                                 int endpoint_bits) {
    for (size_t e = 0; e < EndpointCount; ++e)
        for (size_t c = 0; c < ComponentCount; ++c)
            endpoints[e][c] = bc7_dequant_no_pbit(endpoints[e][c], endpoint_bits);
}

template <size_t EndpointCount>
static inline void bc7_dequantize_rgb_alpha_components(int (&endpoints)[EndpointCount][4],
                                                       int rgb_bits,
                                                       int alpha_bits) {
    for (size_t e = 0; e < EndpointCount; ++e) {
        for (int c = 0; c < 3; ++c)
            endpoints[e][c] = bc7_dequant_no_pbit(endpoints[e][c], rgb_bits);
        endpoints[e][3] = bc7_dequant_no_pbit(endpoints[e][3], alpha_bits);
    }
}

static inline int bc7_separate_color_endpoint_bits(int mode) {
    return (mode == 4) ? 5 : 7;
}

static inline int bc7_separate_alpha_endpoint_bits(int mode) {
    return 6;
}

static inline int bc7_separate_color_index_bits(int mode, int index_mode) {
    if (mode == 4) return index_mode ? 3 : 2;
    return 2;
}

static inline int bc7_separate_alpha_index_bits(int mode, int index_mode) {
    if (mode == 4) return index_mode ? 2 : 3;
    return 2;
}

static inline int bc7_partitioned_color_endpoint_bits(int mode) {
    switch (mode) {
        case 0: return 4;
        case 1: return 6;
        case 2: return 5;
        case 3: return 7;
        case 7: return 5;
        default: return 0;
    }
}

static inline int bc7_partitioned_index_bits(int mode) {
    return (mode == 0 || mode == 1) ? 3 : 2;
}

static inline bool bc7_partitioned_has_shared_pbits(int mode) {
    return mode == 1;
}

static inline int bc7_partitioned_pbit_count(int mode) {
    switch (mode) {
        case 0: return 6;
        case 1: return 2;
        case 2: return 0;
        case 3: return 4;
        case 7: return 4;
        default: return 0;
    }
}

static inline int bc7_combined_endpoint_bits(int mode) {
    return (mode == 6) ? 7 : 5;
}

static inline int bc7_combined_index_bits(int mode) {
    return (mode == 6) ? 4 : 2;
}

static inline int bc7_combined_pbit_count(int mode) {
    return (mode == 6) ? 2 : 4;
}

static inline int bc7_partition_bit_count(int mode) {
    return (mode == 0) ? 4 : 6;
}

static inline void bc7_fill_fixups_2subset(int (&fixups)[16], int partition) {
    fixups[0] = 0;
    fixups[1] = bc7_anchor_second[partition];
    for (int i = 2; i < 16; ++i) fixups[i] = -1;
}

static inline void bc7_fill_fixups_3subset(int (&fixups)[16], int partition) {
    fixups[0] = 0;
    fixups[1] = bc7_anchor_third_1[partition];
    fixups[2] = bc7_anchor_third_2[partition];
    for (int i = 3; i < 16; ++i) fixups[i] = -1;
}

static inline int bc7_partition_subset_2(int partition, int texel) {
    return bc7_partition2[partition * 16 + texel];
}

static inline int bc7_partition_subset_3(int partition, int texel) {
    return bc7_partition3[partition * 16 + texel];
}

static inline int bc7_read_index(BitReader& br, int bit_count, bool is_fixup) {
    return int(br.read(is_fixup ? (bit_count - 1) : bit_count));
}

template <size_t Count>
static inline void bc7_read_index_stream(BitReader& br, int* out, int bit_count, const int (&fixups)[Count]) {
    for (size_t i = 0; i < Count; ++i) {
        bool is_fixup = false;
        for (int fixup : fixups) {
            if (fixup >= 0 && int(i) == fixup) {
                is_fixup = true;
                break;
            }
        }
        out[i] = bc7_read_index(br, bit_count, is_fixup);
    }
}

static inline void bc7_read_single_subset_index_stream(BitReader& br,
                                                       int* out,
                                                       int bit_count,
                                                       bool use_fixup_index) {
    for (int i = 0; i < 16; ++i)
        out[i] = int(br.read((use_fixup_index && i == 0) ? (bit_count - 1) : bit_count));
}

static inline int bc7_interp(int l, int h, int w, int bits) {
    const int* weights = (bits == 2) ? bc7_weights2 : (bits == 3) ? bc7_weights3 : bc7_weights4;
    int wv = weights[w];
    int iwv = 64 - wv;
    return (l * iwv + h * wv + 32) >> 6;
}

template <size_t PaletteSize>
static inline void bc7_build_palette(uint8_t (&palette)[PaletteSize][4],
                                     const int* ep0,
                                     const int* ep1,
                                     int components,
                                     int index_bits,
                                     bool opaque_alpha_when_missing) {
    const int palette_size = 1 << index_bits;
    for (int i = 0; i < palette_size; ++i) {
        for (int c = 0; c < components; ++c)
            palette[i][c] = uint8_t(std::clamp(bc7_interp(ep0[c], ep1[c], i, index_bits), 0, 255));
        if (opaque_alpha_when_missing && components < 4)
            palette[i][3] = 255;
    }
}

static inline void fill_zero(Block4x4& p, uint8_t alpha = 255) {
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            p[y][x][0] = 0; p[y][x][1] = 0; p[y][x][2] = 0; p[y][x][3] = alpha;
        }
}

void unpack_bc7_block(const uint8_t* block, Block4x4& pixels) {
    fill_zero(pixels, 0);

    uint8_t first = block[0];
    int mode = -1;
    for (int i = 0; i < 8; ++i) {
        if (first & (1 << i)) { mode = i; break; }
    }
    if (mode < 0) return;

    BitReader br{block, 0};
    uint32_t mode_bits = br.read(mode + 1);
    if (int(mode_bits) != (1 << mode)) return;

    if (mode == 0 || mode == 2) {
        int endpoint_bits = bc7_partitioned_color_endpoint_bits(mode);
        int weight_bits   = bc7_partitioned_index_bits(mode);
        int pbits_count   = bc7_partitioned_pbit_count(mode);
        int partition = br.read(bc7_partition_bit_count(mode));

        int endpoints[6][3] = {};
        for (int c = 0; c < 3; ++c)
            for (int e = 0; e < 6; ++e)
                endpoints[e][c] = br.read(endpoint_bits);

        int pb[6] = {};
        for (int i = 0; i < pbits_count; ++i) pb[i] = br.read(1);
        if (pbits_count) {
            bc7_apply_unique_pbits(endpoints, pb, endpoint_bits);
        } else {
            bc7_dequantize_all_components(endpoints, endpoint_bits);
        }

        int num_weights = 1 << weight_bits;
        uint8_t block_colors[3][16][4] = {};
        for (int s = 0; s < 3; ++s)
            bc7_build_palette(block_colors[s], endpoints[s*2], endpoints[s*2+1], 3, weight_bits, true);

        int fixups[16] = {};
        bc7_fill_fixups_3subset(fixups, partition);
        int weights[16] = {};
        bc7_read_index_stream(br, weights, weight_bits, fixups);
        for (int i = 0; i < 16; ++i) {
            int x = i & 3, y = i >> 2;
            int s = bc7_partition_subset_3(partition, i);
            for (int c = 0; c < 4; ++c) pixels[y][x][c] = block_colors[s][weights[i]][c];
        }
    }
    else if (mode == 1 || mode == 3 || mode == 7) {
        int comps        = (mode == 7) ? 4 : 3;
        int weight_bits  = (mode == 7) ? bc7_combined_index_bits(mode) : bc7_partitioned_index_bits(mode);
        int endpoint_bits= (mode == 7) ? bc7_combined_endpoint_bits(mode) : bc7_partitioned_color_endpoint_bits(mode);
        int num_pbits    = (mode == 7) ? bc7_combined_pbit_count(mode) : bc7_partitioned_pbit_count(mode);
        bool shared_pbits= bc7_partitioned_has_shared_pbits(mode);

        int partition = br.read(bc7_partition_bit_count(mode));
        int endpoints[4][4] = {};
        for (int c = 0; c < comps; ++c)
            for (int e = 0; e < 4; ++e)
                endpoints[e][c] = br.read(endpoint_bits);

        int pb[4] = {};
        for (int i = 0; i < num_pbits; ++i) pb[i] = br.read(1);
        if (shared_pbits) bc7_apply_shared_pbits(endpoints, pb, endpoint_bits);
        else              bc7_apply_unique_pbits(endpoints, pb, endpoint_bits);

        int num_weights = 1 << weight_bits;
        uint8_t block_colors[2][16][4] = {};
        for (int s = 0; s < 2; ++s)
            bc7_build_palette(block_colors[s], endpoints[s*2], endpoints[s*2+1], comps, weight_bits, comps == 3);

        int fixups[16] = {};
        bc7_fill_fixups_2subset(fixups, partition);
        int weights[16] = {};
        bc7_read_index_stream(br, weights, weight_bits, fixups);
        for (int i = 0; i < 16; ++i) {
            int x = i & 3, y = i >> 2;
            int s = bc7_partition_subset_2(partition, i);
            for (int c = 0; c < 4; ++c) pixels[y][x][c] = block_colors[s][weights[i]][c];
        }
    }
    else if (mode == 4 || mode == 5) {
        int rgb_bits = bc7_separate_color_endpoint_bits(mode);
        int a_bits   = bc7_separate_alpha_endpoint_bits(mode);

        int comp_rot = br.read(2);
        int index_mode = (mode == 4) ? br.read(1) : 0;
        const int color_wb = bc7_separate_color_index_bits(mode, index_mode);
        const int alpha_wb = bc7_separate_alpha_index_bits(mode, index_mode);

        int endpoints[2][4] = {};
        for (int c = 0; c < 3; ++c)
            for (int e = 0; e < 2; ++e)
                endpoints[e][c] = br.read(rgb_bits);
        for (int e = 0; e < 2; ++e) endpoints[e][3] = br.read(a_bits);
        bc7_dequantize_rgb_alpha_components(endpoints, rgb_bits, a_bits);

        int color_weights[16] = {};
        int alpha_weights[16] = {};
        if (mode == 4) {
            int idx2[16] = {};
            int idx3[16] = {};
            bc7_read_single_subset_index_stream(br, idx2, 2, index_mode == 0);
            bc7_read_single_subset_index_stream(br, idx3, 3, index_mode != 0);
            for (int i = 0; i < 16; ++i) {
                color_weights[i] = index_mode ? idx3[i] : idx2[i];
                alpha_weights[i] = index_mode ? idx2[i] : idx3[i];
            }
        } else {
            bc7_read_single_subset_index_stream(br, color_weights, color_wb, true);
            bc7_read_single_subset_index_stream(br, alpha_weights, alpha_wb, false);
        }

        uint8_t block_colors[16][3] = {};
        uint8_t color_palette[16][4] = {};
        uint8_t alpha_palette[16][4] = {};
        bc7_build_palette(color_palette, endpoints[0], endpoints[1], 3, color_wb, false);
        bc7_build_palette(alpha_palette, endpoints[0] + 3, endpoints[1] + 3, 1, alpha_wb, false);
        for (int i = 0; i < (1 << color_wb); ++i)
            for (int ch = 0; ch < 3; ++ch)
                block_colors[i][ch] = color_palette[i][ch];
        uint8_t a_colors[16] = {};
        for (int i = 0; i < (1 << alpha_wb); ++i)
            a_colors[i] = alpha_palette[i][0];

        for (int i = 0; i < 16; ++i) {
            int x = i & 3, y = i >> 2;
            uint8_t c[4] = { block_colors[color_weights[i]][0], block_colors[color_weights[i]][1],
                             block_colors[color_weights[i]][2], 255 };
            c[3] = a_colors[alpha_weights[i]];
            if (comp_rot >= 1) std::swap(c[3], c[comp_rot - 1]);
            for (int ch = 0; ch < 4; ++ch) pixels[y][x][ch] = c[ch];
        }
    }
    else if (mode == 6) {
        int endpoint_bits = bc7_combined_endpoint_bits(mode);
        int weight_bits   = bc7_combined_index_bits(mode);
        int pbit_count    = bc7_combined_pbit_count(mode);

        int ep[2][4] = {};
        for (int ch = 0; ch < 4; ++ch)
            for (int e = 0; e < 2; ++e)
                ep[e][ch] = int(br.read(endpoint_bits));

        int pb[2] = {};
        for (int i = 0; i < pbit_count; ++i) pb[i] = int(br.read(1));
        bc7_apply_unique_pbits(ep, pb, endpoint_bits);

        uint8_t color_vals[16][4] = {};
        bc7_build_palette(color_vals, ep[0], ep[1], 4, weight_bits, false);

        int s[16] = {};
        bc7_read_single_subset_index_stream(br, s, weight_bits, true);

        static const int order_x[16] = {0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3};
        static const int order_y[16] = {0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3};
        for (int idx = 0; idx < 16; ++idx) {
            int x = order_x[idx], y = order_y[idx];
            for (int ch = 0; ch < 4; ++ch) pixels[y][x][ch] = color_vals[s[idx]][ch];
        }
    }
}

bool decode_dds_block(BCFormat fmt, const uint8_t* block, Block4x4& out) {
    switch (fmt) {
        case BCFormat::BC1: unpack_bc1_block(block, out); return true;
        case BCFormat::BC2: unpack_bc2_block(block, out); return true;
        case BCFormat::BC3: unpack_bc3_block(block, out); return true;
        case BCFormat::BC4: unpack_bc4_block(block, out); return true;
        case BCFormat::BC5: unpack_bc5_block(block, out); return true;
        case BCFormat::BC7: unpack_bc7_block(block, out); return true;
        default: return false;
    }
}

} // namespace uvc::codec
