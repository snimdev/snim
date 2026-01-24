#ifndef CORE_UPDATECHECK_H
#define CORE_UPDATECHECK_H

#include <QByteArray>
#include <QString>
#include <QStringView>
#include <QUrl>

#include <functional>

class QObject;

/**
 * "Check for updates" against the GitHub releases API.
 *
 * The network call is a single-shot async query, like the Uploader contract: the
 * callback fires exactly once, on the GUI thread, and is dropped when the context
 * object died first. Parsing and version comparison are pure free functions so the
 * tests can cover them without touching the network.
 */
namespace Core::UpdateCheck {

struct Result {
    bool ok = false;      // the query itself succeeded (a repo with no releases still counts)
    QString error;        // failure reason, or an informational note when ok is true
    QString latestTag;    // the release tag, e.g. v1.2.0; empty when there are no releases
    QUrl releaseUrl;      // the release's page on GitHub
    bool newer = false;   // latestTag is newer than the running build
};

// Reads tag_name + html_url out of a GitHub "latest release" payload and compares the
// tag against this build. Garbage in gives ok = false, never a crash.
[[nodiscard]] Result parseLatestRelease(const QByteArray &json);

// -1, 0 or +1 for left older than, equal to, or newer than right. Tolerates a leading v,
// and a git describe tail (-4-gabc1234, with or without -dirty) compares as the tag it
// counts from; an unparsable head compares as 0.0.0. Prereleases follow semver precedence:
// 1.0.0-alpha.2 < 1.0.0-alpha.10 < 1.0.0-beta.1 < 1.0.0.
[[nodiscard]] int compareVersions(QStringView left, QStringView right);

[[nodiscard]] bool isNewer(QStringView candidate, QStringView current);

// GETs the latest release and delivers the outcome to onDone on the GUI thread.
void checkLatest(QObject *context, std::function<void(Result)> onDone);

} // namespace Core::UpdateCheck

#endif // CORE_UPDATECHECK_H
