#include <QtTest>
#include <QProcess>
#include <QProcessEnvironment>
#include "../../components/hmi/src/localapi.h"

class LocalApiTest : public QObject
{
    Q_OBJECT
private slots:
    void defaultPortPreservesExistingBoards()
    {
        qunsetenv("DSHANCONTROL_HMI_API_PORT");
        QCOMPARE(hmiApiBaseUrl(), QString("http://127.0.0.1/api/hmi/v1"));
    }
    void unprivilegedPortStaysOnLoopback()
    {
        qputenv("DSHANCONTROL_HMI_API_PORT", "8080");
        QCOMPARE(hmiApiBaseUrl(), QString("http://127.0.0.1:8080/api/hmi/v1"));
        qunsetenv("DSHANCONTROL_HMI_API_PORT");
    }
    void malformedPortCannotRedirectControls()
    {
        for (const auto &value : {"0", "65536", "-1", "http://example.com", "8080/path"}) {
            QProcess child;
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert("DSHANCONTROL_HMI_API_PORT", value);
            child.setProcessEnvironment(env);
            child.start(QCoreApplication::applicationFilePath(), {"--probe"});
            QVERIFY(child.waitForFinished(5000));
            QVERIFY(child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0);
            QVERIFY(child.readAllStandardError().contains("DSHANCONTROL_HMI_API_PORT"));
        }
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--probe")) {
        hmiApiBaseUrl();
        return 0;
    }
    LocalApiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "localapi-test.moc"
