#include "core/UpdateCheck.h"

#include "core/Version.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QList>
#include <QPointer>

#include <algorithm>
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

struct Parsed {
    Triple triple;
    QStringView prerelease;   // empty for a plain release, "alpha.1" for v1.0.0-alpha.1
};

bool isHexDigit(QChar c)
{
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
}

// git describe appends -<commits>-g<hash>, plus -dirty for a modified tree, to the tag it
// counts from. That tail names a build, not a version, so it is dropped before comparing.
QStringView stripDescribeSuffix(QStringView version)
{
    QStringView head = version;
    if (head.endsWith(QLatin1String("-dirty")))
        head = head.first(head.size() - 6);

    const qsizetype hash = head.lastIndexOf(u'-');
    if (hash < 0 || head.size() - hash < 3 || head.at(hash + 1) != u'g')
        return version;
    for (qsizetype i = hash + 2; i < head.size(); ++i) {
        if (!isHexDigit(head.at(i)))
            return version;
    }

    const QStringView counted = head.first(hash);
    const qsizetype dash = counted.lastIndexOf(u'-');
    if (dash < 0 || dash + 1 == counted.size())
        return version;
    for (qsizetype i = dash + 1; i < counted.size(); ++i) {
        if (!counted.at(i).isDigit())
            return version;
    }

    return counted.first(dash);
}

// Reads the leading X.Y.Z plus the prerelease behind it; anything else (build metadata,
// a fourth component, plain garbage) is ignored, so an unparsable head is 0.0.0.
Parsed parseVersion(QStringView version)
{
    if (!version.isEmpty() && (version.front() == u'v' || version.front() == u'V'))
        version = version.sliced(1);
    version = stripDescribeSuffix(version);

    Parsed out;
    int *fields[] = { &out.triple.major, &out.triple.minor, &out.triple.patch };
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

    if (i < version.size() && version.at(i) == u'-') {
        const QStringView tail = version.sliced(i + 1);
        // Build metadata (+sha) carries no precedence, so it never reaches the comparison.
        const qsizetype plus = tail.indexOf(u'+');
        out.prerelease = plus < 0 ? tail : tail.first(plus);
    }

    return out;
}

// Semver precedence for the tail: a release outranks every prerelease of the same X.Y.Z,
// and two prereleases compare identifier by identifier, numeric pairs numerically.
int comparePrerelease(QStringView left, QStringView right)
{
    if (left.isEmpty() || right.isEmpty()) {
        if (left.isEmpty() && right.isEmpty())
            return 0;
        return left.isEmpty() ? 1 : -1;
    }

    const QList<QStringView> a = left.split(u'.');
    const QList<QStringView> b = right.split(u'.');

    for (qsizetype i = 0; i < std::min(a.size(), b.size()); ++i) {
        bool leftNumeric = false;
        bool rightNumeric = false;
        const qulonglong leftValue = a.at(i).toULongLong(&leftNumeric);
        const qulonglong rightValue = b.at(i).toULongLong(&rightNumeric);

        if (leftNumeric && rightNumeric) {
            if (leftValue != rightValue)
                return leftValue < rightValue ? -1 : 1;
            continue;
        }
        if (a.at(i) != b.at(i))
            return a.at(i) < b.at(i) ? -1 : 1;
    }

    // Equal as far as the shorter one goes: the longer identifier list is the later one.
    if (a.size() != b.size())
        return a.size() < b.size() ? -1 : 1;
    return 0;
}

QString translated(const char *text)
{
    return QCoreApplication::translate("Core::UpdateCheck", text);
}

} // namespace

int compareVersions(QStringView left, QStringView right)
{
    const Parsed a = parseVersion(left);
    const Parsed b = parseVersion(right);

    if (a.triple.major != b.triple.major)
        return a.triple.major < b.triple.major ? -1 : 1;
    if (a.triple.minor != b.triple.minor)
        return a.triple.minor < b.triple.minor ? -1 : 1;
    if (a.triple.patch != b.triple.patch)
        return a.triple.patch < b.triple.patch ? -1 : 1;
    return comparePrerelease(a.prerelease, b.prerelease);
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
