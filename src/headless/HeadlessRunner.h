#pragma once

#include <QStringList>

namespace uvc::headless {

bool wantsHeadlessMode(const QStringList& args);
int runHeadless(int argc, char** argv);

} // namespace uvc::headless

