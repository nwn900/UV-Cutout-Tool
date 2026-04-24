#include "TGAIO.h"

#include <QFile>
#include <vector>

namespace uvc::codec {

bool write_tga(const QString& path, const QImage& image) {
    QImage img = image.convertToFormat(QImage::Format_RGBA8888);
    const int w = img.width();
    const int h = img.height();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;

    uint8_t hdr[18] = {};
    hdr[2]  = 2;                              // uncompressed true-color
    hdr[12] = uint8_t(w & 0xFF);
    hdr[13] = uint8_t((w >> 8) & 0xFF);
    hdr[14] = uint8_t(h & 0xFF);
    hdr[15] = uint8_t((h >> 8) & 0xFF);
    hdr[16] = 32;                             // bits per pixel
    hdr[17] = 0x28;                           // top-left origin, alpha=8

    if (f.write(reinterpret_cast<const char*>(hdr), sizeof(hdr)) != sizeof(hdr)) return false;

    // TGA stores BGRA. Python wrote `px[:, :, [2, 1, 0, 3]].tobytes()`.
    std::vector<uint8_t> row(size_t(w) * 4);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = img.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            row[x * 4 + 0] = src[x * 4 + 2];
            row[x * 4 + 1] = src[x * 4 + 1];
            row[x * 4 + 2] = src[x * 4 + 0];
            row[x * 4 + 3] = src[x * 4 + 3];
        }
        if (f.write(reinterpret_cast<const char*>(row.data()), qint64(row.size())) != qint64(row.size()))
            return false;
    }
    return true;
}

} // namespace uvc::codec
