#include "updatepanel.h"
#include "visuals.h"
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QProcessEnvironment>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <cmath>

namespace
{
QJsonObject readObject(const QString &path)
{
    if (path.isEmpty())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QLabel *valueLabel()
{
    auto *label = new QLabel("未提供");
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}
} // namespace

UpdatePanel::UpdatePanel(QWidget *parent) : QWidget(parent)
{
    // Selected by the system launcher; RPC still enforces its fixed system scope.
    configPath = qEnvironmentVariable("DSHANCONTROL_UPDATER_CONFIG", configPath);
    nativeMode = !qEnvironmentVariable("DSHANCONTROL_NATIVE_PLATFORM_SOCKET").isEmpty();
    updaterExecutable = qEnvironmentVariable("DSHANCONTROL_UPDATER_EXECUTABLE",
        nativeMode ? "/usr/bin/dshancontrol-runtime" : updaterExecutable);
    if (nativeMode) {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("DSHANCONTROL_PLATFORM_SOCKET", qEnvironmentVariable("DSHANCONTROL_NATIVE_PLATFORM_SOCKET"));
        process.setProcessEnvironment(environment);
    }
    auto *layout = new PageLayout(this);
    layout->setContentsMargins(12, 10, 12, 8);
    layout->setSpacing(6);
    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel("DshanControl 应用更新");
    title->setObjectName("panelTitle");
    heading->addWidget(title);
    auto *subtitle = new QLabel("仅更新桌面与应用，保留系统和用户数据。");
    subtitle->setObjectName("caption");
    heading->addWidget(subtitle);
    header->addLayout(heading, 1);
    check = new QPushButton("检查更新");
    check->setProperty("kind", "secondary");
    header->addWidget(check);
    layout->addLayout(header);
    connect(check, &QPushButton::clicked, this, &UpdatePanel::checkUpdates);
    columns = new QHBoxLayout;
    auto *info = new QGroupBox("发布版本");
    auto *releaseLayout = new QVBoxLayout(info);
    availableVersion = new QLabel("尚未检查");
    availableVersion->setTextFormat(Qt::PlainText);
    availableVersion->setObjectName("statusValue");
    releaseLayout->addWidget(availableVersion);
    releaseSummary = new QLabel("检查后显示与当前设备兼容的应用版本。");
    releaseSummary->setWordWrap(true);
    releaseSummary->setObjectName("infoNote");
    releaseLayout->addWidget(releaseSummary);
    auto *form = new QFormLayout;
    releaseLayout->addLayout(form);
    releaseLayout->addStretch();
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    version = valueLabel();
    board = valueLabel();
    contract = valueLabel();
    for (auto *identity : {board, contract}) {
        identity->setParent(this);
        identity->hide();
    }
    form->addRow("当前版本", version);
    columns->addWidget(info, 1);
    auto *preflight = new QGroupBox("更新流程与检查");
    auto *checks = new QFormLayout(preflight);
    checks->setVerticalSpacing(10);
    auto *flow = new QLabel("1  检查兼容版本与签名\n2  下载应用包并校验完整性\n3  进入维护流程后安装，核对最终结果");
    flow->setObjectName("updateWorkflowSteps"); flow->setWordWrap(true);
    checks->addRow(flow);
    int iconIndex = 0;
    const QStringList symbols = {"chip", "server", "check"};
    for (const QString &name : {QString("设备匹配"), QString("运行环境"), QString("发布清单签名")}) {
        auto *value = valueLabel();
        value->setText("待检查");
        auto *label = new QWidget;
        auto *row = new QHBoxLayout(label);
        row->setContentsMargins(0, 5, 8, 5);
        auto *symbol = new QLabel;
        symbol->setPixmap(Visuals::icon(symbols[iconIndex++]).pixmap(24, 24));
        row->addWidget(symbol);
        row->addWidget(new QLabel(name));
        checks->addRow(label, value);
        preflightValues << value;
    }
    columns->addWidget(preflight, 1);
    layout->addLayout(columns, 1);
    status = valueLabel();
    status->setObjectName("warningNote");
    layout->addWidget(status);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    auto *details = new QPushButton("查看兼容性详情");
    details->setObjectName("updateCompatibilityDetails");
    details->setIcon(Visuals::icon("document", Visuals::blue));
    details->setProperty("kind", "secondary");
    actions->addWidget(details);
    connect(details, &QPushButton::clicked, this, [this]() {
        QMessageBox dialog(QMessageBox::Information, "兼容性详情", QString(), QMessageBox::Ok, this);
        dialog.setTextFormat(Qt::PlainText);
        dialog.setText("当前应用：" + version->text() + "\n硬件标识：" + board->text() +
                       "\n系统接口：" + contract->text() + "\n\n" + status->text());
        dialog.exec();
    });
    download = new QPushButton("下载更新");
    download->setObjectName("downloadUpdate");
    download->setEnabled(false);
    download->setProperty("kind", "secondary");
    actions->addWidget(download);
    connect(download, &QPushButton::clicked, this, &UpdatePanel::downloadUpdate);
    install = new QPushButton("安装更新");
    install->setObjectName("installUpdate");
    install->setEnabled(false);
    install->setToolTip("安装操作需由系统维护流程授权执行");
    actions->addWidget(install);
    connect(install, &QPushButton::clicked, this, &UpdatePanel::installUpdate);
    layout->addLayout(actions);
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exitStatus) {
                if (nativeMode) {
                    nativeResult(code, exitStatus);
                    return;
                }
                check->setEnabled(true);
                for (auto *label : preflightValues)
                    label->setStyleSheet("color:#61758b;");
                releaseSummary->setText("本次检查未确认可用版本；未下载或安装应用包。");
                const QByteArray output = process.readAllStandardOutput().trimmed();
                if (exitStatus != QProcess::NormalExit || code != 0) {
                    availableVersion->setText("检查失败");
                    for (auto *label : preflightValues)
                        label->setText("未通过检查");
                    status->setText("检查失败。请检查网络、系统更新配置和签名公钥；未执行安装。");
                    return;
                }
                if (output == "null") {
                    availableVersion->setText("无兼容更新");
                    for (auto *label : preflightValues)
                        label->setText("无匹配版本");
                    status->setText("未发现符合当前硬件与版本条件的应用更新。");
                    return;
                }
                const QJsonObject candidate = QJsonDocument::fromJson(output).object();
                const auto sequence = candidate["sequence"];
                bool valid = QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")
                    .match(candidate["version"].toString()).hasMatch() && sequence.isDouble() &&
                    sequence.toDouble() >= 1 && sequence.toDouble() <= 9007199254740991.0 &&
                    sequence.toDouble() == std::floor(sequence.toDouble());
                for (const auto &key : {"manifest_url", "signature_url", "package_url"})
                    valid = valid && candidate[key].isString() && !candidate[key].toString().trimmed().isEmpty();
                if (!valid) {
                    availableVersion->setText("检查结果无效");
                    for (auto *label : preflightValues)
                        label->setText("结果无效");
                    status->setText("系统更新器返回了无效结果，请检查系统服务。");
                    return;
                }
                availableVersion->setText("版本 " + candidate.value("version").toString());
                releaseSummary->setText(
                    "已找到与当前设备匹配的发布版本。应用包尚未安装，设备继续运行当前版本。");
                for (auto *label : preflightValues) {
                    label->setText("● 检查通过");
                    label->setStyleSheet("color:#168345;");
                }
                preflightValues.last()->setText("清单已验证");
                status->setText("发现兼容版本 " + candidate.value("version").toString() +
                                "。请安排维护窗口完成安装。");
            });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        taskPoll.stop();
        download->setEnabled(false);
        install->setEnabled(false);
        for (auto *label : preflightValues) {
            label->setText("检查未完成");
            label->setStyleSheet("color:#61758b;");
        }
        releaseSummary->setText("本次检查未完成；未下载或安装应用包。");
        availableVersion->setText("检查未完成");
        check->setEnabled(true);
        status->setText("无法完成更新检查，请检查系统更新服务。");
    });
    taskPoll.setInterval(1000);
    connect(&taskPoll, &QTimer::timeout, this, [this]() {
        if (process.state() == QProcess::NotRunning && !taskId.isEmpty())
            requestNative("update.status", {{"task_id", taskId}});
    });
    reloadIdentity();
}

