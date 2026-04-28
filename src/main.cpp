#include "ui/MainWindow.h"
#include "headless/HeadlessRunner.h"

#include <QApplication>
#include <QStringList>
#include <QSurfaceFormat>

int main(int argc, char** argv) {
    QStringList raw_args;
    for (int i = 1; i < argc; ++i) raw_args.push_back(QString::fromLocal8Bit(argv[i]));
    if (uvc::headless::wantsHeadlessMode(raw_args)) {
        return uvc::headless::runHeadless(argc, argv);
    }

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("UV Cutout Tool");

    uvc::ui::MainWindow w;
    w.show();
    return app.exec();
}
