#include "recording/strategies/LinuxRecorderModule.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLibrary>

#include <memory>

namespace Recording::LinuxRecorderModule {

namespace {

using CreateFunction = RecordingStrategy *(*)(QObject *);

QString moduleFileName()
{
    return QStringLiteral("lib") + QLatin1String(kBaseName) + QStringLiteral(".so");
}

QFunctionPointer entryPoint()
{
    // Loaded once and never unloaded: the strategies it hands out outlive their callers.
    // A failed load is not cached, so a module installed later is still picked up.
    static QLibrary library;

    if (!library.isLoaded()) {
        const QStringList candidates = candidatePaths(QCoreApplication::applicationDirPath());
        for (const QString &candidate : candidates) {
            library.setFileName(candidate);
            // Resolve everything now: a lazily bound symbol would crash mid-recording
            // instead of falling back to the stub here.
            library.setLoadHints(QLibrary::ResolveAllSymbolsHint);
            if (library.load())
                break;
        }
        if (!library.isLoaded()) {
            qWarning() << "Linux recorder module not loaded:" << library.errorString();
            return nullptr;
        }
        qDebug() << "Loaded the Linux recorder module from" << library.fileName();
    }

    const QFunctionPointer entry = library.resolve(kEntryPoint);
    if (!entry)
        qWarning() << "Linux recorder module exports no" << kEntryPoint << ":" << library.errorString();
    return entry;
}

} // namespace

QStringList candidatePaths(const QString &binDir)
{
    if (qEnvironmentVariableIsSet("SNIM_RECORDER_MODULE"))
        return {qEnvironmentVariable("SNIM_RECORDER_MODULE")};

    const QString fileName = moduleFileName();
    QStringList paths;
    if (!binDir.isEmpty()) {
        paths << binDir + QLatin1Char('/') + fileName
              << QDir::cleanPath(binDir + QStringLiteral("/../lib/") + fileName)
              << QDir::cleanPath(binDir + QStringLiteral("/../lib64/") + fileName);
    }
    paths << QString::fromLatin1(kBaseName);
    return paths;
}

RecordingStrategy *create(QObject *parent)
{
    const QFunctionPointer entry = entryPoint();
    if (!entry)
        return nullptr;
    return reinterpret_cast<CreateFunction>(entry)(parent);
}

bool isAvailable()
{
    const std::unique_ptr<RecordingStrategy> probe(create());
    return probe && probe->isAvailable();
}

} // namespace Recording::LinuxRecorderModule
