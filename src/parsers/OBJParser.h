#pragma once

#include "../geometry/MeshData.h"
#include <QString>
#include <vector>

namespace uvc::parsers {

class OBJParser {
public:
    std::vector<geom::Mesh> parse(const QString& filepath);
};

} // namespace uvc::parsers
