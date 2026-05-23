#include <QtTest>

#include "core/UpdateCheck.h"
#include "core/Version.h"

using namespace Core;

// UpdateCheck's two pure helpers: the version comparison the tray action decides on,
// and the release selector that reads a GitHub /releases page. Both are deliberately free
// of network and UI, so this test never opens a socket - checkLatest() itself stays a
// manual/CI-free concern.
class tst_UpdateCheck : public QObject
{
    Q_OBJECT

private slots:
    void comparesEqualVersions();
    void comparesPatchMinorMajor();
    void toleratesPrefixesAndSuffixes();
    void ordersPrereleases();
    void treatsMalformedInputAsZero();
    void offersNewestStableToStableBuild();
    void offersNewestAlphaToAlphaBuild();
    void offersStableToAlphaBuildWhenItOutranks();
    void skipsDrafts();
    void skipsUnparsableTags();
    void reportsNoReleasesForEmptyArray();
    void ignoresTheArrayOrder();
    void rejectsGarbagePayloads();
};

namespace {

// GitHub returns the releases newest-published first, which is not version order. Every
// payload here is trimmed to the four keys the selector reads.
const QByteArray kMixedChannels = R"([
    {
        "tag_name": "v1.1.0-alpha.2",
        "html_url": "https://github.com/snimdev/snim/releases/tag/v1.1.0-alpha.2",
        "draft": false,
        "prerelease": true
    },
    {
        "tag_name": "v1.0.0",
        "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0",
        "draft": false,
        "prerelease": false
    },
    {
        "tag_name": "v1.0.0-alpha.1",
        "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0-alpha.1",
        "draft": false,
        "prerelease": true
    }
])";

} // namespace

void tst_UpdateCheck::comparesEqualVersions()
{
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.3", u"1.2.3"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"0.0.0", u"0.0.0"), 0);
    QVERIFY(!UpdateCheck::isNewer(u"1.2.3", u"1.2.3"));
}

void tst_UpdateCheck::comparesPatchMinorMajor()
{
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.4", u"1.2.3"), 1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.3", u"1.2.4"), -1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.3.0", u"1.2.9"), 1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.9", u"1.3.0"), -1);
    QCOMPARE(UpdateCheck::compareVersions(u"2.0.0", u"1.99.99"), 1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.99.99", u"2.0.0"), -1);

    QVERIFY(UpdateCheck::isNewer(u"1.2.4", u"1.2.3"));
    QVERIFY(!UpdateCheck::isNewer(u"1.2.3", u"1.2.4"));
}

void tst_UpdateCheck::toleratesPrefixesAndSuffixes()
{
    // Release tags carry the v; the running build never does.
    QCOMPARE(UpdateCheck::compareVersions(u"v1.2.3", u"1.2.3"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"V1.2.3", u"v1.2.3"), 0);
    QVERIFY(UpdateCheck::isNewer(u"v1.3.0", u"1.2.3"));

    // An untagged dev build must look older than any real release.
    QCOMPARE(UpdateCheck::compareVersions(u"0.0.0-dev", u"1.0.0"), -1);
    QVERIFY(UpdateCheck::isNewer(u"v1.0.0", u"0.0.0-dev"));

    // git describe between tags: the tail names a build, so it compares as its base tag.
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.3-4-gabc1234", u"1.2.3"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.3-4-gabc1234-dirty", u"1.2.2"), 1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-3-gabc123", u"1.0.0"), 0);
    // A prerelease marker is not a build tail: it ranks below the release itself.
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.3-rc1", u"1.2.3"), -1);
}

void tst_UpdateCheck::ordersPrereleases()
{
    // The alpha line the first public tags follow: v1.0.0-alpha.1, .2, ... then v1.0.0.
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-alpha.1", u"1.0.0-alpha.2"), -1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-alpha.2", u"1.0.0-alpha.1"), 1);
    // Numeric identifiers compare as numbers, so .10 is not "less than" .2.
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-alpha.2", u"1.0.0-alpha.10"), -1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-alpha.10", u"1.0.0-beta.1"), -1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-beta.1", u"1.0.0"), -1);
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0", u"1.0.0-beta.1"), 1);

    // Same version, one spelled as a release tag.
    QCOMPARE(UpdateCheck::compareVersions(u"v1.0.0-alpha.1", u"1.0.0-alpha.1"), 0);
    // Equal prefixes: the longer identifier list is the later prerelease.
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0-alpha", u"1.0.0-alpha.1"), -1);

    // The tray only offers an update when the release is genuinely newer.
    QVERIFY(UpdateCheck::isNewer(u"v1.0.0-alpha.2", u"1.0.0-alpha.1"));
    QVERIFY(UpdateCheck::isNewer(u"v1.0.0", u"1.0.0-alpha.9"));
    QVERIFY(!UpdateCheck::isNewer(u"v1.0.0-alpha.1", u"1.0.0"));
}

