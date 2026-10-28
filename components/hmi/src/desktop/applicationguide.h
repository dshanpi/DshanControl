#pragma once
#include "applicationregistry.h"
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QVBoxLayout>

namespace DesktopV1 {
struct ApplicationHelp {
    QString before;
    QStringList steps;
    QString result, recovery, related, relatedLabel;
};
inline ApplicationHelp applicationHelp(const QString &id)
{
    static const QMap<QString, ApplicationHelp> guides = {
        {"overview", {"先看数据来源：样例仅用于演示，离线表示当前无法读取设备。",
            {"查看连接、资源和告警概况。", "需要读数细节时打开设备监控；需要处理告警时进入告警记录。", "进入相应应用完成操作，再返回总览核对状态。"},
            "以新收到的设备数据为准；页面打开或按钮点击不表示设备恢复。",
            "数据不更新时先查看连接状态，不要反复发送控制命令。",
            "devices", "打开设备监控"}},
        {"fieldbus", {"先选择实际使用的协议和通道；写参数、控制节点前确认现场允许操作。",
            {"在 CAN、RS485、EtherCAT 标签中选择协议。", "核对通道、节点、速率或对象地址，再填写参数。", "点击对应操作，阅读确认内容；完成后查看状态和操作日志。"},
            "请求被接收不等于现场设备已响应。NMT、SDO 等操作需要核对目标设备结果。",
            "操作不可用时查看能力和连接提示；未知执行结果先核对，不能直接重复发送。",
            "settings", "打开系统设置"}},
        {"network", {"先区分有线、无线和蜂窝网络。链路连接与能访问外部服务是不同状态。",
            {"选择网络类型，查看接口、地址和连接状态。", "使用刷新连接信息获取当前状态；蜂窝操作先核对对应设备。", "操作后重新查看地址与状态，再到目标应用验证服务连接。"},
            "有 IP 地址不代表互联网、云服务或账号绑定已经可用。",
            "本页未提供的网络配置不能在此完成；状态未获取时先检查系统网络服务。",
            "cloud_bind", "打开官网绑定"}},
        {"devices", {"确认数据来源与点位；没有读数时不把空白趋势当作零值。",
            {"查看读数、单位和时间趋势。", "需要仔细查看时点击暂停监控显示；这只冻结本页，现场采集仍继续。", "点击继续显示恢复最新数据；刷新频率在显示设置中调整。"},
            "暂停期间显示的是旧数据；恢复后趋势从新片段开始。",
            "数据缺失时查看现场总线和采集服务，不能据此判断设备数值为零。",
            "fieldbus", "打开现场总线"}},
        {"alarms", {"确认告警数据已加载。确认告警表示已知悉，不会消除现场故障。",
            {"按状态、级别、来源和时间筛选；结果为空时可清除筛选。", "选择一条记录查看详情，再确认选中告警。", "需要留存时导出筛选结果；保存的是点击导出时的可见记录。"},
            "确认后核对记录状态；导出成功以保存完成提示为准。",
            "无匹配记录与服务离线不同。离线可查看保留记录，但不能确认告警。",
            "devices", "打开设备监控"}},
        {"settings", {"显示设置需要手动保存；服务管理和整机重启会影响运行中的业务。",
            {"在显示与交互中修改亮度、演示开关或刷新频率。", "核对后保存并应用；等待设备确认，不要重复点击。", "服务操作先读确认内容；执行结果到操作日志核对。"},
            "未保存的编辑保留在页面内；等待确认与已应用不是同一状态。",
            "离线不能保存；执行结果未知时先查看日志和当前状态。",
            "overview", "打开运行总览"}},
        {"cloud_bind", {"需要网络、接入服务和设备登记；绑定码属于设备归属流程。",
            {"查看设备编号、接入连接和账号归属。", "连接就绪后查询绑定码，按绑定步骤到官网设备管理中完成绑定。", "返回本页重新查询归属，核对是否已绑定到正确账号。"},
            "显示绑定码不表示绑定完成；以设备归属查询结果为准。",
            "未连接先看网络；绑定码失效需重新查询，不能使用旧码判断成功。",
            "network", "打开网络通信"}},
        {"reporting", {"先确认接入服务、上报配置与数据来源；样例数据不等于真实采集。",
            {"查看当前连接、配置和待上报数据。", "核对内容后执行上报，等待最近服务反馈。", "在云端核对收到的内容与时间。"},
            "代理接收请求不等于云端已收到数据；页面反馈与实时状态分开查看。",
            "反馈未知时先查询服务或云端记录，避免重复发送。",
            "cloud_bind", "打开官网绑定"}},
        {"voice", {"需要语音接入服务、麦克风、扬声器及对应能力就绪。",
            {"查看服务状态并选择可用的对话模式。", "按页面提示开始说话，松开或停止后等待识别与回复。", "结束对话；需要时清空本次页面记录。"},
            "以实际识别、回复和声音为准；按钮可点击不表示音频链路已通过。",
            "服务未连接时先查看网络与服务状态；不要把空回复当作执行成功。",
            "network", "打开网络通信"}},
        {"home", {"需要家庭服务已连接，并且目标设备已提供可用功能。",
            {"选择设备，查看名称、当前状态和可用功能。", "选择要执行的功能，确认目标和参数后发送控制。", "等待设备状态回报，再判断操作是否完成。"},
            "服务接收命令不等于设备已动作；状态未知时不要反复发送。",
            "没有设备或功能时先完成服务侧接入，刷新后再操作。",
            "network", "打开网络通信"}},
        {"video", {"需要可读的视频文件和就绪的播放组件；选择文件不会自动播放。",
            {"点击选择视频，从视频目录、U盘或文件系统进入文件夹。", "选择视频返回播放器，核对文件名，再点击播放。", "用文字按钮暂停、静音或全屏；退出全屏后可返回其他应用。"},
            "以实际画面和声音为准；文件被选中不表示解码已成功。",
            "文件不可用先检查存储位置；组件未就绪时先处理组件提示，不要反复点击播放。",
            "settings", "打开系统设置"}},
        {"update", {"需要兼容的更新服务、当前版本记录和维护条件；本页不更新系统固件。",
            {"检查更新，查看可用版本与兼容性检查结果。", "可用时下载更新，等待完整性校验。", "仅在允许维护时安装，查看明确的成功或失败结果。"},
            "找到版本、下载完成与安装完成是三个不同阶段。",
            "检查失败先查看兼容性详情；安装结果未知时核对记录，不要重复安装或重启。",
            "settings", "打开系统设置"}}
    };
    return guides.value(id);
}

// Instruction and navigation only: no command execution or service writes.
class ApplicationGuide : public QDialog {
public:
    ApplicationGuide(const QString &id, const QString &location, QWidget *parent=nullptr)
        : QDialog(parent)
    {
        setObjectName("applicationGuide");
        setWindowTitle("使用指引");
        const auto guide=applicationHelp(id);
        setStyleSheet("QDialog{background:#f4f4f4;} QWidget{font-family:'Dshan Sans';font-size:16px;color:#161616;}"
            "QScrollArea{border:0;background:#f4f4f4;} QFrame[helpCard=true]{background:white;border:1px solid #d0d0d0;}"
            "QPushButton{min-height:48px;padding:0 16px;background:white;border:1px solid #a8a8a8;}"
            "QPushButton:focus{border:2px solid #0f62fe;} QPushButton[primary=true]{background:#0f62fe;color:white;}");
        auto *outer=new QVBoxLayout(this);
        outer->setContentsMargins(20,16,20,16);
        auto *title=new QLabel("使用指引");
        title->setStyleSheet("font-size:22px;font-weight:600;");
        outer->addWidget(title);
        auto *where=new QLabel(location);
        where->setObjectName("guideLocation"); where->setTextFormat(Qt::PlainText); where->setWordWrap(true);
        outer->addWidget(where);
        auto *scroll=new QScrollArea; scroll->setWidgetResizable(true);
        scroll->setObjectName("guideScroll");
        auto *body=new QWidget; auto *content=new QVBoxLayout(body);
        content->setContentsMargins(0,0,8,0);
        auto card=[content](const QString &heading,const QString &text,const QString &name) {
            auto *frame=new QFrame;frame->setProperty("helpCard",true);
            auto *layout=new QVBoxLayout(frame);
            auto *label=new QLabel(heading);label->setStyleSheet("font-weight:600;color:#0f62fe;");
            layout->addWidget(label);
            auto *description=new QLabel(text);description->setObjectName(name);
            description->setTextFormat(Qt::PlainText);description->setWordWrap(true);
            layout->addWidget(description);content->addWidget(frame);
        };
        card("开始前",guide.before,"guideBefore");
        QStringList steps;
        for(int i=0;i<guide.steps.size();++i) steps<<QString("%1. %2").arg(i+1).arg(guide.steps[i]);
        card("操作步骤",steps.join("\n\n"),"guideSteps");
        card("如何确认结果",guide.result,"guideResult");
        card("遇到问题",guide.recovery,"guideRecovery");
        content->addStretch();scroll->setWidget(body);outer->addWidget(scroll,1);
        QScroller::grabGesture(scroll->viewport(),QScroller::TouchGesture);
        auto *footer=new QHBoxLayout;
        auto *related=new QPushButton(guide.relatedLabel);
        related->setObjectName("guideRelated");
        related->setEnabled(ApplicationRegistry::indexForId(guide.related)>=0);
        footer->addWidget(related);footer->addStretch();
        auto *close=new QPushButton("返回当前页面");close->setObjectName("guideClose");
        close->setProperty("primary",true);close->setDefault(true);related->setAutoDefault(false);footer->addWidget(close);
        outer->addLayout(footer);
        connect(close,&QPushButton::clicked,this,&QDialog::reject);
        connect(related,&QPushButton::clicked,this,[this,guide](){target=guide.related;accept();});
        resize(parent?qMin(900,parent->width()-32):900,parent?qMin(550,parent->height()-24):550);
    }
    QString target;
};
} // namespace DesktopV1
