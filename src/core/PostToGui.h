#ifndef CORE_POSTTOGUI_H
#define CORE_POSTTOGUI_H

#include <QCoreApplication>
#include <QMetaObject>

#include <utility>

namespace Core {

// Every worker -> GUI hop goes through here: qApp is the context object, so fn runs on the
// GUI thread. During shutdown qApp can already be gone, and dropping the post is correct.
template <typename F>
void postToGui(F &&fn)
{
    if (QCoreApplication *app = QCoreApplication::instance())
        QMetaObject::invokeMethod(app, std::forward<F>(fn), Qt::QueuedConnection);
}

} // namespace Core

#endif // CORE_POSTTOGUI_H