UpdatePanel::~UpdatePanel()
{
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
    }
}

void UpdatePanel::reloadIdentity(bool restoreTask)
{
    // Native launchers announce the public service. Its clients never need to
    // open the system owner's updater configuration, trust root or data paths.
    const QJsonObject config = nativeMode ? QJsonObject() : readObject(configPath);
    nativeMode = nativeMode || (config.value("task_service").isBool() && config.value("task_service").toBool());
    if (nativeMode) {
        restoreTaskAfterIdentity = restoreTask;
        nativeIdentityReady = false;
        check->setEnabled(false);
        status->setText("正在读取系统更新服务的版本记录…");
        requestNative("update.identity", {});
        return;
    }
    const QString root = config.value("application_root").toString();
    const QJsonObject installed =
        root.isEmpty() ? QJsonObject() : readObject(root + "/current/installed.json");
    const QJsonObject system = readObject(config.value("system_contract").toString());
    const QString currentVersion = installed["version"].toString().trimmed();
    const QString boardId = system["board_id"].toString().trimmed();
    const QString contractId = system["contract_id"].toString().trimmed();
    version->setText(currentVersion.isEmpty() ? "未提供应用版本记录" : currentVersion);
    board->setText(boardId.isEmpty() ? "未配置独立应用更新" : boardId);
    contract->setText(contractId.isEmpty() ? "未提供系统接口记录" : contractId);
    const QFileInfo key(config.value("public_key").toString());
    const QFileInfo executable(updaterExecutable);
    const bool configured = !root.isEmpty() && !currentVersion.isEmpty() && !boardId.isEmpty() &&
                            !contractId.isEmpty() && key.isFile() && key.isReadable() &&
                            executable.isFile() && executable.isExecutable();
    check->setEnabled(configured);
    status->setText(configured ? "可以检查兼容应用更新，尚未开始检查。"
                               : "应用更新尚未就绪：需要系统版本记录、预装应用记录、更新器和签名公钥。");
}

