#pragma once

#include <array>
#include <cstdint>

namespace uvc::codec {

// 4x4 pixel blocks. Each pixel is RGBA8 (alpha is 255 for formats lacking alpha).
using Block4x4 = std::array<std::array<std::array<uint8_t, 4>, 4>, 4>;

void unpack_bc1_block(const uint8_t* block, Block4x4& out);
void unpack_bc2_block(const uint8_t* block, Block4x4& out);
void unpack_bc3_block(const uint8_t* block, Block4x4& out);
void unpack_bc4_block(const uint8_t* block, Block4x4& out);   // single-channel -> replicated into RGB, A=255
void unpack_bc5_block(const uint8_t* block, Block4x4& out);   // RG -> R,G,0, A=255
void unpack_bc7_block(const uint8_t* block, Block4x4& out);

enum class BCFormat {
    Unknown,
    BC1,
    BC2,
    BC3,
    BC4,
    BC5,
    BC7,
};

// Dispatch helper for BC block decode into RGBA8 texels.
bool decode_dds_block(BCFormat fmt, const uint8_t* block, Block4x4& out);

} // namespace uvc::codec
