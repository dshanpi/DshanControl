#pragma once
#include "desktopcanvas.h"

namespace DesktopV1 {
// A context view over the same bounded history used by the desktop cards.
// It never polls hardware, invents samples or owns a second history buffer.
class DesktopTrendPanel : public QWidget {
public:
    explicit DesktopTrendPanel(QWidget *parent=nullptr) : QWidget(parent) {
        setObjectName("boundTrendPanel");setMinimumHeight(350);
        auto *timer=new QTimer(this);
        connect(timer,&QTimer::timeout,this,[this](){if(isVisible())update();});
        timer->start(1000);
        expand=new QPushButton("展开已有采样",this);
        expand->setObjectName("expandTrendHistory");
        expand->setProperty("role","secondary");
        expand->setCheckable(true);expand->setMinimumHeight(48);
        connect(expand,&QPushButton::toggled,this,[this](bool checked) {
            expand->setText(checked?"恢复30分钟":"展开已有采样");update();
        });
    }
    void setSource(DesktopCanvas *value) { source=value;update(); }
    void setBinding(const QJsonObject &value) { binding=value;update(); }
    QJsonObject context() const { return binding; }
    bool expandedTimeRange() const { return expand->isChecked(); }
    QJsonObject reading(bool secondary=false) const {
        return source?source->pointById(binding[secondary?"secondary_point_id":"point_id"].toString()):QJsonObject();
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        expand->setGeometry(width()<760?20:width()-232,width()<760?140:60,212,48);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(),QColor("#F3F6FB"));
        auto text=[&](const QRect &r,const QString &s,int pixels,const QColor &color,bool bold=false){
            QFont font("Dshan Sans");font.setPixelSize(pixels);font.setBold(bold);p.setFont(font);p.setPen(color);
            p.drawText(r,Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(font).elidedText(s,Qt::ElideMiddle,r.width()));
        };
        const bool narrow=width()<760;
        const int left=20, full=width()-40, column=(full-12)/2;
        for(int i=0;i<2;++i) {
            const auto point=reading(i==1);
            const auto id=binding[i?"secondary_point_id":"point_id"].toString();
            const int x=left+i*(column+12);
            text(QRect(x,8,column,24),i?"对照点位（可选）":"主点位（必选）",13,QColor("#71859D"));
            text(QRect(x,34,column,30),point["name"].toString(id.isEmpty()?QString("未绑定"):id),
                 18,QColor(i?"#159D85":"#1468FA"),true);
            if(narrow) {
                const QString value=point["quality"]=="good" && point["value"].isDouble()
                    ?QString::number(point["value"].toDouble(),'g',7)+" "+point["unit"].toString():QString("--");
                text(QRect(x,64,column,26),value,20,QColor("#142C4B"),true);
                text(QRect(x,92,column,22),id.isEmpty()?QString("未绑定"):
                     point["quality"]=="good"?QString("质量：有效采样"):
                     point["quality"]=="stale"?QString("质量：已过期"):QString("质量：暂不可用"),12,QColor("#71859D"));
                text(QRect(x,116,column,20),id,11,QColor("#71859D"));
            }
        }
        if(narrow) {
            text(QRect(244,142,width()-264,22),"仅调整时间轴显示范围",12,QColor("#71859D"));
            text(QRect(244,164,width()-264,22),"共享30分钟历史，不新增采集",12,QColor("#71859D"));
        } else
            text(QRect(left,68,full-228,24),"绑定在工作台调整；共享最近30分钟历史，不新增采集。",12,QColor("#71859D"));
        const int chartTop=narrow?200:112;
        QRectF chart(left,chartTop,narrow?full:full*.66,qMax(100,height()-chartTop-28));
        p.setBrush(Qt::white);p.setPen(QPen(QColor("#DCE6F2"),1));p.drawRoundedRect(chart,3,3);
        const QRectF plot=chart.adjusted(54,76,-54,-48);
        text(QRect(chart.x()+14,chart.y()+5,chart.width()-28,24),expandedTimeRange()?"已有采样 · 时间轴展开 · 各自量程":"点位趋势 · 最近 30 分钟 · 各自量程",16,QColor("#142C4B"),true);
        if(source)source->paintPointHistory(p,plot,binding,expandedTimeRange());
        else text(plot.toRect(),"桌面数据源不可用",16,QColor("#71859D"));
        if(!narrow) {
            const int x=chart.right()+12,w=width()-x-20;
            p.setBrush(Qt::white);p.setPen(QPen(QColor("#DCE6F2"),1));
            p.drawRoundedRect(QRectF(x,112,w,chart.height()),3,3);
            text(QRect(x+12,120,w-24,26),"点位信息",16,QColor("#142C4B"),true);
            for(int i=0;i<2;++i) {
                const auto point=reading(i==1);
                const auto id=binding[i?"secondary_point_id":"point_id"].toString();
                const int y=158+i*132;
                text(QRect(x+12,y,w-24,24),i?"对照点位":"主点位",14,QColor("#71859D"),true);
                const QString value=point["quality"]=="good" && point["value"].isDouble()
                    ?QString::number(point["value"].toDouble(),'g',7)+" "+point["unit"].toString():QString("--");
                text(QRect(x+12,y+28,w-24,30),value,23,QColor("#142C4B"),true);
                text(QRect(x+12,y+64,w-24,24),id.isEmpty()?QString("未绑定"):
                     point["quality"]=="good"?QString("质量：有效采样"):
                     point["quality"]=="stale"?QString("质量：已过期"):QString("质量：暂不可用"),13,QColor("#71859D"));
                text(QRect(x+12,y+90,w-24,22),id,11,QColor("#71859D"));
            }
        }
    }
private:
    QPushButton *expand=nullptr;
    QPointer<DesktopCanvas> source;
    QJsonObject binding;
};
}
