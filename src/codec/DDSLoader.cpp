#include "DDSLoader.h"
#include "BCDecoder.h"

#include <QFile>
#include <QByteArray>

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace uvc::codec {

namespace {
constexpr int   DDS_HEADER_SIZE = 128;
constexpr int   DX10_EXT_SIZE   = 20;
constexpr uint32_t DDS_MAGIC    = 0x20534444u;
constexpr uint32_t DDSD_CAPS    = 0x1u;
constexpr uint32_t DDSD_HEIGHT  = 0x2u;
constexpr uint32_t DDSD_WIDTH   = 0x4u;
constexpr uint32_t DDSD_PIXELFORMAT = 0x1000u;
constexpr uint32_t DDSCAPS_TEXTURE = 0x1000u;
constexpr uint32_t DDPF_FOURCC  = 0x4u;
constexpr uint32_t DDPF_RGB     = 0x40u;

constexpr uint32_t FCC_DXT1  = 0x31545844u;
constexpr uint32_t FCC_DXT2  = 0x32545844u;
constexpr uint32_t FCC_DXT3  = 0x33545844u;
constexpr uint32_t FCC_DXT4  = 0x34545844u;
constexpr uint32_t FCC_DXT5  = 0x35545844u;
constexpr uint32_t FCC_DX10  = 0x30315844u;
constexpr uint32_t FCC_ATI2  = 0x32495441u;
constexpr uint32_t FCC_ATI1  = 0x31495441u;
constexpr uint32_t FCC_BC4U  = 0x55344342u;
constexpr uint32_t FCC_BC4S  = 0x53344342u;
constexpr uint32_t FCC_BC5U  = 0x55354342u;
constexpr uint32_t FCC_BC5S  = 0x53354342u;

inline uint32_t rd_u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

[[noreturn]] void fail(const char* message) {
    throw std::runtime_error(message);
}
} // namespace

QImage load_dds_image(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error(("Unable to open DDS: " + path.toStdString()));

    QByteArray raw = f.readAll();
    f.close();

    if (raw.size() < DDS_HEADER_SIZE)
        throw std::runtime_error("DDS file truncated");

    const uint8_t* hdr = reinterpret_cast<const uint8_t*>(raw.constData());
    if (rd_u32(hdr + 0) != DDS_MAGIC)
        fail("Not a valid DDS file");

    const uint32_t header_size = rd_u32(hdr + 4);
    const uint32_t flags       = rd_u32(hdr + 8);
    uint32_t height            = rd_u32(hdr + 12);
    uint32_t width             = rd_u32(hdr + 16);
    const uint32_t pitch_or_linear = rd_u32(hdr + 20);
    const uint32_t pf_size     = rd_u32(hdr + 76);
    const uint32_t pf_flags    = rd_u32(hdr + 80);
    const uint32_t fourcc      = rd_u32(hdr + 84);
    const uint32_t caps        = rd_u32(hdr + 108);

    if (header_size != 124) fail("DDS header size is invalid");
    if ((flags & (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT)) !=
        (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT)) {
        fail("DDS header flags are invalid");
    }
    if (pf_size != 32) fail("DDS pixel format header is invalid");
    if ((caps & DDSCAPS_TEXTURE) == 0) fail("DDS missing texture capability flag");
    if (width == 0 || height == 0) fail("DDS dimensions are invalid");
    if (width > 32768 || height > 32768) fail("DDS dimensions exceed supported limits");
    if ((pf_flags & DDPF_FOURCC) == 0) {
        if (pf_flags & DDPF_RGB) fail("Uncompressed DDS textures are not supported");
        fail("DDS pixel format is unsupported");
    }
    (void)pitch_or_linear;

    BCFormat fmt = BCFormat::Unknown;
    bool has_dx10 = false;

    switch (fourcc) {
        case FCC_DXT1: fmt = BCFormat::BC1; break;
        case FCC_DXT2:
        case FCC_DXT3: fmt = BCFormat::BC2; break;
        case FCC_DXT4:
        case FCC_DXT5: fmt = BCFormat::BC3; break;
        case FCC_ATI1:
        case FCC_BC4U: fmt = BCFormat::BC4; break;
        case FCC_ATI2:
        case FCC_BC5U: fmt = BCFormat::BC5; break;
        case FCC_BC4S:
        case FCC_BC5S:
            fail("Signed BC4/BC5 DDS textures are not supported");
        case FCC_DX10: {
            has_dx10 = true;
            if (raw.size() < DDS_HEADER_SIZE + DX10_EXT_SIZE)
                throw std::runtime_error("DDS DX10 header truncated");
            uint32_t dx10_format = rd_u32(hdr + DDS_HEADER_SIZE);
            switch (dx10_format) {
                case 98: fmt = BCFormat::BC7; break;
                case 99: fmt = BCFormat::BC5; break;
                case 95: fmt = BCFormat::BC4; break;
                case 97: fmt = BCFormat::BC3; break;
                case 96: fmt = BCFormat::BC1; break;
                default:
                    throw std::runtime_error("Unsupported DX10 format");
            }
            break;
        }
        default:
            throw std::runtime_error("Unsupported DDS format");
    }

    int block_size = 16;
    if (fmt == BCFormat::BC1 || fmt == BCFormat::BC4) block_size = 8;

    const qint64 data_offset = DDS_HEADER_SIZE + (has_dx10 ? DX10_EXT_SIZE : 0);
    const qint64 blocks_x = (qint64(width) + 3) / 4;
    const qint64 blocks_y = (qint64(height) + 3) / 4;
    const qint64 total_bytes = blocks_x * blocks_y * block_size;
    if (blocks_x <= 0 || blocks_y <= 0 || total_bytes <= 0)
        fail("DDS block layout is invalid");
    if (data_offset < 0 || total_bytes > std::numeric_limits<qint64>::max() - data_offset)
        fail("DDS size calculation overflowed");
    if (raw.size() < data_offset + total_bytes)
        fail("DDS pixel data truncated");

    const uint8_t* data = reinterpret_cast<const uint8_t*>(raw.constData()) + data_offset;

    QImage img(int(width), int(height), QImage::Format_RGBA8888);
    img.fill(0);

    Block4x4 block;
    for (int by = 0; by < blocks_y; ++by) {
        for (int bx = 0; bx < blocks_x; ++bx) {
            int block_offset = (by * blocks_x + bx) * block_size;
            decode_dds_block(fmt, data + block_offset, block);
            for (int y = 0; y < 4; ++y) {
                int py = by * 4 + y;
                if (py >= int(height)) continue;
                uint8_t* line = img.scanLine(py);
                for (int x = 0; x < 4; ++x) {
                    int px = bx * 4 + x;
                    if (px >= int(width)) continue;
                    uint8_t* dst = line + px * 4;
                    dst[0] = block[y][x][0];
                    dst[1] = block[y][x][1];
                    dst[2] = block[y][x][2];
                    dst[3] = block[y][x][3];
                }
            }
        }
    }
    return img;
}

} // namespace uvc::codec
