#pragma once

#include <QImage>
#include <QString>

namespace uvc::codec {

// Write a TGA file.
// rle:        use run-length encoding (image type 10) instead of uncompressed (type 2).
// with_alpha: write 32-bit RGBA; when false writes 24-bit RGB (transparent pixels
//             must already be composited to an opaque background before calling).
bool write_tga(const QString& path, const QImage& image,
               bool rle = false, bool with_alpha = true);

} // namespace uvc::codec
