#include "desktopcanvas.h"
#include <cmath>
namespace DesktopV1
{
static const QColor Ink("#142C4B"), Muted("#71859D"), Blue("#1468FA"), Line("#E3EBF3"),
    Green("#159D85"), Amber("#C88525");
void DesktopCanvas::status(QPainter &p)
{
    p.fillRect(QRectF(0, 0, 1024, 53), QColor(255, 255, 255, 230));
    p.setPen(QPen(QColor("#DFE7F1"), 1));
    p.drawLine(0, 52, 1024, 52);
    rr(p, QRectF(22, 12, 28, 28), Blue, 8);
    txt(p, 22, 11, 28, 30, "D", 22, Qt::white, true, Qt::AlignCenter);
    txt(p, 60, 12, 155, 28, "DshanControl", 17, Ink, true);
    txt(p, 230, 12, 105, 28, "工业控制桌面", 12, Muted);
    txt(p, 391, 10, 65, 30, model.clock, 20, Ink, true);
    txt(p, 464, 12, 170, 28, model.date, 13, Muted);
    icon(p, "wifi", 739, 16, 20, model.networkOnline ? Green : Muted);
    txt(p, 765, 12, 100, 28, model.connection, 12, Ink);
    icon(p, "bell", 879, 15, 20, Ink);
    if (model.fixture || model.alarmCount.toInt() > 0)
        rr(p, QRectF(893, 12, 7, 7), QColor("#E49B3C"), 4);
    pill(p, QRectF(920, 12, 80, 28), model.badge);
    rr(p, QRectF(490, 47, 44, 3), QColor("#A6B7CA"), 2);
    hit(QRectF(0, 0, 1024, 53), [this]() { change(state == 1 ? 0 : 1); });
}
void DesktopCanvas::footer(QPainter &p)
{
    rr(p, QRectF(0, 542, 1024, 58), QColor(255, 255, 255, 110), 0);
    txt(p, 24, 558, 230, 24, model.footer, 11, Muted);
    rr(p, QRectF(355, 563, 7, 7), desk == 0 ? Blue : QColor("#B7C6D9"), 4);
    rr(p, QRectF(372, 563, 7, 7), desk == 1 ? Blue : QColor("#B7C6D9"), 4);
    hit(QRectF(330, 542, 65, 58), [this]() {
        change(0);
        if (state != 0) return;
        desk = 1 - desk;
        hits.clear();
        update();
    });
    rr(p, QRectF(413, 548, 198, 44), QColor(255, 255, 255, 245), 22, QColor("#DDE6F2"));
    icon(p, "up", 430, 558, 22, Blue);
    txt(p, 461, 548, 124, 44, "所有应用", 15, Ink, true);
    icon(p, "grid", 579, 561, 18, Blue);
    hit(QRectF(413, 542, 198, 58), [this]() { change(2); });
    txt(p, 795, 556, 205, 26, "向上滑动，打开应用抽屉", 12, Muted, false,
        Qt::AlignRight | Qt::AlignVCenter);
}
void DesktopCanvas::desktop(QPainter &p, bool editing)
{
    txt(p, 24, 69, 220, 37, editing ? "编辑工作台" : "我的工作台", 25, Ink, true);
    txt(p, 213, 76, 250, 28, editing ? "拖动排列 · 自定义你的现场" : "把关心的现场，放在眼前", 12,
        Muted);
    pill(p, QRectF(838, 72, 162, 36),
         editing ? QString("完成编辑")
                 : QString("＋ 添加组件"),
         editing);
    hit(QRectF(828, 64, 172, 44), [this, editing]() {
        if (editing)
        {
            commitLayout();
        }
        else
        {
            change(4);
        }
    });
    const auto rects = slotRects();
    for (int n = 0; n < rects.size(); ++n) {
        const auto r = rects[n];
        const QString type = widgetAt(n)["type"].toString();
        if(!model.fixture && editing && n==ghostSlot)card(p,r,true);
        else drawWidget(p, type, r, editing, widgetAt(n)["binding"].toObject());
        if (type == "empty") {
            icon(p, "plus", r.center().x()-12, r.center().y()-24, 24, Blue);
            txt(p, r.left(), r.center().y()+6, r.width(), 26, "添加组件", 14, Blue, false, Qt::AlignCenter);
            hit(r, [this,n]() { librarySlot=n; change(4); });
        } else if (editing && (type == "image" || type == "trend" || type == "video" || type == "home") && !model.fixture) {
            hit(r,[this,n,type]() { if(type=="image")editImageBinding(n); else if(type=="video")editVideoBinding(n); else if(type=="home")editHomeBinding(n); else editTrendBinding(n); });
        } else if (!editing) {
            hit(r,[this,n]() { openWidget(n); });
            if(type=="cloud") hit(QRectF(r.x()+12,r.y()+172,r.width()-24,44),[this](){openApp(6);});
        }
    }
    if (editing) {
        for (int n=0; n<rects.size(); ++n) {
            const auto r=rects[n];
            rr(p,QRectF(r.right()-29,r.top()-9,28,28),QColor("#F5F8FD"),14,QColor("#C8D9EF"));
            txt(p,r.right()-29,r.top()-10,28,28,"−",21,Muted,true,Qt::AlignCenter);
            // Keep the full delete target below the 44px editor toolbar targets.
            // Painting stays unchanged; overlapping targets dispatch to the later one.
            hit(QRectF(r.right()-44,r.top()-10,44,44),[this,n]() {
                if(model.fixture) hidden.insert(n); else removeWidget(n);
                update();
            });
        }
        txt(p,40,315,426,21,"拖动组件调整位置",11,Blue);
        if(!model.fixture && dragTarget>=0 && dragTarget<rects.size()) {
            p.setPen(QPen(Blue,2,Qt::DashLine));p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rects[dragTarget].adjusted(2,2,-2,-2),18,18);
        }
    }
    // Fixture removal state is in-memory only; never touches user layout storage.
    if(model.fixture) for(int n:hidden) {
        const auto r=rects[n];card(p,r,true);
        icon(p,"plus",r.center().x()-12,r.center().y()-24,24,Blue);
        txt(p,r.left(),r.center().y()+6,r.width(),26,"添加组件",14,Blue,false,Qt::AlignCenter);
        hit(r,[this](){change(4);});
    }
}

