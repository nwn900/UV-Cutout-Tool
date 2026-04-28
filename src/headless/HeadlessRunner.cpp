#include "HeadlessRunner.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

namespace uvc::headless {

namespace {

QString default_blender_script_path() {
    return QStringLiteral("C:/Users/micha/Desktop/Blender mesh editor/scripts/apply_uvcut_manifest.py");
}

QString default_blender_path() {
    return QStringLiteral("C:/Users/micha/Desktop/Blender mesh editor/tools/blender/blender.exe");
}

int run_apply_manifest_mode(QCoreApplication& app) {
    QCommandLineParser parser;
    parser.setApplicationDescription("UV Cutout Tool headless runner");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption mode_opt(
        QStringList{QStringLiteral("headless-apply-manifest")},
        QStringLiteral("Apply a .uvcut.json manifest to source NIFs using Blender/PyNifly."));
    const QCommandLineOption manifest_opt(
        QStringList{QStringLiteral("manifest")},
        QStringLiteral("Path to manifest JSON file."),
        QStringLiteral("file"));
    const QCommandLineOption output_opt(
        QStringList{QStringLiteral("output-root")},
        QStringLiteral("Output root folder for patched NIFs."),
        QStringLiteral("dir"));
    const QCommandLineOption blender_opt(
        QStringList{QStringLiteral("blender")},
        QStringLiteral("Path to blender executable."),
        QStringLiteral("exe"),
        default_blender_path());
    const QCommandLineOption script_opt(
        QStringList{QStringLiteral("blender-script")},
        QStringLiteral("Path to apply_uvcut_manifest.py script."),
        QStringLiteral("file"),
        default_blender_script_path());
    const QCommandLineOption game_opt(
        QStringList{QStringLiteral("game")},
        QStringLiteral("Target game (passed through to script)."),
        QStringLiteral("name"),
        QStringLiteral("SKYRIMSE"));

    parser.addOption(mode_opt);
    parser.addOption(manifest_opt);
    parser.addOption(output_opt);
    parser.addOption(blender_opt);
    parser.addOption(script_opt);
    parser.addOption(game_opt);
    parser.process(app);

    if (!parser.isSet(mode_opt)) {
        QTextStream(stderr) << "No headless mode selected.\n";
        return 2;
    }

    if (!parser.isSet(manifest_opt) || !parser.isSet(output_opt)) {
        QTextStream(stderr)
            << "Missing required options. Use --manifest and --output-root.\n";
        return 2;
    }

    const QString manifest = QDir::toNativeSeparators(parser.value(manifest_opt));
    const QString output_root = QDir::toNativeSeparators(parser.value(output_opt));
    const QString blender = QDir::toNativeSeparators(parser.value(blender_opt));
    const QString script = QDir::toNativeSeparators(parser.value(script_opt));
    const QString game = parser.value(game_opt).trimmed().isEmpty()
        ? QStringLiteral("SKYRIMSE")
        : parser.value(game_opt).trimmed();

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

} // namespace

bool wantsHeadlessMode(const QStringList& args) {
    return args.contains(QStringLiteral("--headless-apply-manifest"))
        || args.contains(QStringLiteral("--headless"));
}

int runHeadless(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("UV Cutout Tool");
    QCoreApplication::setApplicationVersion("headless");
    return run_apply_manifest_mode(app);
}

} // namespace uvc::headless