void UpdatePanel::checkUpdates()
{
    if (process.state() != QProcess::NotRunning)
        return;
    if (nativeMode && !nativeIdentityReady) {
        reloadIdentity();
        return;
    }
    if (!nativeMode)
        reloadIdentity();
    if (!check->isEnabled()) {
        availableVersion->setText("检查未完成");
        releaseSummary->setText("系统更新配置不可用；未下载或安装应用包。");
        for (auto *label : preflightValues) {
            label->setText("未检查");
            label->setStyleSheet("color:#61758b;");
        }
        return;
    }
    for (auto *label : preflightValues) {
        label->setText("检查中");
        label->setStyleSheet("color:#526d93;");
    }
    availableVersion->setText("正在检查…");
    releaseSummary->setText("读取发布清单并匹配当前设备、运行环境与应用版本。");
    check->setEnabled(false);
    status->setText("正在检查 GitHub Releases 并校验兼容性…");
    if (nativeMode) {
        taskPoll.stop();
        taskId.clear();
        checkTaskId.clear();
        downloadTaskId.clear();
        selectedRelease = {};
        download->setEnabled(false);
        install->setEnabled(false);
        requestNative("update.start", {{"operation", "check"}});
        return;
    }
    process.start(updaterExecutable, {"check"});
    // Own timeout per invocation; delete it on finish so an old check cannot kill a new one.
    auto *timeout = new QTimer(&process);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, &process, &QProcess::kill);
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), timeout,
            &QObject::deleteLater);
    connect(&process, &QProcess::errorOccurred, timeout, &QObject::deleteLater);
    timeout->start(600000);
}

void UpdatePanel::requestNative(const QString &method, const QJsonObject &params)
{
    if (process.state() != QProcess::NotRunning)
        return;
    nativeMethod = method;
    process.start(updaterExecutable, {"rpc", method,
        QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))});
}

