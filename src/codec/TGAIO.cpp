#include "TGAIO.h"

#include <QByteArray>
#include <QFile>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace uvc::codec {

namespace {

uint16_t read_le16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

void require_bytes(const QByteArray& bytes, qsizetype pos, qsizetype count,
                   const char* message) {
    if (pos < 0 || count < 0 || pos + count > bytes.size())
        throw std::runtime_error(message);
}

void store_tga_pixel(QImage& img, int dst_x, int dst_y, const uint8_t* src,
                     int src_bpp) {
    uint8_t* dst = img.scanLine(dst_y) + dst_x * 4;
    dst[0] = src[2];                      // R
    dst[1] = src[1];                      // G
    dst[2] = src[0];                      // B
    dst[3] = (src_bpp == 4) ? src[3] : 255; // A
}

// Encode one row of already-BGRA (or BGR) pixels into TGA RLE packets.
// Packets never cross scanline boundaries.
void encode_rle_row(const uint8_t* row, int pixel_count, int bpp,
                    std::vector<uint8_t>& out) {
    int pos = 0;
    while (pos < pixel_count) {
        const uint8_t* cur = row + pos * bpp;

        // Count run of identical pixels (max 128).
        int run = 1;
        while (run < 128 && pos + run < pixel_count &&
               std::memcmp(cur, row + (pos + run) * bpp, bpp) == 0)
            ++run;

        if (run >= 2) {
            // RLE packet: header byte has bit-7 set; run count = value + 1.
            out.push_back(uint8_t(0x80 | (run - 1)));
            out.insert(out.end(), cur, cur + bpp);
            pos += run;
        } else {
            // Raw packet: gather pixels until a run of ≥3 identical pixels begins.
            int raw = 1;
            while (raw < 128 && pos + raw < pixel_count) {
                const uint8_t* p1 = row + (pos + raw - 1) * bpp;
                const uint8_t* p2 = row + (pos + raw) * bpp;
                // Peek one further to detect an upcoming 2+ run.
                bool next_run = (pos + raw + 1 < pixel_count) &&
                    std::memcmp(p2, row + (pos + raw + 1) * bpp, bpp) == 0;
                if (std::memcmp(p1, p2, bpp) == 0 && next_run)
                    break;
                ++raw;
            }
            out.push_back(uint8_t(raw - 1));
            for (int i = 0; i < raw; ++i)
                out.insert(out.end(), row + (pos + i) * bpp,
                           row + (pos + i) * bpp + bpp);
            pos += raw;
        }
    }
}

} // namespace

QImage load_tga_image(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error("Could not open TGA file");

    const QByteArray bytes = f.readAll();
    if (bytes.size() < 18)
        throw std::runtime_error("TGA file is too small");

    const auto* hdr = reinterpret_cast<const uint8_t*>(bytes.constData());
    const int id_len = hdr[0];
    const int color_map_type = hdr[1];
    const int image_type = hdr[2];
    const uint16_t color_map_len = read_le16(hdr + 5);
    const int color_map_entry_bits = hdr[7];
    const int w = int(read_le16(hdr + 12));
    const int h = int(read_le16(hdr + 14));
    const int bits = hdr[16];
    const int desc = hdr[17];

    if (w <= 0 || h <= 0)
        throw std::runtime_error("TGA has invalid dimensions");
    if (w > 32768 || h > 32768 || qint64(w) * qint64(h) > 2147483647)
        throw std::runtime_error("TGA dimensions are too large");
    if (color_map_type != 0 || color_map_len != 0 || color_map_entry_bits != 0)
        throw std::runtime_error("Color-mapped TGA files are not supported");
    if (image_type != 2 && image_type != 10)
        throw std::runtime_error("Only true-color TGA files are supported");
    if (bits != 24 && bits != 32)
        throw std::runtime_error("Only 24-bit and 32-bit TGA files are supported");

    const int src_bpp = bits / 8;
    qsizetype pos = 18 + id_len;
    require_bytes(bytes, pos, 0, "TGA header is truncated");

    QImage img(w, h, QImage::Format_RGBA8888);
    if (img.isNull())
        throw std::runtime_error("Could not allocate TGA image");

    const bool top_origin = (desc & 0x20) != 0;
    const bool right_origin = (desc & 0x10) != 0;

    auto write_pixel_index = [&](int pixel_index, const uint8_t* px) {
        const int file_x = pixel_index % w;
        const int file_y = pixel_index / w;
        const int dst_x = right_origin ? (w - 1 - file_x) : file_x;
        const int dst_y = top_origin ? file_y : (h - 1 - file_y);
        store_tga_pixel(img, dst_x, dst_y, px, src_bpp);
    };

    const int pixel_count = w * h;
    if (image_type == 2) {
        require_bytes(bytes, pos, qsizetype(pixel_count) * src_bpp,
                      "TGA pixel data is truncated");
        const auto* src = reinterpret_cast<const uint8_t*>(bytes.constData() + pos);
        for (int i = 0; i < pixel_count; ++i)
            write_pixel_index(i, src + i * src_bpp);
    } else {
        int out_i = 0;
        while (out_i < pixel_count) {
            require_bytes(bytes, pos, 1, "TGA RLE data is truncated");
            const uint8_t packet = uint8_t(bytes.at(pos++));
            const int count = (packet & 0x7F) + 1;
            if (out_i + count > pixel_count)
                throw std::runtime_error("TGA RLE packet overruns image");

            if (packet & 0x80) {
                require_bytes(bytes, pos, src_bpp, "TGA RLE run is truncated");
                const auto* px = reinterpret_cast<const uint8_t*>(bytes.constData() + pos);
                pos += src_bpp;
                for (int i = 0; i < count; ++i)
                    write_pixel_index(out_i++, px);
            } else {
                require_bytes(bytes, pos, qsizetype(count) * src_bpp,
                              "TGA RLE raw packet is truncated");
                const auto* px = reinterpret_cast<const uint8_t*>(bytes.constData() + pos);
                for (int i = 0; i < count; ++i)
                    write_pixel_index(out_i++, px + i * src_bpp);
                pos += qsizetype(count) * src_bpp;
            }
        }
    }

    return img;
}

