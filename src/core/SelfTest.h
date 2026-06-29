#ifndef CORE_SELFTEST_H
#define CORE_SELFTEST_H

#include <QList>
#include <QString>

#include <functional>
#include <ostream>

namespace Core::SelfTest {

// A check added by a layer above core, which cannot reach the dlopened modules itself.
// `run` returns whether it passed and fills in the detail either way.
struct Check {
    QString name;
    std::function<bool(QString *detail)> run;
};

// Bundle smoke checks for `snim --self-test`: the Qt plugins a deployed build needs,
// bundled tessdata when present, the optional libraries compiled in, then `extra`.
// Prints one line per check and returns the process exit code (0 when every check
// passed). Needs a live QApplication; creates no tray icon or window.
int run(std::ostream &out, const QList<Check> &extra = {});

} // namespace Core::SelfTest

#endif // CORE_SELFTEST_H