void tst_UpdateCheck::treatsMalformedInputAsZero()
{
    QCOMPARE(UpdateCheck::compareVersions(u"", u"0.0.0"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"v", u"0.0.0"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"not a version", u"0.0.0"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"...", u""), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"1.0.0", u"garbage"), 1);
    // Short and over-long forms must not read past the string.
    QCOMPARE(UpdateCheck::compareVersions(u"1", u"1.0.0"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"1.2", u"1.2.0"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"1.2.3.4.5", u"1.2.3"), 0);
    QCOMPARE(UpdateCheck::compareVersions(u"99999999999999999999.0.0", u"1.0.0"), 1);
}

void tst_UpdateCheck::offersNewestStableToStableBuild()
{
    // A stable build never sees the alpha line, even when an alpha outranks every release.
    const UpdateCheck::Result result = UpdateCheck::selectRelease(kMixedChannels, u"0.9.0");
    QVERIFY(result.ok);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.latestTag, QStringLiteral("v1.0.0"));
    QCOMPARE(result.releaseUrl,
             QUrl(QStringLiteral("https://github.com/snimdev/snim/releases/tag/v1.0.0")));
    QVERIFY(result.newer);

    // Already on the newest stable: the alpha above it still must not be offered.
    const UpdateCheck::Result current = UpdateCheck::selectRelease(kMixedChannels, u"1.0.0");
    QCOMPARE(current.latestTag, QStringLiteral("v1.0.0"));
    QVERIFY(!current.newer);

    // A describe tail names a build, not a channel: this is still a stable 1.0.0.
    const UpdateCheck::Result described =
        UpdateCheck::selectRelease(kMixedChannels, u"1.0.0-4-gabc1234");
    QCOMPARE(described.latestTag, QStringLiteral("v1.0.0"));
    QVERIFY(!described.newer);
}

void tst_UpdateCheck::offersNewestAlphaToAlphaBuild()
{
    // A build whose own version is a prerelease follows the alpha channel.
    const UpdateCheck::Result result =
        UpdateCheck::selectRelease(kMixedChannels, u"1.0.0-alpha.1");
    QVERIFY(result.ok);
    QCOMPARE(result.latestTag, QStringLiteral("v1.1.0-alpha.2"));
    QCOMPARE(result.releaseUrl,
             QUrl(QStringLiteral("https://github.com/snimdev/snim/releases/tag/v1.1.0-alpha.2")));
    QVERIFY(result.newer);

    // The describe tail of a locally built alpha is stripped before the channel is decided.
    const UpdateCheck::Result described =
        UpdateCheck::selectRelease(kMixedChannels, u"1.0.0-alpha.1-4-gabc1234-dirty");
    QCOMPARE(described.latestTag, QStringLiteral("v1.1.0-alpha.2"));
    QVERIFY(described.newer);
}

void tst_UpdateCheck::offersStableToAlphaBuildWhenItOutranks()
{
    // The cross-over: the alpha line ended in v1.0.0, so that is what an alpha tester gets.
    const QByteArray payload = R"([
        {
            "tag_name": "v1.0.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0",
            "draft": false,
            "prerelease": false
        },
        {
            "tag_name": "v1.0.0-alpha.3",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0-alpha.3",
            "draft": false,
            "prerelease": true
        }
    ])";

    const UpdateCheck::Result result = UpdateCheck::selectRelease(payload, u"1.0.0-alpha.3");
    QVERIFY(result.ok);
    QCOMPARE(result.latestTag, QStringLiteral("v1.0.0"));
    QVERIFY(result.newer);
}

void tst_UpdateCheck::skipsDrafts()
{
    const QByteArray payload = R"([
        {
            "tag_name": "v2.0.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v2.0.0",
            "draft": true,
            "prerelease": false
        },
        {
            "tag_name": "v1.2.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.2.0",
            "draft": false,
            "prerelease": false
        }
    ])";

    const UpdateCheck::Result result = UpdateCheck::selectRelease(payload, u"1.0.0");
    QVERIFY(result.ok);
    QCOMPARE(result.latestTag, QStringLiteral("v1.2.0"));
    QVERIFY(result.newer);
}

