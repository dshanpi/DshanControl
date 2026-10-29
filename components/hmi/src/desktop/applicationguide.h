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
inline ApplicationHelp applicationHelp(const QString &id, const QString &section = {})
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
            {"在显示与交互中对照服务记录修改亮度、演示开关或刷新频率；不想保留时选择撤销修改。", "核对后保存并应用；在底部查看最近操作结果，等待设备确认，不要重复点击。", "服务操作先读确认内容；执行结果到操作日志核对。"},
            "未保存的编辑保留在页面内；等待确认与已应用不是同一状态。",
            "离线保留编辑，不能保存或撤销；保存成功但背光未确认时，先检查背光设备。连接变化会取消本次保存，需重新核对。",
            "overview", "打开运行总览"}},
        {"cloud_bind", {"需要网络、接入服务和设备登记；绑定码属于设备归属流程。",
            {"查看设备编号、接入连接和账号归属。", "连接就绪后查询绑定码，按绑定步骤到官网设备管理中完成绑定。", "返回本页重新查询归属，核对是否已绑定到正确账号。"},
            "显示绑定码不表示绑定完成；以设备归属查询结果为准。",
            "未连接先看网络；绑定码失效需重新查询，不能使用旧码判断成功。",
            "network", "打开网络通信"}},
        {"reporting", {"先确认接入服务、上报配置与数据来源；样例数据不等于真实采集。",
            {"查看当前连接和待上报数据；自动上报开关与周期选择会立即提交配置。", "核对内容后执行上报，等待最近服务反馈。", "在云端核对收到的内容与时间。"},
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
    static const QMap<QString, ApplicationHelp> sections = {
        {"fieldbus.nmt", {"确认 CAN 通道、速率和目标节点；节点 0 表示广播，会影响总线上所有节点。",
            {"需要改变通道速率时先填写 CAN 参数，再点应用并确认。", "填写 NMT 节点编号，选择目标状态；核对是否为广播。", "点击发送 NMT，确认目标，再查看设备状态与操作日志。"},
            "NMT 发送完成只证明请求完成；节点是否进入目标状态需设备回报。", "结果未知先检查节点与日志，不能连续发送复位或广播。", "settings", "打开操作日志所在应用"}},
        {"fieldbus.sdo", {"确认 CANopen 能力、节点和对象字典。写入对象可能改变现场行为。",
            {"先核对 CAN 通道与速率。", "填写节点 1–127、对象索引、子索引和写入值；当前写入长度为 4 字节。", "点写入 SDO，核对目标后确认；到设备侧读取目标对象验证。"},
            "请求成功不代表参数已持久化；以设备对象读回及实际行为为准。", "地址、数据类型或长度不匹配时先查设备对象字典，不能反复写入。", "settings", "打开系统设置"}},
        {"fieldbus.rs485", {"确认实际串口、波特率及从站地址；本页配置首个 Modbus 从站。",
            {"填写串口、波特率、从站地址和轮询间隔。", "点击保存配置，核对确认内容。", "保存后到设备监控检查真实采样，确认运行中的采集是否采用新配置。"},
            "配置保存不代表串口通信成功；空寄存器列表不表示读数为零。", "先核对接线与设备参数；运行状态未确认时保留错误信息，不反复保存。", "devices", "打开设备监控"}},
        {"fieldbus.ethercat", {"需要唯一可用的 EtherCAT 接口和正确从站；写对象与周期测试均会产生现场操作。",
            {"填写从站、对象索引、子索引和十六进制数据。", "写对象时点击 CoE 写入并核对确认内容。", "需要周期测试时单独启动测试；本次为 1000 个周期、周期 1000 微秒。"},
            "测试完成与从站正常是不同结果，需查看真实从站响应与日志。", "接口不唯一或能力不可用时先修正设备配置；失败或未知结果先查日志。", "settings", "打开系统设置"}},
        {"network.wired", {"本页查询有线接口；地址配置由系统网络服务管理。",
            {"选择接口，核对名称、状态、IPv4 地址和来源。", "点击刷新连接信息，查看新的接口状态。", "有地址后进入目标应用验证连接。"},
            "网线连接、有 IP 地址和外部服务可达分别确认。", "本页不能修改 IP 地址；接口未出现时先检查系统网络服务与接线。", "cloud_bind", "打开官网绑定"}},
        {"network.wifi", {"本页查询无线接口；Wi-Fi 接入由系统网络服务管理。",
            {"选择无线接口，核对状态、地址与来源。", "点击刷新连接信息。", "进入需要联网的应用核对服务连接。"},
            "显示无线接口不表示已连接热点，获得地址也不代表云端可达。", "本页没有热点选择或密码配置；未接入时先在系统网络服务完成配置。", "cloud_bind", "打开官网绑定"}},
        {"network.cellular", {"确认所选蜂窝模块与对应接口；断开可能中断远程连接。",
            {"查看两张模块卡片的设备、接口和当前状态。", "在目标卡片选择拨号或断开，核对确认内容。", "等待结果，再刷新接口与地址；进入目标应用验证连接。"},
            "拨号命令完成不代表已获得地址或联网。", "模块映射变化或执行结果未知时先核对设备与日志，避免重复拨号。", "cloud_bind", "打开官网绑定"}},
        {"settings.display", {"编辑值与服务记录分开显示；只有保存并应用才提交设置。",
            {"修改亮度、样例开关或刷新频率。", "需要放弃编辑时点撤销修改并核对服务记录；默认仍继续编辑。", "点保存并应用，核对实际提交值，再查看底部保存和背光结果。"},
            "保存成功与背光已应用分别确认；编辑值不等于设备当前状态。", "失败保留编辑和最近结果；断线后重新核对，不重复提交旧确认。", "overview", "打开运行总览"}},
        {"settings.services", {"服务启动、重启、停止及整机重启会影响业务；先确认维护条件。",
            {"核对服务名称与当前状态。", "在对应服务卡片选择操作，核对确认框；整机重启是单独操作。", "等待结果并刷新状态，切到操作日志核对记录。"},
            "命令完成不等于服务健康或设备完成重启。", "连接中断或结果未知时先核对运行状态，避免再次重启。", "overview", "打开运行总览"}},
        {"settings.audit", {"本页显示已加载的最近操作记录；没有记录不代表系统从未执行操作。",
            {"选择全部、成功或失败筛选。", "点击刷新记录获取最近记录，选择一条后查看选中详情。", "对照时间、动作、目标与结果，核对对应设备的实际状态。"},
            "日志成功只表示该动作记录成功，不能替代现场结果验证。", "无匹配时先选择全部；获取失败检查连接，勿为生成日志而重复执行控制。", "overview", "打开运行总览"}},
        {"video.player", {"先选可读的视频，播放组件也需就绪；选择文件不会自动播放。",
            {"点击选择视频并进入文件夹，选中文件后返回。", "核对文件名，点击播放。", "使用暂停、静音、进度与全屏；离开前可停止播放。"},
            "以实际画面、声音与播放状态为准，选中与解码完成不同。", "文件消失或组件报错时先检查提示；取消选择保留原选择。", "settings", "打开系统设置"}},
        {"video.library", {"本地视频列表来自可用的视频目录；外接存储需要可读。",
            {"在列表核对文件名和大小。", "选择目标视频进入播放器；其他位置可使用选择视频浏览文件夹。", "核对选中名称，再点击播放。"},
            "列表出现或选择成功只说明文件可选，不证明可解码。", "文件不在列表时检查目录与存储；不要重复播放已消失的文件。", "settings", "打开系统设置"}},
        {"video.settings", {"播放设置影响当前播放器；解码组件状态需单独确认。",
            {"选择画面比例和循环播放选项。", "查看解码组件状态与错误提示。", "返回播放器后播放并核对画面比例和循环行为。"},
            "选项改变不代表播放已成功，需实际画面验证。", "组件不可用时先处理组件提示；不要靠切换比例或循环掩盖播放失败。", "settings", "打开系统设置"}},
        {"devices.trend", {"本页显示桌面卡片绑定的点位及最近 30 分钟趋势。",
            {"核对主点位和对照点位的名称、标识与单位。", "查看采样质量；已过期或暂不可用时不按当前读数判断。", "需要改变点位绑定时返回工作台编辑对应卡片。"},
            "空趋势和未绑定对照点位均不等于零读数；不同单位不能直接比较大小。", "数据不可用先检查来源与采集；这里不会启动新的硬件采集。", "fieldbus", "打开现场总线"}},
        {"alarms.all", {"当前页汇集已加载告警，仍受级别、来源和时间筛选影响。",
            {"选择记录查看详情。", "确认选中告警前核对设备和内容。", "导出当前筛选结果；结果为空可清除全部筛选。"},
            "确认仅表示已知悉，不会清除现场故障；导出以保存完成为准。", "离线保留记录不是最新状态；无匹配先清除筛选。", "devices", "打开设备监控"}},
        {"alarms.active", {"当前页仅显示未确认告警；先判断现场故障是否需要处置。",
            {"核对当前匹配数和其他筛选条件。", "选择记录阅读详情，处理现场问题后按需要确认。", "确认成功后记录可能从本页消失，可在已确认页核对。"},
            "从未确认列表消失只表示确认状态变化，不表示故障消除。", "没有记录时区分无匹配、无记录和离线；结果未知先刷新核对。", "devices", "打开设备监控"}},
        {"alarms.acknowledged", {"当前页是已知悉的告警记录，不表示所有现场故障已恢复。",
            {"选择记录查看时间、来源与内容。", "需要核对未处理记录时切到未确认。", "需要留存时导出当前筛选结果。"},
            "以现场状态判断故障恢复；已确认记录不需再次确认。", "筛选结果为空可清除筛选；离线时当前状态未确认。", "devices", "打开设备监控"}}
    };
    if (sections.contains(section)) return sections.value(section);
    return guides.value(id);
}

// Instruction and navigation only: no command execution or service writes.
class ApplicationGuide : public QDialog {
public:
    ApplicationGuide(const QString &id, const QString &location, QWidget *parent=nullptr, const QString &section = {})
        : QDialog(parent)
    {
        setObjectName("applicationGuide");
        setWindowTitle("使用指引");
        const auto guide=applicationHelp(id, section);
        setProperty("helpSection", section.isEmpty() ? id : section);
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
