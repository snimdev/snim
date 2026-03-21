#include <QtTest>
#include <QTcpServer>

#include "upload/SocketShim.h"

using namespace Upload;

// The POSIX/Winsock Adapter against loopback only: numeric resolution, a real connect to
// a local QTcpServer, and a refused one. No DNS and no outside network.
class tst_SocketShim : public QObject
{
    Q_OBJECT

private slots:
    void resolvesANumericHost();
    void reportsAResolveFailure();
    void connectsToALoopbackListener();
    void reportsARefusedConnection();
};

namespace {

addrinfo numericHints()
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    return hints;
}

// Opens and connects to 127.0.0.1:port; the error text is captured on failure.
SocketShim::Handle connectLoopback(quint16 port, QString *error)
{
    const addrinfo hints = numericHints();
    addrinfo *res = nullptr;
    const QByteArray portStr = QByteArray::number(port);
    if (SocketShim::resolve("127.0.0.1", portStr.constData(), &hints, &res) != 0 || !res)
        return SocketShim::Invalid;
    SocketShim::Handle s = SocketShim::open(res);
    if (s != SocketShim::Invalid && !SocketShim::connect(s, res)) {
        *error = SocketShim::lastError();
        SocketShim::close(s);
        s = SocketShim::Invalid;
    }
    SocketShim::freeResolved(res);
    return s;
}

} // namespace

void tst_SocketShim::resolvesANumericHost()
{
    const addrinfo hints = numericHints();
    addrinfo *res = nullptr;
    QCOMPARE(SocketShim::resolve("127.0.0.1", "22", &hints, &res), 0);
    QVERIFY(res);
    QCOMPARE(res->ai_family, AF_INET);
    SocketShim::freeResolved(res);
}

void tst_SocketShim::reportsAResolveFailure()
{
    const addrinfo hints = numericHints();
    addrinfo *res = nullptr;
    const int rc = SocketShim::resolve("not-an-address", "22", &hints, &res);
    QVERIFY(rc != 0);
    QVERIFY(!SocketShim::resolveError(rc).isEmpty());
}

void tst_SocketShim::connectsToALoopbackListener()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QString error;
    const SocketShim::Handle s = connectLoopback(server.serverPort(), &error);
    QVERIFY2(s != SocketShim::Invalid, qPrintable(error));
    SocketShim::setIoTimeout(s, 30);
    QVERIFY(server.waitForNewConnection(5000));
    SocketShim::close(s);
}

void tst_SocketShim::reportsARefusedConnection()
{
    quint16 port = 0;
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        port = server.serverPort();
    }   // closed: nothing listens there now

    QString error;
    const SocketShim::Handle s = connectLoopback(port, &error);
    QCOMPARE(s, SocketShim::Invalid);
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(tst_SocketShim)
#include "tst_socketshim.moc"
