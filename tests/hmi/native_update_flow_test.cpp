// Real native task service and TLS downloader in a private synthetic fixture.
#include "../../components/hmi/src/updatepanel.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
class HmiUiTest : public QObject
{
    Q_OBJECT
  private slots:
    void checkDownloadAndIdentityPinnedConfirmation()
    {
        const QString mode = qEnvironmentVariable("DSHANCONTROL_TEST_UPDATE_FLOW");
        QVERIFY(mode == "normal" || mode == "replace-on-download");
        UpdatePanel panel;
        panel.show();
        QTRY_VERIFY(panel.nativeIdentityReady);
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        QVERIFY(!panel.download->isEnabled() && !panel.install->isEnabled());
        panel.check->click();
        QTRY_VERIFY_WITH_TIMEOUT(panel.download->isEnabled(), 90000);
        const QString checked = panel.checkTaskId;
        QVERIFY(checked.size() == 32);
        QVERIFY(panel.availableVersion->text().contains("开发草稿"));
        QVERIFY(!panel.install->isEnabled());
        const QString packageHash = panel.selectedRelease["package_sha256"].toString();
        QVERIFY(packageHash.size() == 64);
        panel.download->click();
        QJsonObject evidence{{"check_task_id", checked}, {"package_sha256", packageHash}, {"mode", mode}};
        if (mode == "replace-on-download") {
            QTRY_VERIFY_WITH_TIMEOUT(panel.status->text().contains("development-release-changed"), 90000);
            QVERIFY(panel.status->text().contains("download"));
            QVERIFY(!panel.install->isEnabled() && !panel.download->isEnabled());
            QVERIFY(!panel.taskPoll.isActive());
            evidence["failed_task_id"] = panel.taskId;
            evidence["failure_text"] = panel.status->text();
        } else {
            QTRY_VERIFY_WITH_TIMEOUT(panel.install->isEnabled(), 90000);
            const QString downloaded = panel.downloadTaskId;
            QVERIFY(downloaded.size() == 32 && downloaded != checked);
            QCOMPARE(panel.selectedRelease["package_sha256"].toString(), packageHash);
            QVERIFY(panel.releaseSummary->text().contains("下载并校验"));
            QTimer::singleShot(0, &panel, []() {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (box) box->button(QMessageBox::Yes)->click();
            });
            panel.install->click();
            QTRY_VERIFY_WITH_TIMEOUT(panel.status->text().contains("17"), 90000);
            QVERIFY(panel.status->text().contains("maintenance"));
            QVERIFY(!panel.install->isEnabled() && !panel.download->isEnabled());
            evidence["download_task_id"] = downloaded;
            evidence["failed_task_id"] = panel.taskId;
            evidence["failure_text"] = panel.status->text();
        }
        QCOMPARE(panel.status->textFormat(), Qt::PlainText);
        QVERIFY(!panel.taskPoll.isActive());
        const QString failed = panel.taskId;
        QTest::qWait(1200);
        QCOMPARE(panel.taskId, failed);
        QVERIFY(!panel.taskPoll.isActive());
        QFile output(qEnvironmentVariable("DSHANCONTROL_TEST_UI_EVIDENCE"));
        QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::NewOnly));
        QVERIFY(output.write(QJsonDocument(evidence).toJson()) > 0);
    }
};
QTEST_MAIN(HmiUiTest)
#include "native_update_flow_test.moc"
