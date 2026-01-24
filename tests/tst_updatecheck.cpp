#include <QtTest>

#include "core/UpdateCheck.h"
#include "core/Version.h"

using namespace Core;

// UpdateCheck's two pure helpers: the version comparison the tray action decides on,
// and the GitHub payload parser. Both are deliberately free of network and UI, so this
// test never opens a socket - checkLatest() itself stays a manual/CI-free concern.
class tst_UpdateCheck : public QObject
{
    Q_OBJECT

private slots:
    void comparesEqualVersions();
    void comparesPatchMinorMajor();
    void toleratesPrefixesAndSuffixes();
    void ordersPrereleases();
    void treatsMalformedInputAsZero();
    void parsesLatestRelease();
    void rejectsGarbagePayloads();
};

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

void tst_UpdateCheck::parsesLatestRelease()
{
    // Trimmed to the two keys the parser reads, plus noise it must ignore.
    const QByteArray payload = R"({
        "url": "https://api.github.com/repos/snimdev/snim/releases/1",
        "html_url": "https://github.com/snimdev/snim/releases/tag/v1.4.0",
        "tag_name": "v1.4.0",
        "name": "Snim 1.4.0",
        "draft": false,
        "assets": []
    })";

    const UpdateCheck::Result result = UpdateCheck::parseLatestRelease(payload);
    QVERIFY(result.ok);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.latestTag, QStringLiteral("v1.4.0"));
    QCOMPARE(result.releaseUrl,
             QUrl(QStringLiteral("https://github.com/snimdev/snim/releases/tag/v1.4.0")));
    // Whether it is newer depends on how this binary was versioned, so assert the rule.
    QCOMPARE(result.newer,
             UpdateCheck::isNewer(u"v1.4.0", QString::fromLatin1(Version::kVersion)));
}

void tst_UpdateCheck::rejectsGarbagePayloads()
{
    for (const QByteArray &payload : { QByteArray(),
                                       QByteArray("not json at all"),
                                       QByteArray("[1, 2, 3]"),
                                       QByteArray("{\"message\": \"Not Found\"}"),
                                       QByteArray("{\"tag_name\": \"\"}") }) {
        const UpdateCheck::Result result = UpdateCheck::parseLatestRelease(payload);
        QVERIFY2(!result.ok, payload.constData());
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.latestTag.isEmpty());
        QVERIFY(!result.newer);
    }
}

QTEST_MAIN(tst_UpdateCheck)
#include "tst_updatecheck.moc"
