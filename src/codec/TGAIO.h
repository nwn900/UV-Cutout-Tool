#pragma once

#include <QImage>
#include <QString>

namespace uvc::codec {

// Write an uncompressed 32-bit RGBA TGA (top-left origin, matches the Python write_tga).
bool write_tga(const QString& path, const QImage& image);

} // namespace uvc::codec
