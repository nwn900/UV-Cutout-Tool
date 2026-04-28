#include "HeadlessRunner.h"

#include "../cut/MeshCutManifest.h"
#include "../parsers/NiflyParser.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace uvc::headless {

namespace {

QString default_blender_script_path() {
    return QStringLiteral("C:/Users/micha/Desktop/Blender mesh editor/scripts/apply_uvcut_manifest.py");
}

QString default_blender_path() {
    return QStringLiteral("C:/Users/micha/Desktop/Blender mesh editor/tools/blender/blender.exe");
}

float parse_float(const QString& value, const char* what) {
    bool ok = false;
    const float result = value.toFloat(&ok);
    if (!ok) throw std::runtime_error(QString("Invalid %1: %2").arg(what, value).toStdString());
    return result;
}

bool contains_ci(const std::string& haystack, const QString& needle) {
    if (needle.trimmed().isEmpty()) return true;
    const QString hay = QString::fromStdString(haystack);
    return hay.contains(needle, Qt::CaseInsensitive);
}

bool tri_in_bounds(const uvc::geom::Triangle& t,
                   float x_abs_min, float x_abs_max,
                   float y_min, float y_max,
                   float z_min, float z_max) {
    float sx = 0.f, sy = 0.f, sz = 0.f;
    for (const auto& v : t.verts) {
        sx += v.x;
        sy += v.y;
        sz += v.z;
    }
    const float cx = sx / 3.f;
    const float cy = sy / 3.f;
    const float cz = sz / 3.f;
    const float ax = std::fabs(cx);
    return ax >= x_abs_min && ax <= x_abs_max
        && cy >= y_min && cy <= y_max
        && cz >= z_min && cz <= z_max;
}

int run_apply_manifest_mode(const QCommandLineParser& parser) {
    const QString manifest = QDir::toNativeSeparators(parser.value(QStringLiteral("manifest")));
    const QString output_root = QDir::toNativeSeparators(parser.value(QStringLiteral("output-root")));
    const QString blender = QDir::toNativeSeparators(parser.value(QStringLiteral("blender")));
    const QString script = QDir::toNativeSeparators(parser.value(QStringLiteral("blender-script")));
    const QString game = parser.value(QStringLiteral("game")).trimmed().isEmpty()
        ? QStringLiteral("SKYRIMSE")
        : parser.value(QStringLiteral("game")).trimmed();

    if (!QFileInfo::exists(blender)) {
        QTextStream(stderr) << "Blender executable not found: " << blender << "\n";
        return 3;
    }
    if (!QFileInfo::exists(script)) {
        QTextStream(stderr) << "Blender script not found: " << script << "\n";
        return 3;
    }
    if (!QFileInfo::exists(manifest)) {
        QTextStream(stderr) << "Manifest not found: " << manifest << "\n";
        return 3;
    }

    QStringList blender_args;
    blender_args
        << QStringLiteral("--background")
        << QStringLiteral("--python")
        << script
        << QStringLiteral("--")
        << QStringLiteral("--manifest")
        << manifest
        << QStringLiteral("--output-root")
        << output_root
        << QStringLiteral("--game")
        << game;

    QTextStream(stdout)
        << "Running headless mesh apply via Blender...\n"
        << "  blender: " << blender << "\n"
        << "  script:  " << script << "\n"
        << "  manifest:" << manifest << "\n"
        << "  output:  " << output_root << "\n";

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start(blender, blender_args);
    if (!proc.waitForStarted()) {
        QTextStream(stderr) << "Failed to start Blender process.\n";
        return 4;
    }
    proc.waitForFinished(-1);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QTextStream(stderr) << "Blender process failed with exit code "
                            << proc.exitCode() << ".\n";
        return proc.exitCode() != 0 ? proc.exitCode() : 5;
    }

    QTextStream(stdout) << "Headless apply completed successfully.\n";
    return 0;
}

