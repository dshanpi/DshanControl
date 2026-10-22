// Integration with the real native service. The runner provides only a
// private, synthetic downloaded-task fixture; no production installation.
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
    void downloadedIdentityAndMaintenanceFailureUseRealNativeService()
    {
        const QString source = qEnvironmentVariable("DSHANCONTROL_TEST_DOWNLOADED_TASK");
        QVERIFY2(source.size() == 32, "Run using run_native_update_bridge.py with a private service");
        UpdatePanel panel;
        panel.resize(1024, 600);
        panel.show();
        QTRY_VERIFY(panel.nativeIdentityReady);
        QTRY_VERIFY(panel.install->isEnabled());
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        QCOMPARE(panel.version->text(), QString("0.1.0"));
        QCOMPARE(panel.board->text(), QString("100ask-t113s4-sdnand"));
        QCOMPARE(panel.downloadTaskId, source);
        QCOMPARE(panel.selectedRelease["version"].toString(), QString("0.2.0"));
        QVERIFY(panel.status->text().contains("历史"));
        QVERIFY(!panel.download->isEnabled());
        QVERIFY(!panel.taskPoll.isActive());

        // Declining confirmation must not submit a task at all.
        QTimer::singleShot(0, &panel, []() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) box->button(QMessageBox::No)->click();
        });
        panel.install->click();
        QCOMPARE(panel.taskId, source);
        QVERIFY(panel.install->isEnabled());
        QCOMPARE(panel.process.state(), QProcess::NotRunning);

        // The real worker verifies the signed fixture and reaches the hook.
        // The private hook declines maintenance with exit 17 before stop.
        QTimer::singleShot(0, &panel, []() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) box->button(QMessageBox::Yes)->click();
        });
        panel.install->click();
        // SD NAND durably stages each fixture member before maintenance.
        QTRY_VERIFY_WITH_TIMEOUT(panel.status->text().contains("17"), 90000);
        QVERIFY(panel.status->text().contains("maintenance"));
        QCOMPARE(panel.status->textFormat(), Qt::PlainText);
        QCOMPARE(panel.process.state(), QProcess::NotRunning);
        QVERIFY(!panel.install->isEnabled());
        QVERIFY(!panel.download->isEnabled());
        QVERIFY(panel.check->isEnabled());
        QVERIFY(!panel.taskPoll.isActive());
        const QString failedTask = panel.taskId;
        QVERIFY(failedTask.size() == 32 && failedTask != source);
        const QString failureText = panel.status->text();
        QTest::qWait(1200);
        QCOMPARE(panel.taskId, failedTask);
        QCOMPARE(panel.status->text(), failureText);
        QVERIFY(!panel.taskPoll.isActive());

        // Reopening projects the durable failure, without resubmission.
        UpdatePanel reopened;
        QTRY_VERIFY(reopened.nativeIdentityReady);
        QTRY_VERIFY(reopened.status->text().contains("历史任务失败"));
        QCOMPARE(reopened.taskId, failedTask);
        QVERIFY(reopened.status->text().contains("17"));
        QVERIFY(reopened.status->text().contains("maintenance"));
        QVERIFY(!reopened.install->isEnabled() && !reopened.download->isEnabled());
        QVERIFY(!reopened.taskPoll.isActive());
        QFile evidence(qEnvironmentVariable("DSHANCONTROL_TEST_UI_EVIDENCE"));
        QVERIFY(evidence.open(QIODevice::WriteOnly | QIODevice::NewOnly));
        QJsonObject result{{"source_task_id", source}, {"failed_task_id", failedTask},
            {"current_version", panel.version->text()}, {"failure_text", failureText},
            {"restored_failure_text", reopened.status->text()},
            {"failure_plain_text", panel.status->textFormat() == Qt::PlainText},
            {"no_automatic_retry", !panel.taskPoll.isActive() && !reopened.taskPoll.isActive()}};
        QVERIFY(evidence.write(QJsonDocument(result).toJson()) > 0);
    }
};
QTEST_MAIN(HmiUiTest)
#include "native_update_bridge_test.moc"
