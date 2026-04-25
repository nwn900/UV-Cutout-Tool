#pragma once

#include "../geometry/MeshData.h"
#include <QString>
#include <functional>
#include <memory>
#include <vector>

namespace uvc::parsers {

class NiflyParser {
public:
    using ProgressCallback = std::function<void(float fraction, const QString& message)>;

    NiflyParser();
    ~NiflyParser();

    // Dynamically loads NiflyDLL.dll from the portable app folder and parses the NIF.
    // Throws std::runtime_error on failure.
    std::vector<geom::Mesh> parse(const QString& filepath, ProgressCallback cb = {});

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace uvc::parsers
