#include "TGAIO.h"

#include <QFile>
#include <cstring>
#include <vector>

namespace uvc::codec {

namespace {

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
