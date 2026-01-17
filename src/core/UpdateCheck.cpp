#include "core/UpdateCheck.h"

#include "core/Version.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

#include <utility>

namespace Core::UpdateCheck {

namespace {

constexpr int kTimeoutMs = 10000;

const char kLatestReleaseUrl[] = "https://api.github.com/repos/snimdev/snim/releases/latest";

struct Triple {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

// Reads the leading X.Y.Z and stops at the first character that is not part of it, so
// a describe suffix or an -rc marker is simply ignored.
Triple parseTriple(QStringView version)
{
    if (!version.isEmpty() && (version.front() == u'v' || version.front() == u'V'))
        version = version.sliced(1);

    Triple out;
    int *fields[] = { &out.major, &out.minor, &out.patch };
    int field = 0;
    qsizetype i = 0;

    while (field < 3 && i < version.size() && version.at(i).isDigit()) {
        int value = 0;
        while (i < version.size() && version.at(i).isDigit()) {
            // Clamped so an absurdly long digit run cannot overflow.
            if (value < 1000000)
                value = value * 10 + (version.at(i).unicode() - u'0');
            ++i;
        }
        *fields[field++] = value;

        if (i < version.size() && version.at(i) == u'.') {
            ++i;
            continue;
        }
        break;
    }

    return out;
}

QString translated(const char *text)
{
    return QCoreApplication::translate("Core::UpdateCheck", text);
}

} // namespace

int compareVersions(QStringView left, QStringView right)
{
    const Triple a = parseTriple(left);
    const Triple b = parseTriple(right);

    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    return 0;
}

bool isNewer(QStringView candidate, QStringView current)
{
    return compareVersions(candidate, current) > 0;
}

Result parseLatestRelease(const QByteArray &json)
{
    Result result;

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = translated("GitHub returned a response Snim could not read.");
        return result;
    }

    const QJsonObject object = doc.object();
    const QString tag = object.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) {
        result.error = translated("GitHub's response contained no release tag.");
        return result;
    }

    result.ok = true;
    result.latestTag = tag;
    result.releaseUrl = QUrl(object.value(QStringLiteral("html_url")).toString());
    result.newer = isNewer(tag, QString::fromLatin1(Version::kVersion));
    return result;
}

void checkLatest(QObject *context, std::function<void(Result)> onDone)
{
    // One manager per check: the action is rare and this keeps no state around.
    auto *manager = new QNetworkAccessManager;

    QNetworkRequest request{QUrl(QString::fromLatin1(kLatestReleaseUrl))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    // GitHub rejects API requests that carry no User-Agent.
    request.setRawHeader("User-Agent", QByteArrayLiteral("Snim/") + Version::kVersion);
    request.setTransferTimeout(kTimeoutMs);

    QNetworkReply *reply = manager->get(request);
    const QPointer<QObject> guard(context);

    // reply is the connection's context, so the lambda runs on the thread that owns it.
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [reply, manager, guard, onDone = std::move(onDone)] {
                         const int status = reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute).toInt();

                         Result result;
                         if (reply->error() == QNetworkReply::NoError) {
                             result = parseLatestRelease(reply->readAll());
                         } else if (status == 404) {
                             // The repo simply has no published release yet.
                             result.ok = true;
                             result.error = translated("Snim has no published releases yet.");
                         } else {
                             result.error = reply->errorString();
                         }

                         reply->deleteLater();
                         manager->deleteLater();

                         if (guard && onDone)
                             onDone(result);
                     });
}

} // namespace Core::UpdateCheck