int run_build_manifest_mode(const QCommandLineParser& parser) {
    const QString input_nif = QDir::toNativeSeparators(parser.value(QStringLiteral("input-nif")));
    const QString manifest_out = QDir::toNativeSeparators(parser.value(QStringLiteral("manifest-out")));
    const QString mesh_filter = parser.value(QStringLiteral("mesh-name-contains")).trimmed();

    if (!QFileInfo::exists(input_nif)) {
        QTextStream(stderr) << "Input NIF not found: " << input_nif << "\n";
        return 3;
    }
    if (manifest_out.isEmpty()) {
        QTextStream(stderr) << "Missing --manifest-out path.\n";
        return 2;
    }

    try {
        const float x_abs_min = parse_float(parser.value(QStringLiteral("x-abs-min")), "x-abs-min");
        const float x_abs_max = parse_float(parser.value(QStringLiteral("x-abs-max")), "x-abs-max");
        const float y_min = parse_float(parser.value(QStringLiteral("y-min")), "y-min");
        const float y_max = parse_float(parser.value(QStringLiteral("y-max")), "y-max");
        const float z_min = parse_float(parser.value(QStringLiteral("z-min")), "z-min");
        const float z_max = parse_float(parser.value(QStringLiteral("z-max")), "z-max");
        if (x_abs_min > x_abs_max || y_min > y_max || z_min > z_max) {
            QTextStream(stderr) << "Invalid bounds: min must be <= max.\n";
            return 2;
        }

        parsers::NiflyParser p;
        auto meshes = p.parse(input_nif, {});
        int selected = 0;
        int touched_meshes = 0;
        for (auto& mesh : meshes) {
            if (!contains_ci(mesh.name, mesh_filter)) continue;
            bool touched_this_mesh = false;
            for (auto& tri : mesh.triangles) {
                const bool hit = tri_in_bounds(tri, x_abs_min, x_abs_max, y_min, y_max, z_min, z_max);
                if (hit) {
                    tri.selected = true;
                    ++selected;
                    touched_this_mesh = true;
                }
            }
            if (touched_this_mesh) ++touched_meshes;
            mesh.source_name = QFileInfo(input_nif).fileName().toStdString();
            mesh.source_path = input_nif.toStdString();
        }

        if (selected == 0) {
            QTextStream(stderr) << "No triangles selected with provided bounds/filter.\n";
            return 6;
        }

        QString error;
        if (!cut::writeMeshCutManifest(manifest_out, meshes, QStringLiteral("remove"), &error)) {
            QTextStream(stderr) << "Manifest write failed: " << error << "\n";
            return 7;
        }

        QTextStream(stdout)
            << "Manifest built.\n"
            << "  input: " << input_nif << "\n"
            << "  output:" << manifest_out << "\n"
            << "  meshes:" << touched_meshes << "\n"
            << "  tris:  " << selected << "\n";
        return 0;
    } catch (const std::exception& ex) {
        QTextStream(stderr) << "Build manifest failed: " << ex.what() << "\n";
        return 8;
    }
}

} // namespace

bool wantsHeadlessMode(const QStringList& args) {
    return args.contains(QStringLiteral("--headless-apply-manifest"))
        || args.contains(QStringLiteral("--headless-build-manifest"))
        || args.contains(QStringLiteral("--headless"));
}

int runHeadless(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("UV Cutout Tool");
    QCoreApplication::setApplicationVersion("headless");

    QCommandLineParser parser;
    parser.setApplicationDescription("UV Cutout Tool headless runner");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("headless-apply-manifest")},
        QStringLiteral("Apply a .uvcut.json manifest to source NIFs using Blender/PyNifly.")));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("headless-build-manifest")},
        QStringLiteral("Build a .uvcut.json manifest from one NIF using triangle center bounds.")));

    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("manifest")},
        QStringLiteral("Path to input manifest JSON file."), QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("output-root")},
        QStringLiteral("Output root folder for patched NIFs."), QStringLiteral("dir")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("blender")},
        QStringLiteral("Path to blender executable."), QStringLiteral("exe"), default_blender_path()));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("blender-script")},
        QStringLiteral("Path to apply_uvcut_manifest.py script."), QStringLiteral("file"), default_blender_script_path()));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("game")},
        QStringLiteral("Target game (passed through to apply mode)."), QStringLiteral("name"), QStringLiteral("SKYRIMSE")));

    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("input-nif")},
        QStringLiteral("Input NIF for manifest builder mode."), QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("manifest-out")},
        QStringLiteral("Output manifest path for builder mode."), QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("mesh-name-contains")},
        QStringLiteral("Optional mesh name substring filter."), QStringLiteral("text"), QStringLiteral("HIMBO")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("x-abs-min")},
        QStringLiteral("Min abs(center.x) bound."), QStringLiteral("num"), QStringLiteral("8.0")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("x-abs-max")},
        QStringLiteral("Max abs(center.x) bound."), QStringLiteral("num"), QStringLiteral("18.5")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("y-min")},
        QStringLiteral("Min center.y bound."), QStringLiteral("num"), QStringLiteral("-2.5")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("y-max")},
        QStringLiteral("Max center.y bound."), QStringLiteral("num"), QStringLiteral("8.75")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("z-min")},
        QStringLiteral("Min center.z bound."), QStringLiteral("num"), QStringLiteral("76.5")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("z-max")},
        QStringLiteral("Max center.z bound."), QStringLiteral("num"), QStringLiteral("91.5")));

    parser.process(app);

    if (parser.isSet(QStringLiteral("headless-apply-manifest"))) return run_apply_manifest_mode(parser);
    if (parser.isSet(QStringLiteral("headless-build-manifest"))) return run_build_manifest_mode(parser);

    QTextStream(stderr)
        << "No headless mode selected. Use --headless-build-manifest or --headless-apply-manifest.\n";
    return 2;
}

} // namespace uvc::headless