void UpdatePanel::resetPreflight(const QString &text)
{
    for (auto *label : preflightValues) {
        label->setText(text);
        label->setStyleSheet("color:#61758b;");
    }
}

void UpdatePanel::nativeResult(int code, QProcess::ExitStatus exitStatus)
{
    const QByteArray output = process.readAllStandardOutput().trimmed();
    const QByteArray errorOutput = process.readAllStandardError().trimmed();
    if (exitStatus != QProcess::NormalExit || code != 0) {
        if (nativeMethod == "update.identity")
            nativeIdentityReady = false;
        taskPoll.stop();
        check->setEnabled(true);
        download->setEnabled(false);
        install->setEnabled(false);
        resetPreflight("查看失败原因");
        const QJsonObject error = QJsonDocument::fromJson(errorOutput).object();
        status->setText(error.isEmpty() ? "系统更新服务未完成请求，请检查服务状态。" :
            QString("阶段：%1 · 错误码：%2\n%3").arg(error["stage"].toString(),
            error["code"].toString(), error["message"].toString()));
        return;
    }
    if (nativeMethod == "update.identity") {
        const auto identity = QJsonDocument::fromJson(output).object();
        const auto installed = identity["installed"].toObject();
        const auto system = identity["system"].toObject();
        const auto currentVersion = installed["version"].toString().trimmed();
        const auto boardId = system["board_id"].toString().trimmed();
        const auto contractId = system["contract_id"].toString().trimmed();
        version->setText(currentVersion.isEmpty() ? "未提供应用版本记录" : currentVersion);
        board->setText(boardId.isEmpty() ? "未提供硬件标识" : boardId);
        contract->setText(contractId.isEmpty() ? "未提供系统接口记录" : contractId);
        nativeIdentityReady = !currentVersion.isEmpty() && !boardId.isEmpty() && !contractId.isEmpty() &&
            identity["check_configured"].isBool() && identity["check_configured"].toBool();
        check->setEnabled(true); // A later click may refresh unavailable identity.
        download->setEnabled(false);
        install->setEnabled(false);
        if (!nativeIdentityReady) {
            resetPreflight("未检查");
            status->setText("应用更新尚未就绪：系统版本记录或签名公钥未配置。");
            return;
        }
        status->setText("可以检查兼容应用更新，尚未开始检查。");
        if (restoreTaskAfterIdentity)
            requestNative("update.status", {});
        return;
    }
    if (output == "null") {
        taskPoll.stop();
        check->setEnabled(true);
        resetPreflight("未检查");
        status->setText("尚无系统更新任务，可以检查兼容应用版本。");
        return;
    }
    const QJsonObject task = QJsonDocument::fromJson(output).object();
    const QString id = task["task_id"].toString();
    if (!QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch() ||
        (!taskId.isEmpty() && nativeMethod == "update.status" && taskId != id)) {
        taskPoll.stop();
        check->setEnabled(true);
        download->setEnabled(false);
        install->setEnabled(false);
        resetPreflight("未验证");
        status->setText("系统返回的任务标识无效，未执行后续操作。");
        return;
    }
    const bool historical = taskId.isEmpty() && nativeMethod == "update.status";
    taskId = id;
    const QString state = task["status"].toString(), operation = task["operation"].toString();
    const QHash<QString, QString> stages{{"preparing", "准备"}, {"checking", "检查发布"},
        {"download-metadata", "下载清单"}, {"download-package", "下载应用包"},
        {"signature", "校验签名"}, {"compatibility", "匹配系统"}, {"staging", "暂存"},
        {"maintenance", "检查维护状态"}, {"stopping", "停止应用"}, {"switching", "切换版本"},
        {"starting", "启动应用"}, {"health", "健康检查"}, {"rollback", "恢复旧版"},
        {"recovery", "恢复事务"}, {"completed", "完成"}};
    if (state == "running") {
        check->setEnabled(false);
        download->setEnabled(false);
        install->setEnabled(false);
        status->setText("系统任务执行中：" + stages.value(task["stage"].toString(), task["stage"].toString()));
        taskPoll.start();
        return;
    }
    taskPoll.stop();
    check->setEnabled(true);
    download->setEnabled(false);
    install->setEnabled(false);
    if (state == "failed") {
        resetPreflight("查看失败原因");
        const auto error = task["error"].toObject();
        status->setText((historical ? "历史任务失败\n" : "更新任务失败\n") +
            QString("阶段：%1 · 错误码：%2\n%3").arg(error["stage"].toString(),
                error["code"].toString(), error["message"].toString()));
        return;
    }
    if (state != "success") {
        resetPreflight("未验证");
        status->setText("系统任务状态无效，未启动后续操作。");
        return;
    }
    selectedRelease = task["candidate"].toObject();
    if (operation == "check" && selectedRelease.isEmpty()) {
        resetPreflight("无匹配版本");
        preflightValues.last()->setText("无候选清单");
        availableVersion->setText("无兼容更新");
        releaseSummary->setText("未发现匹配当前设备与应用版本的可信发布。");
        status->setText(historical ? "历史检查完成：无兼容更新。" : "检查完成：无兼容更新。");
        return;
    }
    if (operation == "install") {
        reloadIdentity(false);
        status->setText("系统更新任务成功，应用已通过健康检查。");
        return;
    }
    const bool candidateValid = QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")
        .match(selectedRelease["version"].toString()).hasMatch() &&
        QRegularExpression("^[a-f0-9]{64}$").match(selectedRelease["package_sha256"].toString()).hasMatch();
    if (!candidateValid) {
        resetPreflight("未验证");
        status->setText("系统返回的发布身份无效，未启动后续操作。");
        return;
    }
    const bool development = selectedRelease["development_release"].isBool() &&
                             selectedRelease["development_release"].toBool();
    availableVersion->setText((development ? "开发草稿 " : "版本 ") + selectedRelease["version"].toString());
    if (operation == "check") {
        checkTaskId = id;
        download->setEnabled(true);
        releaseSummary->setText(development ? "已验证开发草稿清单，可下载用于开发验证的应用包。"
                                            : "已找到可信兼容版本，可以下载应用包。");
        preflightValues.last()->setText("清单已验证");
    } else if (operation == "download") {
        downloadTaskId = id;
        install->setEnabled(true);
        releaseSummary->setText(development ? "开发草稿已下载并校验，安装前仍需设备允许维护。"
                                            : "应用包已下载并校验，安装前需要设备允许维护。");
        preflightValues.last()->setText("清单与应用包已验证");
    } else {
        resetPreflight("未验证");
        status->setText("系统返回了未知任务类型。");
        return;
    }
    for (int i = 0; i < preflightValues.size() - 1; ++i)
        preflightValues[i]->setText("检查通过");
    status->setText(historical ? "已恢复历史任务结果；尚未开始新操作。" : "系统任务完成。");
}