void tst_UpdateCheck::skipsUnparsableTags()
{
    // A rolling release such as "alpha" carries no version in its tag, so the in-app
    // check must step over it.
    const QByteArray payload = R"([
        {
            "tag_name": "alpha",
            "html_url": "https://github.com/snimdev/snim/releases/tag/alpha",
            "draft": false,
            "prerelease": true
        },
        {
            "tag_name": "nightly-2026-09-18",
            "html_url": "https://github.com/snimdev/snim/releases/tag/nightly-2026-09-18",
            "draft": false,
            "prerelease": true
        },
        {
            "tag_name": "v1.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0",
            "draft": false,
            "prerelease": true
        },
        {
            "tag_name": "v1.0.0-alpha.4",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0-alpha.4",
            "draft": false,
            "prerelease": true
        }
    ])";

    const UpdateCheck::Result result = UpdateCheck::selectRelease(payload, u"1.0.0-alpha.1");
    QVERIFY(result.ok);
    QCOMPARE(result.latestTag, QStringLiteral("v1.0.0-alpha.4"));
    QVERIFY(result.newer);
}

void tst_UpdateCheck::reportsNoReleasesForEmptyArray()
{
    // Nothing eligible is not a failure: the query worked, there is just nothing to offer.
    for (const QByteArray &payload : { QByteArray("[]"),
                                       QByteArray("[1, 2, 3]"),
                                       QByteArray(R"([{"tag_name": "alpha", "draft": false}])") }) {
        const UpdateCheck::Result result = UpdateCheck::selectRelease(payload, u"1.0.0");
        QVERIFY2(result.ok, payload.constData());
        QVERIFY(result.latestTag.isEmpty());
        QVERIFY(!result.newer);
        QVERIFY(!result.error.isEmpty());
    }

    // A stable build looking at an alpha-only repo is the same "nothing for you" case.
    const QByteArray alphaOnly = R"([
        {
            "tag_name": "v1.0.0-alpha.1",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0-alpha.1",
            "draft": false,
            "prerelease": true
        }
    ])";
    const UpdateCheck::Result result = UpdateCheck::selectRelease(alphaOnly, u"1.0.0");
    QVERIFY(result.ok);
    QVERIFY(result.latestTag.isEmpty());
    QVERIFY(!result.newer);
}

void tst_UpdateCheck::ignoresTheArrayOrder()
{
    // Republishing an old tag moves it to the top of GitHub's array; version order decides.
    const QByteArray payload = R"([
        {
            "tag_name": "v1.0.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.0.0",
            "draft": false,
            "prerelease": false
        },
        {
            "tag_name": "v1.4.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.4.0",
            "draft": false,
            "prerelease": false
        },
        {
            "tag_name": "v1.2.0",
            "html_url": "https://github.com/snimdev/snim/releases/tag/v1.2.0",
            "draft": false,
            "prerelease": false
        }
    ])";

    const UpdateCheck::Result result = UpdateCheck::selectRelease(payload, u"1.0.0");
    QVERIFY(result.ok);
    QCOMPARE(result.latestTag, QStringLiteral("v1.4.0"));
    QCOMPARE(result.releaseUrl,
             QUrl(QStringLiteral("https://github.com/snimdev/snim/releases/tag/v1.4.0")));
    // Whether it is newer depends on how this binary was versioned, so assert the rule.
    QCOMPARE(result.newer,
             UpdateCheck::isNewer(u"v1.4.0", u"1.0.0"));
    QCOMPARE(UpdateCheck::selectRelease(payload, QString::fromLatin1(Version::kVersion)).newer,
             UpdateCheck::isNewer(u"v1.4.0", QString::fromLatin1(Version::kVersion)));
}

void tst_UpdateCheck::rejectsGarbagePayloads()
{
    // Anything that is not a JSON array, including the single object /releases/latest used
    // to return, is a response this build cannot read.
    for (const QByteArray &payload : { QByteArray(),
                                       QByteArray("not json at all"),
                                       QByteArray("{\"message\": \"Not Found\"}"),
                                       QByteArray("{\"tag_name\": \"v1.4.0\"}") }) {
        const UpdateCheck::Result result = UpdateCheck::selectRelease(payload, u"1.0.0");
        QVERIFY2(!result.ok, payload.constData());
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.latestTag.isEmpty());
        QVERIFY(!result.newer);
    }
}

QTEST_MAIN(tst_UpdateCheck)
#include "tst_updatecheck.moc"
