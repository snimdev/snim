#ifndef CORE_SELFTEST_H
#define CORE_SELFTEST_H

#include <ostream>

namespace Core::SelfTest {

// Bundle smoke checks for `snim --self-test`: the Qt plugins a deployed build needs,
// bundled tessdata when present, and the optional libraries compiled in. Prints one
// line per check and returns the process exit code (0 when every check passed).
// Needs a live QApplication; creates no tray icon or window.
int run(std::ostream &out);

} // namespace Core::SelfTest

#endif // CORE_SELFTEST_H
