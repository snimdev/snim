#include "core/DynamicModule.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHash>
#include <QLibrary>
#include <QMutex>

namespace Core {

namespace {

QLibrary *libraryFor(const char *baseName)
{
    // One per module, loaded once and never unloaded: the objects it hands out outlive
    // their callers.
    static QHash<QByteArray, QLibrary *> libraries;
    QLibrary *&library = libraries[QByteArray(baseName)];
    if (!library)
        library = new QLibrary;
    return library;
}

} // namespace

QStringList DynamicModule::candidatePaths(const QString &binDir) const
{
    if (qEnvironmentVariableIsSet(envOverride))
        return {qEnvironmentVariable(envOverride)};

    const QString fileName = QStringLiteral("lib") + QLatin1String(baseName)
                             + QStringLiteral(".so");
    QStringList paths;
    if (!binDir.isEmpty()) {
        paths << binDir + QLatin1Char('/') + fileName
              << QDir::cleanPath(binDir + QStringLiteral("/../lib/") + fileName)
              << QDir::cleanPath(binDir + QStringLiteral("/../lib64/") + fileName);
    }
    paths << QString::fromLatin1(baseName);
    return paths;
}

QFunctionPointer DynamicModule::resolve() const
{
    static QMutex mutex;
    const QMutexLocker locker(&mutex);

    // A failed load is not cached, so a module installed later is still picked up.
    QLibrary *library = libraryFor(baseName);
    if (!library->isLoaded()) {
        const QStringList candidates = candidatePaths(QCoreApplication::applicationDirPath());
        for (const QString &candidate : candidates) {
            library->setFileName(candidate);
            // Resolve everything now: a lazily bound symbol would crash mid-use instead
            // of falling back to the stub here.
            library->setLoadHints(QLibrary::ResolveAllSymbolsHint);
            if (library->load())
                break;
        }
        if (!library->isLoaded()) {
            qWarning() << "Module" << baseName << "not loaded:" << library->errorString();
            return nullptr;
        }
        qDebug() << "Loaded module" << baseName << "from" << library->fileName();
    }

    const QFunctionPointer function = library->resolve(entry);
    if (!function)
        qWarning() << "Module" << baseName << "exports no" << entry << ":" << library->errorString();
    return function;
}

} // namespace Core
