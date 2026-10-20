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
        text(QRect(20,8,width()-40,36),"绑定点位 · 设备监控",23,QColor("#142C4B"),true);
        text(QRect(20,44,width()-40,24),"查看已绑定点位的实时读数和最近 30 分钟趋势。",12,QColor("#71859D"));
        for(int i=0;i<2;++i) {
            const auto point=reading(i==1);const auto id=binding[i?"secondary_point_id":"point_id"].toString();
            const int x=20+i*(width()-40)/2,w=(width()-56)/2;
            text(QRect(x,78,w,28),point["name"].toString(id.isEmpty()?QString("未绑定对照点位"):id),16,QColor(i?"#159D85":"#1468FA"),true);
            const QString value=point["quality"]=="good"?QString::number(point["value"].toDouble(),'g',7)+" "+point["unit"].toString():QString("--");
            text(QRect(x,108,w,40),value,29,QColor("#142C4B"),true);
            text(QRect(x,150,w,24),id.isEmpty()?QString("无绑定"):id+" · "+(point["quality"]=="good"?QString("有效采样"):point["quality"]=="stale"?QString("已过期"):QString("暂不可用")),12,QColor("#71859D"));
        }
        QRectF chart(24,190,width()-48,qMax(80,height()-238));
        p.setPen(QPen(QColor("#DCE6F2"),1));
        for(int i=0;i<=4;++i)p.drawLine(QPointF(chart.left(),chart.top()+i*chart.height()/4),QPointF(chart.right(),chart.top()+i*chart.height()/4));
        if(source)source->paintPointHistory(p,chart,binding);
        else text(chart.toRect(),"桌面数据源不可用",16,QColor("#71859D"));
    }
private:
    QPointer<DesktopCanvas> source;
    QJsonObject binding;
};
}