void DesktopCanvas::drawWidget(QPainter &p, const QString &type, const QRectF &target, bool editing, const QJsonObject &binding)
{
    if(type=="empty") { card(p,target,true); return; }
    const QMap<QString,int> sourceIndex={{"trend",0},{"image",target.height()>200 && target.width()>400?0:1},
        {"video",1},{"cloud",2},{"bus",3},{"alarms",4},{"home",5},{"voice",6}};
    const auto source=slotRects()[sourceIndex.value(type,0)];
    p.save();p.translate(target.topLeft()-source.topLeft());
    QRectF tr(24,120,target.width(),target.height()), im(504,120,target.width(),target.height()),
        cl(760,120,target.width(),target.height()), bs(24,356,target.width(),target.height()),
        al(264,356,target.width(),target.height()), hm(504,356,target.width(),target.height()),
        vc(760,356,target.width(),target.height());
    if(type=="trend" || (type=="image" && target.width()>400)) {
    card(p, tr, editing);
    title(p, tr, "设备与趋势", "chart");
    txt(p, 415, 134, 55, 24, "30 min", 11, Muted, false, Qt::AlignRight | Qt::AlignVCenter);
    const auto first=pointHistory.current(binding["point_id"].toString(),QDateTime::currentMSecsSinceEpoch(),pointClock.elapsed());
    const auto second=pointHistory.current(binding["secondary_point_id"].toString(),QDateTime::currentMSecsSinceEpoch(),pointClock.elapsed());
    auto numeric=[](const QJsonObject &point){if(point["quality"]!="good")return QString("--");const double v=point["value"].toDouble();return std::abs(v)<10000?QString::number(v,'f',1):QString::number(v,'g',4);};
    auto metric=[&](int x,const QString &value) {
        QFont font("Dshan Sans");font.setWeight(QFont::DemiBold);
        int pixels=36;do {font.setPixelSize(pixels);if(QFontMetrics(font).horizontalAdvance(value)<=84)break;}while(--pixels>18);
        txt(p,x,166,88,42,value,pixels,Ink,true);
    };
    if(model.fixture)txt(p,44,166,132,42,model.temperature,36,Ink,true);else metric(44,numeric(first));
    txt(p, 133, 180, 42, 25, model.fixture?QString("°C"):first["unit"].toString(), 17, Muted);
    txt(p, 44, 209, 125, 20, model.fixture?QString("环境温度"):first["name"].toString("主点位未绑定"), 12, Muted);
    if(model.fixture)txt(p,223,166,130,42,model.humidity,36,Ink,true);else metric(223,numeric(second));
    txt(p, 313, 180, 30, 25, model.fixture?QString("%"):second["unit"].toString(), 17, Muted);
    txt(p, 223, 209, 130, 20, model.fixture?QString("环境湿度"):second["name"].toString("对照点位未绑定"), 12, Muted);
    const QString quality=first["quality"].toString();
    pill(p, QRectF(385, 184, 83, 27), model.fixture?model.collection:quality=="good"?QString("有效采样"):quality=="stale"?QString("数据过期"):quality=="error"?QString("数据无效"):binding.isEmpty()?QString("未绑定"):QString("待获取"));
    if(model.fixture)trend(p, QRectF(44, 244, 424, 55));
    else drawPointTrend(p,QRectF(44,244,424,55),binding);


    }
    else if(type=="video") {
    card(p, im, editing);
    title(p, im, "视频播放", "video");
    thumbnail(p, QRectF(520, 166, 208, 103));
    rr(p, QRectF(596, 194, 48, 48), QColor(255, 255, 255, 225), 24);
    icon(p, "video", 608, 206, 24, Blue);
    txt(p, 522, 280, 200, 23, !model.fixture && !binding["path"].toString().isEmpty()
        ? QFontMetrics(p.font()).elidedText(QFileInfo(binding["path"].toString()).fileName(),Qt::ElideMiddle,180)
        : model.mediaTitle, 14, Ink, true);
    txt(p, 522, 305, 190, 20, !model.fixture && !binding["path"].toString().isEmpty()
        ? QString("已绑定 · 打开后点击播放") : model.mediaTime, 11, Muted);


    }
    else if(type=="cloud") {
    card(p, cl, editing);
    title(p, cl, "云端与上报", "cloud");
    rr(p, QRectF(778, 169, 202, 47), QColor("#EFF6FF"), 12);
    icon(p, "upload", 789, 181, 23, Blue);
    txt(p, 824, 173, 145, 21, model.cloudState, 13, Ink, true);
    txt(p, 824, 194, 145, 17, model.cloudPeriod, 11, Muted);
    txt(p, 779, 234, 148, 22, "有效点位", 12, Muted);
    txt(p, 940, 231, 40, 26, model.pointCount, 23, Ink, true, Qt::AlignRight | Qt::AlignVCenter);
    txt(p, 779, 265, 148, 22, "最近发送", 12, Muted);
    txt(p, 913, 265, 66, 22, model.lastSend, 14, Ink, true, Qt::AlignRight | Qt::AlignVCenter);
    txt(p, 779, 302, 200, 21, "官网绑定与账号归属  ›", 12, Blue);



    }
    else if(type=="bus") {
    card(p, bs, editing);
    title(p, bs, "现场总线", "bus", QColor("#6472D8"));
    QStringList rows = {"CAN / CANopen", "RS485 / Modbus", "EtherCAT"};
    for (int i = 0; i < 3; i++)
    {
        txt(p, 43, 407 + i * 31, 135, 23, rows[i], 12, Ink);
        txt(p, 169, 407 + i * 31, 61, 23, model.busStates.value(i, "--"), 12,
            model.fixture ? (i == 1 ? Amber : Green) : Muted, false,
            Qt::AlignRight | Qt::AlignVCenter);
    }

    }
    else if(type=="alarms") {
    card(p, al, editing);
    title(p, al, "告警记录", "bell", Amber);
    txt(p, 284, 405, 53, 47, model.alarmCount, 36, Ink, true);
    txt(p, 342, 413, 106, 25, model.alarmState, 13, Amber);
    txt(p, 284, 457, 185, 20, model.alarmText, 12, Ink);
    txt(p, 284, 484, 183, 20, model.alarmTime, 11, Muted);

    }
    else if(type=="home") {
    card(p, hm, editing);
    title(p, hm, "智能家庭", "home", QColor("#D69065"));
    QJsonObject bound;
    const auto id=binding["device_id"].toString();
    for(const auto &entry:homeSnapshot["devices"].toArray())
        if(entry.toObject()["id"]==id)bound=entry.toObject();
    const auto homeName=model.fixture || id.isEmpty()?model.homeName:
        bound.isEmpty()?QString("已绑定设备不可用"):bound["name"].toString();
    const auto homeStatus=model.fixture || id.isEmpty()?model.homeStatus:
        homeSnapshot["fresh"]!=true?QString("状态过期 · 打开家庭同步"):
        bound.isEmpty()?QString("设备未返回，绑定已保留"):
        bound["online"]==true?QString("在线 · 点击查看真实状态"):QString("离线 · 暂不可控制");
    txt(p, 523, 405, 195, 25, homeName, 15, Ink, true);
    txt(p, 523, 433, 190, 21, homeStatus, 11, Muted);
    if (model.fixture)
    {
        rr(p, QRectF(675, 409, 48, 27), QColor("#B4CFFC"), 14);
        rr(p, QRectF(697, 412, 21, 21), Qt::white, 11);
    }
    txt(p, 523, 482, 180, 21, model.homeCount, 12, Blue);


    }
    else if(type=="voice") {
    card(p, vc, editing);
    title(p, vc, added ? "图片组件" : "语音对话", added ? "image" : "mic", QColor("#9981D4"));
    txt(p, 779, 409, 195, 24, added ? "自定义设备图片" : model.voiceTitle, 13, Ink, true);
    for (int i = 0; i < (model.fixture ? 18 : 0); i++)
    {
        qreal h = 6 + 17 * std::abs(std::sin(i * .73));
        rr(p, QRectF(782 + i * 10, 449 - h / 2, 3, h), QColor("#B5A7DE"), 2);
    }
    txt(p, 780, 482, 190, 22, added ? "从本地图片创建预览  ›" : model.voiceState, 12, Muted);


    }
    if(type=="image" && target.width()>400) {

        card(p, tr, editing);
        title(p, tr, "设备图片", "image");
        txt(p, 286, 133, 183, 26,
            model.fixture ? QString("T113 · RGB50 接线参考") : (binding["path"].toString().isEmpty()?QString("尚未绑定图片"):QFileInfo(binding["path"].toString()).fileName()), 11, Muted,
            false, Qt::AlignRight | Qt::AlignVCenter);
        drawBoundImage(p, QRectF(43, 167, 426, 153),binding);

    } else if(type=="image") {
        card(p,im,editing);title(p,im,"设备图片","image");drawBoundImage(p,QRectF(520,166,208,103),binding);
        txt(p,522,280,200,23,model.fixture?QString("设备图片"):(binding["path"].toString().isEmpty()?QString("尚未绑定图片"):QFileInfo(binding["path"].toString()).fileName()),14,Ink,true);
    }
    p.restore();
}
void DesktopCanvas::dim(QPainter &p)
{
    if(skipDim)return;
    p.fillRect(QRectF(0, 53, 1024, 547), QColor(29, 49, 76, 70));
    hit(QRectF(0, 53, 1024, 547), [this]() { if(state==4) closeLibrary(); else change(0); });
}
void DesktopCanvas::control(QPainter &p)
{
    dim(p);
    card(p, QRectF(24, 64, 976, 423));
    hit(QRectF(24, 64, 976, 423), []() {});
    txt(p, 50, 85, 250, 40, "控制中心", 25, Ink, true);
    txt(p, 50, 128, 370, 24, model.controlDate, 14, Muted);
    txt(p, 448, 91, 120, 46, model.clock, 35, Ink, true, Qt::AlignRight | Qt::AlignVCenter);
    auto tile = [&](int x, int y, QString name, QString sub, QString ic, bool on) {
        rr(p, QRectF(x, y, 247, 76), on ? QColor("#EBF3FF") : QColor("#F3F6FA"), 16);
        rr(p, QRectF(x + 15, y + 16, 44, 44), on ? Blue : QColor("#D8E3F1"), 14);
        icon(p, ic, x + 26, y + 27, 22, on ? Qt::white : Muted);
        txt(p, x + 73, y + 14, 160, 27, name, 15, Ink, true);
        txt(p, x + 73, y + 42, 162, 22, sub, 12, Muted);
    };
    tile(50, 176, "有线网络", model.wired, "bus", model.wiredOnline);
    tile(313, 176, "无线网络", model.wireless, "wifi", model.wirelessOnline);
    tile(50, 266, "云端服务", model.controlCloud, "cloud", model.cloudOnline);
    tile(313, 266, "通知声音", model.sound, "bell", false);
    hit(QRectF(50, 176, 247, 76), [this]() { openApp(2); });
    hit(QRectF(313, 176, 247, 76), [this]() { openApp(2); });
    hit(QRectF(50, 266, 247, 76), [this]() { openApp(7); });
    hit(QRectF(613, 145, 360, 109), [this]() { openApp(4); });
    hit(QRectF(613, 268, 360, 104), [this]() { openApp(6); });
    rr(p, QRectF(50, 360, 510, 65), QColor("#F1F5FA"), 17);
    icon(p, "sun", 68, 380, 25, Muted);
    txt(p, 105, 375, 80, 31, "屏幕亮度", 13, Ink);
    rr(p, QRectF(200, 389, 274, 7), QColor("#D7E2F0"), 4);
    rr(p, QRectF(200, 389, 274 * qMax(0, brightness) / 100., 7), Blue, 4);
    rr(p, QRectF(190 + 274 * qMax(0, brightness) / 100., 382, 21, 21), Qt::white, 11,
       QColor("#B8CFF1"));
    txt(p, 487, 375, 55, 32, (brightness < 0 ? QString("--") : QString::number(brightness)) + "%",
        13, Ink, true);
    hit(QRectF(190, 360, 294, 65), [this]() {
        if (model.fixture)
        {
            brightness = brightness == 72 ? 45 : 72;
            update();
        }
    });
    if (!model.fixture && !brightnessMessage.isEmpty())
        txt(p, 50, 426, 510, 22, brightnessMessage, 11, brightnessUnconfirmed ? Amber : Muted);
    p.setPen(QPen(Line, 1));
    p.drawLine(587, 92, 587, 430);
    txt(p, 614, 93, 320, 30, "通知与现场事件", 17, Ink, true);
    pill(p, QRectF(923, 94, 48, 27), model.notificationCount);
    rr(p, QRectF(613, 145, 360, 109), QColor("#FFF7E9"), 16);
    icon(p, "bell", 630, 162, 23, Amber);
    txt(p, 665, 155, 290, 28, model.alarmText, 14, Ink, true);
    txt(p, 665, 187, 285, 26, model.fixture ? QString("14:26 · 待确认 · 示例") : model.alarmState,
        12, Amber);
    txt(p, 665, 217, 285, 23, "前往告警记录  ›", 12, Blue);
    rr(p, QRectF(613, 268, 360, 104), QColor("#F3F7FB"), 16);
    icon(p, "cloud", 630, 285, 24, Blue);
    txt(p, 665, 279, 280, 27,
        model.fixture ? QString("官网账号尚未绑定") : model.pairingNotice.isEmpty()?QString("官网归属待确认"):model.pairingNotice, 14, Ink, true);
    txt(p, 665, 309, 275, 26,
        model.fixture ? QString("绑定设备后查看账号归属") : QString("打开云服务查询实际归属"), 12,
        Muted);
    txt(p, 665, 339, 270, 22, "打开云服务  ›", 12, Blue);
    txt(p, 612, 397, 359, 25, "查看全部通知  →", 13, Blue, false,
        Qt::AlignRight | Qt::AlignVCenter);
    icon(p, "up", 500, 451, 24, Muted);
    hit(QRectF(440, 435, 144, 52), [this]() { change(0); });
}
void DesktopCanvas::apps(QPainter &p)
{
    dim(p);
    card(p, QRectF(24, 98, 976, 482));
    hit(QRectF(24, 98, 976, 482), []() {});
    rr(p, QRectF(488, 109, 48, 4), QColor("#C5D2E3"), 2);
    txt(p, 51, 126, 230, 38, "所有应用", 26, Ink, true);
    txt(p, 190, 133, 160, 26, "12 个功能入口", 12, Muted);
    icon(p, "down", 947, 133, 24, Muted);
    hit(QRectF(930, 117, 55, 50), [this]() { change(0); });
    QStringList cats = {"全部", "工业控制", "网络与云", "系统与媒体"};
    int xx[] = {52, 156, 286, 416};
    for (int n = 0; n < 4; n++)
    {
        QRectF tab(xx[n], 180, n == 0 ? 89 : 120, 32);
        if (group == n)
            pill(p, tab, cats[n], true);
        else
            txt(p, tab.x(), tab.y(), tab.width(), tab.height(), cats[n], 13, Muted, false,
                Qt::AlignCenter);
        hit(tab.adjusted(0, -6, 0, 6), [this, n]() {
            group = n;
            update();
        });
    }
    int visible = 0;
    auto list = catalog();
    for (int i = 0; i < list.size(); i++)
    {
        auto a = list[i];
        if (group == 1 && !(i == 0 || i == 1 || i == 3 || i == 4))
            continue;
        if (group == 2 && !(i == 2 || i == 6 || i == 7 || i == 8 || i == 9))
            continue;
        if (group == 3 && !(i == 5 || i == 10 || i == 11))
            continue;
        if (!query.isEmpty() && !(a.name + a.sub).contains(query, Qt::CaseInsensitive))
            continue;
        int x = 52 + (visible % 6) * 155, y = 232 + (visible / 6) * 143;
        visible++;
        rr(p, QRectF(x + 31, y, 64, 64), a.color, 20);
        icon(p, a.icon, x + 48, y + 17, 30, Qt::white);
        txt(p, x, y + 73, 126, 27, a.name, 14, Ink, true, Qt::AlignCenter);
        txt(p, x, y + 103, 126, 22,
            a.planned ? (model.fixture ? QString("设计入口") : QString("查看服务状态")) : a.sub, 11,
            Muted, false, Qt::AlignCenter);
        hit(QRectF(x, y, 126, 128), [this, i]() { openApp(i); });
    }
    if (visible == 0)
        txt(p, 250, 300, 520, 60, "没有匹配的应用", 18, Muted, false, Qt::AlignCenter);
    txt(p, 52, 545, 820, 22,
        model.fixture
            ? QString("点击进入完整应用 · 向下滑动收起 · 标记“设计入口”的功能沿用现有设计稿")
            : QString("点击进入完整应用 · 向下滑动收起 · 云与媒体入口展示实际服务状态"),
        11, Muted);
}
void DesktopCanvas::library(QPainter &p)
{
    dim(p);
    card(p, QRectF(625, 66, 375, 463));
    hit(QRectF(625, 66, 375, 463), []() {});
    txt(p, 650, 84, 230, 36, "添加桌面组件", 22, Ink, true);
    txt(p, 650, 125, 310, 22, "让应用以你需要的方式留在桌面", 12, Muted);
    txt(p, 950, 87, 30, 30, "×", 24, Muted);
    hit(QRectF(937, 77, 50, 48), [this]() { closeLibrary(); });
    if(!model.fixture && libraryGroup>=0) {
        p.fillRect(QRectF(648,122,315,30),Qt::white);
        txt(p,650,121,310,30,"← 返回组件分类",14,Blue);
        hit(QRectF(646,112,310,44),[this](){backLibraryLevel();});
        const QStringList types=libraryGroup==1?QStringList{"image","video"}:QStringList{"home","voice"};
        const QStringList labels=libraryGroup==1?QStringList{"设备图片","视频播放"}:QStringList{"智能家庭","语音对话"};
        for(int i=0;i<types.size();++i) {
            const int y=165+i*64;const bool enabled=canPlaceWidget(types[i]);
            rr(p,QRectF(650,y,48,44),QColor("#EDF4FF"),13);
            icon(p,types[i]=="voice"?QString("mic"):types[i],663,y+9,22,enabled?Blue:Muted);
            txt(p,711,y-1,225,23,labels[i],14,enabled?Ink:Muted,true);
            txt(p,711,y+23,220,20,enabled?QString("添加到当前空槽"):QString("请先移除一个兼容组件"),11,Muted);
            icon(p,"plus",950,y+10,20,enabled?Blue:Muted);
            if(enabled)hit(QRectF(646,y-3,333,51),[this,types,i](){placeWidget(types[i]);});
        }
        return;
    }
    QStringList names = {"设备趋势", "图片与视频", "现场总线",
                         "云端数据", "告警摘要",   "家庭与语音"};
    QStringList icons = {"chart", "image", "bus", "cloud", "bell", "mic"};
    QStringList desc = {"折线图 / 数值 · 大组件", "图片 / 缩略画面 · 中组件",
                        "通道状态 · 小组件",      "账号 / 点位上报 · 中组件",
                        "待确认事件 · 小组件",    "设备 / 对话 · 小组件"};
    for (int i = 0; i < 6; i++)
    {
        int y = 165 + i * 54;
        const QList<QStringList> candidates={{"trend"},{"image","video"},{"bus"},{"cloud"},{"alarms"},{"home","voice"}};
        bool enabled=model.fixture;
        for(const auto &type:candidates[i])enabled|=canPlaceWidget(type);
        rr(p, QRectF(650, y, 48, 44), QColor("#EDF4FF"), 13);
        icon(p, icons[i], 663, y + 9, 22, Blue);
        txt(p, 711, y - 1, 225, 23, names[i], 14, Ink, true);
        txt(p, 711, y + 23, 220, 20, desc[i], 11, Muted);
        icon(p, "plus", 950, y + 10, 20, enabled?Blue:Muted);
        if(!enabled) { p.fillRect(QRectF(711,y+23,220,20),Qt::white);txt(p,711,y+23,220,20,"请先移除一个兼容组件",11,Muted); }
        if(enabled) hit(QRectF(646, y - 3, 333, 51), [this, i]() {
            if (model.fixture)
            {
                added = i == 1;
                hidden.clear();
                if (i == 1)
                    desk = 1;
                change(3);
            }
            else chooseLibraryItem(i);
        });
    }
}
void DesktopCanvas::detail(QPainter &p)
{
    auto a = catalog()[selected];
    txt(p, 24, 72, 280, 38, a.name, 25, Ink, true);
    pill(p, QRectF(838, 73, 162, 36), "← 返回桌面");
    hit(QRectF(828, 63, 172, 50), [this]() { change(0); });
    card(p, QRectF(24, 126, 636, 390));
    card(p, QRectF(676, 126, 324, 390));
    if (selected == 10)
    {
        thumbnail(p, QRectF(43, 146, 598, 282));
        txt(p, 44, 440, 400, 30, "RGB50 接线演示", 18, Ink, true);
        txt(p, 44, 478, 440, 23, "01:36 / 05:12 · 画面沿用已有播放器设计稿", 12, Muted);
    }
    else
    {
        txt(p, 46, 148, 350, 30, "环境温度趋势", 18, Ink, true);
        txt(p, 46, 194, 250, 57, "24.3 °C", 42, Ink, true);
        txt(p, 46, 255, 350, 24, "最近 30 分钟 · 示例数据", 13, Muted);
        trend(p, QRectF(47, 308, 588, 150), true);
    }
    title(p, QRectF(676, 126, 324, 390), "实时点位", a.icon);
    QStringList names = {"环境温度", "环境湿度", "车间压力", "门磁状态"};
    QStringList values = {"24.3 °C", "52.6 %", "暂无数据", "关闭"};
    for (int i = 0; i < 4; i++)
    {
        int y = 183 + i * 47;
        txt(p, 698, y, 150, 30, names[i], 14, Muted);
        txt(p, 839, y, 136, 30, values[i], 15, i == 2 ? Amber : Ink, true,
            Qt::AlignRight | Qt::AlignVCenter);
        p.setPen(Line);
        p.drawLine(698, y + 38, 975, y + 38);
    }
    txt(p, 698, 387, 270, 24, "采集间隔  1 秒", 12, Muted);
    txt(p, 698, 414, 270, 22, "最近采集  14:28:00 · 示例", 12, Muted);
    pill(p, QRectF(699, 447, 276, 43), "返回组件桌面", true);
    hit(QRectF(699, 440, 276, 53), [this]() { change(0); });
}
} // namespace DesktopV1