void UpdatePanel::downloadUpdate()
{
    if (!nativeMode || checkTaskId.isEmpty() || process.state() != QProcess::NotRunning)
        return;
    download->setEnabled(false);
    install->setEnabled(false);
    taskId.clear();
    requestNative("update.start", {{"operation", "download"}, {"source_task_id", checkTaskId}});
}

void UpdatePanel::installUpdate()
{
    if (!nativeMode || downloadTaskId.isEmpty() || process.state() != QProcess::NotRunning)
        return;
    QMessageBox dialog(QMessageBox::Warning, "安装应用更新",
        "安装版本 " + selectedRelease["version"].toString() +
        " 将重启桌面及相关应用服务。系统会检查维护状态，失败时恢复旧版。", QMessageBox::Yes | QMessageBox::No, this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setDefaultButton(QMessageBox::No);
    if (dialog.exec() != QMessageBox::Yes)
        return;
    install->setEnabled(false);
    download->setEnabled(false);
    taskId.clear();
    requestNative("update.start", {{"operation", "install"}, {"source_task_id", downloadTaskId},
        {"confirmed", true}, {"version", selectedRelease["version"]},
        {"package_sha256", selectedRelease["package_sha256"]}});
}

void UpdatePanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    columns->setDirection(width() < 600 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
}
