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
    }
    void setSource(DesktopCanvas *value) { source=value;update(); }
    void setBinding(const QJsonObject &value) { binding=value;update(); }
    QJsonObject context() const { return binding; }
    QJsonObject reading(bool secondary=false) const {
        return source?source->pointById(binding[secondary?"secondary_point_id":"point_id"].toString()):QJsonObject();
    }
protected:
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
        text(QRect(left,narrow?140:68,full,24),"绑定在工作台调整；本页读取同一份最近 30 分钟历史，不新增采集。",12,QColor("#71859D"));
        const int chartTop=narrow?176:112;
        QRectF chart(left,chartTop,narrow?full:full*.66,qMax(100,height()-chartTop-28));
        p.setBrush(Qt::white);p.setPen(QPen(QColor("#DCE6F2"),1));p.drawRoundedRect(chart,3,3);
        const QRectF plot=chart.adjusted(14,32,-14,-20);
        text(QRect(chart.x()+14,chart.y()+5,chart.width()-28,24),"点位趋势 · 最近 30 分钟",16,QColor("#142C4B"),true);
        if(source)source->paintPointHistory(p,plot,binding);
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
    QPointer<DesktopCanvas> source;
    QJsonObject binding;
};
}
