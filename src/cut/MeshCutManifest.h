#pragma once

#include "../geometry/MeshData.h"

#include <QJsonDocument>
#include <QString>

#include <vector>

namespace uvc::cut {

QJsonDocument buildMeshCutManifest(const std::vector<geom::Mesh>& meshes,
                                   const QString& selection_mode = QStringLiteral("remove"));

bool writeMeshCutManifest(const QString& path,
                          const std::vector<geom::Mesh>& meshes,
                          const QString& selection_mode = QStringLiteral("remove"),
                          QString* error = nullptr);

} // namespace uvc::cut
