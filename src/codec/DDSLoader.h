#pragma once

#include <QImage>
#include <QString>

namespace uvc::codec {

// Loads a DDS file (BC1/BC2/BC3/BC4/BC5/BC7, including DX10 extension) as a QImage in RGBA8888 format.
// Throws std::runtime_error on invalid / unsupported input.
QImage load_dds_image(const QString& path);

} // namespace uvc::codec
