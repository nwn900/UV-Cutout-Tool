#pragma once

#include <QImage>
#include <QString>

namespace uvc::codec {

// Write an uncompressed 32-bit RGBA TGA with a top-left origin.
bool write_tga(const QString& path, const QImage& image);

} // namespace uvc::codec