bool write_tga(const QString& path, const QImage& src_image,
               bool rle, bool with_alpha) {
    const int bpp = with_alpha ? 4 : 3;
    QImage img = with_alpha
        ? src_image.convertToFormat(QImage::Format_RGBA8888)
        : src_image.convertToFormat(QImage::Format_RGB888);

    const int w = img.width();
    const int h = img.height();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;

    // 18-byte TGA header.
    uint8_t hdr[18] = {};
    hdr[2]  = rle ? 10 : 2;            // image type: 2=uncompressed RGB, 10=RLE RGB
    hdr[12] = uint8_t(w & 0xFF);
    hdr[13] = uint8_t((w >> 8) & 0xFF);
    hdr[14] = uint8_t(h & 0xFF);
    hdr[15] = uint8_t((h >> 8) & 0xFF);
    hdr[16] = uint8_t(bpp * 8);        // bits per pixel: 32 or 24
    // Image descriptor: bit5=top-left origin, bits3-0=alpha channel depth.
    hdr[17] = with_alpha ? 0x28 : 0x20;

    if (f.write(reinterpret_cast<const char*>(hdr), 18) != 18) return false;

    if (rle) {
        // RLE path: encode each scanline.
        std::vector<uint8_t> bgr_row(size_t(w) * bpp);
        std::vector<uint8_t> rle_buf;
        rle_buf.reserve(size_t(w) * bpp * 2);

        for (int y = 0; y < h; ++y) {
            const uint8_t* src = img.constScanLine(y);
            // Convert RGBA→BGRA or RGB→BGR.
            for (int x = 0; x < w; ++x) {
                bgr_row[x * bpp + 0] = src[x * bpp + 2]; // B
                bgr_row[x * bpp + 1] = src[x * bpp + 1]; // G
                bgr_row[x * bpp + 2] = src[x * bpp + 0]; // R
                if (bpp == 4) bgr_row[x * bpp + 3] = src[x * bpp + 3]; // A
            }
            rle_buf.clear();
            encode_rle_row(bgr_row.data(), w, bpp, rle_buf);
            if (f.write(reinterpret_cast<const char*>(rle_buf.data()),
                        qint64(rle_buf.size())) != qint64(rle_buf.size()))
                return false;
        }
    } else {
        // Uncompressed path.
        std::vector<uint8_t> row(size_t(w) * bpp);
        for (int y = 0; y < h; ++y) {
            const uint8_t* src = img.constScanLine(y);
            for (int x = 0; x < w; ++x) {
                row[x * bpp + 0] = src[x * bpp + 2]; // B
                row[x * bpp + 1] = src[x * bpp + 1]; // G
                row[x * bpp + 2] = src[x * bpp + 0]; // R
                if (bpp == 4) row[x * bpp + 3] = src[x * bpp + 3]; // A
            }
            if (f.write(reinterpret_cast<const char*>(row.data()),
                        qint64(row.size())) != qint64(row.size()))
                return false;
        }
    }
    return true;
}

} // namespace uvc::codec
