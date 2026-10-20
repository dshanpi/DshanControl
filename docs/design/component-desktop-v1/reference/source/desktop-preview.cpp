#include <QtWidgets>
#include <cmath>
#include <functional>

static const QColor Ink("#142C4B"), Muted("#71859D"), Blue("#1468FA"), Line("#E3EBF3"), Green("#159D85"), Amber("#C88525");
struct AppItem {QString name, icon, sub; QColor color; bool planned;};
static QList<AppItem> catalog(){return {
 {"运行总览","chart","系统与现场",Blue,false},{"现场总线","bus","CAN / RS485 / ECAT",QColor("#6472D8"),false},
 {"网络通信","wifi","连接管理",QColor("#309ABA"),false},{"设备监控","device","点位与趋势",QColor("#20A88D"),false},
 {"告警记录","bell","事件与确认",QColor("#D89B3B"),false},{"系统设置","settings","显示与服务",QColor("#698098"),false},
 {"官网绑定","cloud","设备与账号",QColor("#4386D8"),true},{"数据上报","upload","点位与 MQTT",QColor("#24A4B2"),true},
 {"语音对话","mic","涂鸦语音",QColor("#9981D4"),true},{"智能家庭","home","家庭设备",QColor("#DF9970"),true},
 {"视频播放","video","本地媒体",QColor("#6E8BCB"),true},{"应用更新","update","签名应用更新",QColor("#7484A9"),false}};}

class Desktop:public QWidget{
public:
 int state=0,desk=0,selected=0,brightness=72,group=0;QSet<int> hidden;QElapsedTimer pressAge;bool rearranged=false,removed=false,added=false;QString query,notice;
 QLineEdit *search;QPointF pressed;QList<QPair<QRectF,std::function<void()>>> hits;
 QPixmap media;
 Desktop(){setWindowTitle("DshanControl · 组件桌面设计预览");resize(1024,600);setMinimumSize(800,480);setAttribute(Qt::WA_AcceptTouchEvents);
  media.load("/home/ubuntu/T113-tina5v1.2-sdk/UI播放器1280x720.png");
  search=new QLineEdit(this);search->setPlaceholderText("搜索应用");search->setStyleSheet("QLineEdit{border:1px solid #e2e9f3;border-radius:12px;background:#f3f6fb;padding:8px 16px;color:#203958;font-size:14px;}");search->hide();
  connect(search,&QLineEdit::textChanged,this,[this](const QString &s){query=s;update();});
 }
 void change(int s){state=s;notice.clear();search->setVisible(s==2);if(s!=2){search->clear();query.clear();}update();}
 void txt(QPainter &p,qreal x,qreal y,qreal w,qreal h,QString s,int px=14,QColor c=Ink,bool bold=false,int align=Qt::AlignLeft|Qt::AlignVCenter){
  QFont f("Dshan Sans");f.setPixelSize(px);f.setWeight(bold?QFont::DemiBold:QFont::Normal);p.setFont(f);p.setPen(c);p.drawText(QRectF(x,y,w,h),align,s);
 }
 void rr(QPainter &p,QRectF r,QColor c,qreal radius=18,QColor border=Qt::transparent){p.setPen(border.alpha()?QPen(border,1):Qt::NoPen);p.setBrush(c);p.drawRoundedRect(r,radius,radius);}
 void card(QPainter &p,QRectF r,bool editing=false){rr(p,r.translated(0,4),QColor(64,95,134,12),20);rr(p,r,QColor(255,255,255,255),20);if(editing){p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#7AA9F9"),1.3,Qt::DashLine));p.drawRoundedRect(r.adjusted(1,1,-1,-1),19,19);}}
 void icon(QPainter &p,QString k,qreal x,qreal y,qreal size,QColor c=Blue){p.save();p.translate(x,y);p.scale(size/24,size/24);p.setBrush(Qt::NoBrush);p.setPen(QPen(c,1.65,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
  if(k=="chart"){p.drawLine(3,3,3,21);p.drawLine(3,21,22,21);QPolygonF a;a<<QPointF(6,16)<<QPointF(10,10)<<QPointF(14,13)<<QPointF(21,5);p.drawPolyline(a);}
  else if(k=="wifi"){p.drawArc(QRectF(2,5,20,16),30*16,120*16);p.drawArc(QRectF(6,10,12,10),30*16,120*16);p.setBrush(c);p.drawEllipse(QPointF(12,20),1,1);}
  else if(k=="bus"){p.drawRoundedRect(QRectF(8,2,8,6),1,1);p.drawLine(12,8,12,12);p.drawLine(4,12,20,12);for(int x:{4,12,20}){p.drawLine(x,12,x,16);p.drawRect(QRectF(x-3,16,6,5));}}
  else if(k=="device"){p.drawRoundedRect(QRectF(2,3,20,14),2,2);p.drawLine(12,17,12,21);p.drawLine(7,21,17,21);p.drawLine(6,12,10,8);p.drawLine(10,8,14,11);p.drawLine(14,11,18,7);}
  else if(k=="bell"){QPainterPath a;a.moveTo(5,17);a.lineTo(7,14);a.lineTo(7,9);a.cubicTo(7,2,17,2,17,9);a.lineTo(17,14);a.lineTo(19,17);a.closeSubpath();p.drawPath(a);p.drawArc(QRectF(9,17,6,5),180*16,180*16);}
  else if(k=="cloud"||k=="upload"){QPainterPath a;a.moveTo(6,18);a.cubicTo(-2,18,1,8,7,10);a.cubicTo(6,0,19,0,19,10);a.cubicTo(25,10,25,18,18,18);a.closeSubpath();p.drawPath(a);if(k=="upload"){p.drawLine(12,19,12,10);p.drawLine(9,13,12,10);p.drawLine(12,10,15,13);}}
  else if(k=="video"){p.drawRoundedRect(QRectF(2,4,20,16),3,3);p.setBrush(c);QPolygonF a;a<<QPointF(10,8)<<QPointF(10,16)<<QPointF(16,12);p.drawPolygon(a);}
  else if(k=="image"){p.drawRoundedRect(QRectF(2,3,20,18),3,3);p.drawEllipse(QPointF(8,8),2,2);p.drawLine(4,18,11,11);p.drawLine(11,11,16,16);p.drawLine(16,16,20,12);}
  else if(k=="home"){QPolygonF a;a<<QPointF(2,11)<<QPointF(12,2)<<QPointF(22,11);p.drawPolyline(a);p.drawLine(5,9,5,22);p.drawLine(5,22,19,22);p.drawLine(19,22,19,9);p.drawRect(QRectF(10,14,5,8));}
  else if(k=="mic"){p.drawRoundedRect(QRectF(8,2,8,13),4,4);p.drawArc(QRectF(4,6,16,14),180*16,180*16);p.drawLine(12,20,12,23);}
  else if(k=="settings"){p.drawEllipse(QRectF(5,5,14,14));p.drawEllipse(QRectF(9,9,6,6));for(int i=0;i<8;i++){p.save();p.translate(12,12);p.rotate(i*45);p.drawLine(0,7,0,10);p.restore();}}
  else if(k=="update"){p.drawArc(QRectF(4,4,16,16),-30*16,290*16);p.drawLine(20,3,20,9);p.drawLine(20,9,15,8);}
  else if(k=="sun"){p.drawEllipse(QRectF(8,8,8,8));for(int i=0;i<8;i++){p.save();p.translate(12,12);p.rotate(i*45);p.drawLine(0,8,0,10);p.restore();}}
  else if(k=="grid"){for(int i=0;i<4;i++)p.drawRoundedRect(QRectF(3+(i%2)*11,3+(i/2)*11,7,7),1,1);}
  else if(k=="plus"){p.drawLine(12,4,12,20);p.drawLine(4,12,20,12);}
  else if(k=="up"||k=="down"){QPolygonF a;a<<QPointF(6,k=="up"?15:9)<<QPointF(12,k=="up"?9:15)<<QPointF(18,k=="up"?15:9);p.drawPolyline(a);}
  else if(k=="back"){p.drawLine(18,12,5,12);p.drawLine(5,12,11,6);p.drawLine(5,12,11,18);}
  p.restore();
 }
 void hit(QRectF r,std::function<void()> f){hits.append(qMakePair(r,f));}
 void pill(QPainter &p,QRectF r,QString s,bool primary=false){rr(p,r,primary?Blue:QColor("#EDF3FD"),r.height()/2);txt(p,r.x(),r.y(),r.width(),r.height(),s,14,primary?Qt::white:Blue,true,Qt::AlignCenter);}
 void title(QPainter &p,QRectF r,QString name,QString ico,QColor c=Blue){icon(p,ico,r.x()+18,r.y()+17,20,c);txt(p,r.x()+46,r.y()+12,r.width()-65,30,name,15,Ink,true);}
 void trend(QPainter &p,QRectF r,bool big=false){
  for(int i=0;i<3;i++){p.setPen(QPen(QColor("#E7EDF4"),1,Qt::DashLine));p.drawLine(QPointF(r.x(),r.y()+i*r.height()/2),QPointF(r.right(),r.y()+i*r.height()/2));}
  QPainterPath line;for(int i=0;i<=80;i++){qreal x=r.x()+r.width()*i/80; qreal y=r.y()+r.height()*(.5+.14*sin(i*.17)+.08*sin(i*.6)-i*.0015);if(i==0)line.moveTo(x,y);else line.lineTo(x,y);}
  QPainterPath area=line;area.lineTo(r.right(),r.bottom());area.lineTo(r.left(),r.bottom());area.closeSubpath();QLinearGradient g(0,r.top(),0,r.bottom());g.setColorAt(0,QColor(70,137,255,46));g.setColorAt(1,QColor(70,137,255,1));p.fillPath(area,g);p.setPen(QPen(Blue,big?3:2.5));p.drawPath(line);
  p.setPen(QPen(Qt::white,3));p.setBrush(Blue);p.drawEllipse(line.currentPosition(),4,4);
  txt(p,r.x(),r.bottom()+6,80,20,"14:00",11,Muted);txt(p,r.center().x()-30,r.bottom()+6,60,20,"14:15",11,Muted,false,Qt::AlignCenter);txt(p,r.right()-60,r.bottom()+6,60,20,"14:28",11,Muted,false,Qt::AlignRight|Qt::AlignVCenter);
 }
 void thumbnail(QPainter &p,QRectF r){
  p.save();QPainterPath clip;clip.addRoundedRect(r,12,12);p.setClipPath(clip);p.fillRect(r,QColor("#DCE7EC"));
  if(!media.isNull())p.drawPixmap(r,media,QRectF(225,215,695,300));
  p.restore();
 }
 void status(QPainter &p){
  p.fillRect(QRectF(0,0,1024,53),QColor(255,255,255,230));p.setPen(QPen(QColor("#DFE7F1"),1));p.drawLine(0,52,1024,52);
  rr(p,QRectF(22,12,28,28),Blue,8);txt(p,22,11,28,30,"D",22,Qt::white,true,Qt::AlignCenter);txt(p,60,12,155,28,"DshanControl",17,Ink,true);
  txt(p,230,12,105,28,"工业控制桌面",12,Muted);
  txt(p,391,10,65,30,"14:28",20,Ink,true);txt(p,464,12,170,28,"10月16日  星期五",13,Muted);
  icon(p,"wifi",739,16,20,Green);txt(p,765,12,100,28,"网络已连接",12,Ink);icon(p,"bell",879,15,20,Ink);rr(p,QRectF(893,12,7,7),QColor("#E49B3C"),4);
  pill(p,QRectF(920,12,80,28),"示例预览");rr(p,QRectF(490,47,44,3),QColor("#A6B7CA"),2);
  hit(QRectF(0,0,1024,53),[this](){change(state==1?0:1);});
 }
 void background(QPainter &p){
  QLinearGradient g(0,0,1024,600);g.setColorAt(0,QColor("#EAF2FD"));g.setColorAt(.55,QColor("#EEF3F9"));g.setColorAt(1,QColor("#DFE9F8"));p.fillRect(QRectF(0,0,1024,600),g);
  QRadialGradient a(1020,620,680);a.setColorAt(0,QColor(111,163,249,30));a.setColorAt(1,QColor(111,163,249,0));p.fillRect(QRectF(0,53,1024,547),a);
 }
 void footer(QPainter &p){
  rr(p,QRectF(0,542,1024,58),QColor(255,255,255,110),0);
  txt(p,24,558,230,24,"Qt5 设计预览 · 数据均为示例",11,Muted);
  rr(p,QRectF(355,563,7,7),desk==0?Blue:QColor("#B7C6D9"),4);rr(p,QRectF(372,563,7,7),desk==1?Blue:QColor("#B7C6D9"),4);
  hit(QRectF(330,542,65,58),[this](){desk=1-desk;change(0);});
  rr(p,QRectF(413,548,198,44),QColor(255,255,255,245),22,QColor("#DDE6F2"));icon(p,"up",430,558,22,Blue);txt(p,461,548,124,44,"所有应用",15,Ink,true);icon(p,"grid",579,561,18,Blue);hit(QRectF(413,542,198,58),[this](){change(2);});
  txt(p,795,556,205,26,"向上滑动，打开应用抽屉",12,Muted,false,Qt::AlignRight|Qt::AlignVCenter);
 }
 void desktop(QPainter &p,bool editing=false){
  txt(p,24,69,220,37,editing?"编辑工作台":"我的工作台",25,Ink,true);txt(p,213,76,250,28,editing?"拖动排列 · 自定义你的现场":"把关心的现场，放在眼前",12,Muted);
  pill(p,QRectF(838,72,162,36),editing?"完成编辑":"＋ 添加组件",editing);hit(QRectF(828,64,172,48),[this,editing](){change(editing?0:4);});
  QRectF tr(24,120,464,220),im(504,120,240,220),cl(760,120,240,220),bs(24,356,224,164),al(264,356,224,164),hm(504,356,240,164),vc(760,356,240,164);
  card(p,tr,editing);title(p,tr,"设备与趋势","chart");txt(p,415,134,55,24,"30 min",11,Muted,false,Qt::AlignRight|Qt::AlignVCenter);
  txt(p,44,166,132,42,"24.3",36,Ink,true);txt(p,133,180,42,25,"°C",17,Muted);txt(p,44,209,125,20,"环境温度",12,Muted);
  txt(p,223,166,130,42,"52.6",36,Ink,true);txt(p,313,180,30,25,"%",17,Muted);txt(p,223,209,130,20,"环境湿度",12,Muted);
  pill(p,QRectF(385,184,83,27),"采集中");trend(p,QRectF(44,244,424,55));
  hit(tr,[this,editing](){if(editing){rearranged=!rearranged;update();}else{selected=3;change(5);}});
  card(p,im,editing);title(p,im,"视频播放","video");thumbnail(p,QRectF(520,166,208,103));
  rr(p,QRectF(596,194,48,48),QColor(255,255,255,225),24);icon(p,"video",608,206,24,Blue);
  txt(p,522,280,200,23,"RGB50 接线演示",14,Ink,true);txt(p,522,305,190,20,"01:36 / 05:12 · 示例画面",11,Muted);hit(im,[this](){selected=10;change(5);});
  card(p,cl,editing);title(p,cl,"云端与上报","cloud");
  rr(p,QRectF(778,169,202,47),QColor("#EFF6FF"),12);icon(p,"upload",789,181,23,Blue);txt(p,824,173,145,21,"自动上报已启用",13,Ink,true);txt(p,824,194,145,17,"每 30 秒 · 示例",11,Muted);
  txt(p,779,234,148,22,"有效点位",12,Muted);txt(p,940,231,40,26,"12",23,Ink,true,Qt::AlignRight|Qt::AlignVCenter);
  txt(p,779,265,148,22,"最近发送",12,Muted);txt(p,913,265,66,22,"14:28",14,Ink,true,Qt::AlignRight|Qt::AlignVCenter);
  txt(p,779,302,200,21,"官网绑定与账号归属  ›",12,Blue);hit(cl,[this](){selected=7;change(5);});
  p.save();p.translate(rearranged?240:0,0);card(p,bs,editing);title(p,bs,"现场总线","bus",QColor("#6472D8"));
  QStringList rows={"CAN / CANopen","RS485 / Modbus","EtherCAT"};
  for(int i=0;i<3;i++){txt(p,43,407+i*31,135,23,rows[i],12,Ink);txt(p,169,407+i*31,61,23,i==1?"1 告警":"在线",12,i==1?Amber:Green,false,Qt::AlignRight|Qt::AlignVCenter);}
  p.restore();hit(bs.translated(rearranged?240:0,0),[this,editing](){if(editing){rearranged=!rearranged;update();}else{selected=1;change(5);}});
  p.save();p.translate(rearranged?-240:0,0);card(p,al,editing);title(p,al,"告警记录","bell",Amber);txt(p,284,405,53,47,"02",36,Ink,true);txt(p,342,413,106,25,"待确认",13,Amber);txt(p,284,457,185,20,"Modbus 从站响应超时",12,Ink);txt(p,284,484,183,20,"14:26  ·  查看事件  ›",11,Muted);p.restore();hit(al.translated(rearranged?-240:0,0),[this](){selected=4;change(5);});
  card(p,hm,editing);title(p,hm,"智能家庭","home",QColor("#D69065"));txt(p,523,405,195,25,"书房台灯",15,Ink,true);txt(p,523,433,190,21,"亮度 60% · 状态示例",11,Muted);
  rr(p,QRectF(675,409,48,27),QColor("#B4CFFC"),14);rr(p,QRectF(697,412,21,21),Qt::white,11);txt(p,523,482,180,21,"4 台设备 · 打开家庭  ›",12,Blue);hit(hm,[this](){selected=9;change(5);});
  card(p,vc,editing);title(p,vc,added?"图片组件":"语音对话",added?"image":"mic",QColor("#9981D4"));txt(p,779,409,195,24,added?"自定义设备图片":"今天，有什么需要帮忙？",13,Ink,true);
  for(int i=0;i<18;i++){qreal h=6+17*std::abs(std::sin(i*.73));rr(p,QRectF(782+i*10,449-h/2,3,h),QColor("#B5A7DE"),2);}
  txt(p,780,482,190,22,added?"从本地图片创建预览  ›":"按住说话  ·  设计入口",12,Muted);hit(vc,[this](){selected=8;change(5);});
  if(desk==1){card(p,tr,editing);title(p,tr,"设备图片","image");txt(p,286,133,183,26,"T113 · RGB50 接线参考",11,Muted,false,Qt::AlignRight|Qt::AlignVCenter);thumbnail(p,QRectF(43,167,426,153));}
  if(editing){int id=0;for(const auto &r:{tr,im,cl,bs,al,hm,vc}){rr(p,QRectF(r.right()-29,r.top()-9,28,28),QColor("#F5F8FD"),14,QColor("#C8D9EF"));txt(p,r.right()-29,r.top()-10,28,28,"−",21,Muted,true,Qt::AlignCenter);int n=id++;hit(QRectF(r.right()-44,r.top()-14,44,44),[this,n](){hidden.insert(n);update();});}txt(p,40,315,426,21,"拖动组件调整位置",11,Blue);}
  QList<QRectF> rects={tr,im,cl,bs,al,hm,vc};for(int n:hidden){card(p,rects[n],true);icon(p,"plus",rects[n].center().x()-12,rects[n].center().y()-24,24,Blue);txt(p,rects[n].left(),rects[n].center().y()+6,rects[n].width(),26,"添加组件",14,Blue,false,Qt::AlignCenter);hit(rects[n],[this](){change(4);});}
 }
 void dim(QPainter &p){p.fillRect(QRectF(0,53,1024,547),QColor(29,49,76,70));hit(QRectF(0,53,1024,547),[this](){change(0);});}
 void control(QPainter &p){
  dim(p);card(p,QRectF(24,64,976,423));hit(QRectF(24,64,976,423),[](){});
  txt(p,50,85,250,40,"控制中心",25,Ink,true);txt(p,50,128,370,24,"2026年10月16日  星期五",14,Muted);txt(p,448,91,120,46,"14:28",35,Ink,true,Qt::AlignRight|Qt::AlignVCenter);
  auto tile=[&](int x,int y,QString name,QString sub,QString ic,bool on){rr(p,QRectF(x,y,247,76),on?QColor("#EBF3FF"):QColor("#F3F6FA"),16);rr(p,QRectF(x+15,y+16,44,44),on?Blue:QColor("#D8E3F1"),14);icon(p,ic,x+26,y+27,22,on?Qt::white:Muted);txt(p,x+73,y+14,160,27,name,15,Ink,true);txt(p,x+73,y+42,162,22,sub,12,Muted);};
  tile(50,176,"有线网络","已连接 · 状态示例","bus",true);tile(313,176,"无线网络","已连接 · 状态示例","wifi",true);
  tile(50,266,"云端服务","自动上报 · 每 30 秒","cloud",true);tile(313,266,"通知声音","已开启","bell",false);
  rr(p,QRectF(50,360,510,65),QColor("#F1F5FA"),17);icon(p,"sun",68,380,25,Muted);txt(p,105,375,80,31,"屏幕亮度",13,Ink);
  rr(p,QRectF(200,389,274,7),QColor("#D7E2F0"),4);rr(p,QRectF(200,389,274*brightness/100.,7),Blue,4);rr(p,QRectF(190+274*brightness/100.,382,21,21),Qt::white,11,QColor("#B8CFF1"));txt(p,487,375,55,32,QString::number(brightness)+"%",13,Ink,true);
  hit(QRectF(190,360,294,65),[this](){brightness=brightness==72?45:72;update();});
  p.setPen(QPen(Line,1));p.drawLine(587,92,587,430);txt(p,614,93,320,30,"通知与现场事件",17,Ink,true);pill(p,QRectF(923,94,48,27),"2");
  rr(p,QRectF(613,145,360,109),QColor("#FFF7E9"),16);icon(p,"bell",630,162,23,Amber);txt(p,665,155,290,28,"Modbus 从站响应超时",14,Ink,true);txt(p,665,187,285,26,"14:26 · 待确认 · 示例",12,Amber);txt(p,665,217,285,23,"前往告警记录  ›",12,Blue);
  rr(p,QRectF(613,268,360,104),QColor("#F3F7FB"),16);icon(p,"cloud",630,285,24,Blue);txt(p,665,279,280,27,"官网账号尚未绑定",14,Ink,true);txt(p,665,309,275,26,"绑定设备后查看账号归属",12,Muted);txt(p,665,339,270,22,"打开云服务  ›",12,Blue);
  txt(p,612,397,359,25,"查看全部通知  →",13,Blue,false,Qt::AlignRight|Qt::AlignVCenter);
  icon(p,"up",500,451,24,Muted);hit(QRectF(440,435,144,52),[this](){change(0);});
 }
 void apps(QPainter &p){
  dim(p);card(p,QRectF(24,98,976,482));hit(QRectF(24,98,976,482),[](){});rr(p,QRectF(488,109,48,4),QColor("#C5D2E3"),2);
  txt(p,51,126,230,38,"所有应用",26,Ink,true);txt(p,190,133,160,26,"12 个功能入口",12,Muted);icon(p,"down",947,133,24,Muted);hit(QRectF(930,117,55,50),[this](){change(0);});
  QStringList cats={"全部","工业控制","网络与云","系统与媒体"};int xx[]={52,156,286,416};for(int n=0;n<4;n++){QRectF tab(xx[n],180,n==0?89:120,32);if(group==n)pill(p,tab,cats[n],true);else txt(p,tab.x(),tab.y(),tab.width(),tab.height(),cats[n],13,Muted,false,Qt::AlignCenter);hit(tab.adjusted(0,-6,0,6),[this,n](){group=n;update();});}
  int visible=0;auto list=catalog();for(int i=0;i<list.size();i++){auto a=list[i];if(group==1 && !(i==0||i==1||i==3||i==4))continue;if(group==2 && !(i==2||i==6||i==7||i==8||i==9))continue;if(group==3 && !(i==5||i==10||i==11))continue;if(!query.isEmpty()&&!(a.name+a.sub).contains(query,Qt::CaseInsensitive))continue;int x=52+(visible%6)*155,y=232+(visible/6)*143;visible++;
   rr(p,QRectF(x+31,y,64,64),a.color,20);icon(p,a.icon,x+48,y+17,30,Qt::white);txt(p,x,y+73,126,27,a.name,14,Ink,true,Qt::AlignCenter);txt(p,x,y+103,126,22,a.planned?"设计入口":a.sub,11,Muted,false,Qt::AlignCenter);hit(QRectF(x,y,126,128),[this,i](){selected=i;change(5);});}
  if(visible==0)txt(p,250,300,520,60,"没有匹配的应用",18,Muted,false,Qt::AlignCenter);
  txt(p,52,545,820,22,"点击进入完整应用 · 向下滑动收起 · 标记“设计入口”的功能沿用现有设计稿",11,Muted);
 }
 void library(QPainter &p){
  dim(p);card(p,QRectF(625,66,375,463));hit(QRectF(625,66,375,463),[](){});txt(p,650,84,230,36,"添加桌面组件",22,Ink,true);txt(p,650,125,310,22,"让应用以你需要的方式留在桌面",12,Muted);txt(p,950,87,30,30,"×",24,Muted);hit(QRectF(937,77,50,48),[this](){change(0);});
  QStringList names={"设备趋势","图片与视频","现场总线","云端数据","告警摘要","家庭与语音"};QStringList icons={"chart","image","bus","cloud","bell","mic"};QStringList desc={"折线图 / 数值 · 大组件","图片 / 缩略画面 · 中组件","通道状态 · 小组件","账号 / 点位上报 · 中组件","待确认事件 · 小组件","设备 / 对话 · 小组件"};
  for(int i=0;i<6;i++){int y=165+i*54;rr(p,QRectF(650,y,48,44),QColor("#EDF4FF"),13);icon(p,icons[i],663,y+9,22,Blue);txt(p,711,y-1,225,23,names[i],14,Ink,true);txt(p,711,y+23,220,20,desc[i],11,Muted);icon(p,"plus",950,y+10,20,Blue);hit(QRectF(646,y-3,333,51),[this,i](){added=i==1;hidden.clear();if(i==1)desk=1;change(3);});}
 }
 void detail(QPainter &p){
  auto a=catalog()[selected];txt(p,24,72,280,38,a.name,25,Ink,true);pill(p,QRectF(838,73,162,36),"← 返回桌面");hit(QRectF(828,63,172,50),[this](){change(0);});
  card(p,QRectF(24,126,636,390));card(p,QRectF(676,126,324,390));
  if(selected==10){thumbnail(p,QRectF(43,146,598,282));txt(p,44,440,400,30,"RGB50 接线演示",18,Ink,true);txt(p,44,478,440,23,"01:36 / 05:12 · 画面沿用已有播放器设计稿",12,Muted);}
  else{txt(p,46,148,350,30,"环境温度趋势",18,Ink,true);txt(p,46,194,250,57,"24.3 °C",42,Ink,true);txt(p,46,255,350,24,"最近 30 分钟 · 示例数据",13,Muted);trend(p,QRectF(47,308,588,150),true);}
  title(p,QRectF(676,126,324,390),"实时点位",a.icon);QStringList names={"环境温度","环境湿度","车间压力","门磁状态"};QStringList values={"24.3 °C","52.6 %","暂无数据","关闭"};for(int i=0;i<4;i++){int y=183+i*47;txt(p,698,y,150,30,names[i],14,Muted);txt(p,839,y,136,30,values[i],15,i==2?Amber:Ink,true,Qt::AlignRight|Qt::AlignVCenter);p.setPen(Line);p.drawLine(698,y+38,975,y+38);}txt(p,698,387,270,24,"采集间隔  1 秒",12,Muted);txt(p,698,414,270,22,"最近采集  14:28:00 · 示例",12,Muted);
  pill(p,QRectF(699,447,276,43),"返回组件桌面",true);hit(QRectF(699,440,276,53),[this](){change(0);});
 }
protected:
 void paintEvent(QPaintEvent*)override{QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);p.scale(width()/1024.,height()/600.);hits.clear();background(p);
  if(state==5)detail(p);else desktop(p,state==3);footer(p);status(p);if(state==1)control(p);if(state==2)apps(p);if(state==4)library(p);
  if(state==3){pill(p,QRectF(650,75,169,32),"＋ 添加组件");hit(QRectF(645,64,174,47),[this](){change(4);});}
  search->setGeometry(QRect(int(692*width()/1024.),int(126*height()/600.),int(228*width()/1024.),int(39*height()/600.)));
 }
 void mousePressEvent(QMouseEvent *e)override{pressAge.start();pressed=QPointF(e->pos().x()*1024./width(),e->pos().y()*600./height());}
 void mouseReleaseEvent(QMouseEvent *e)override{QPointF point(e->pos().x()*1024./width(),e->pos().y()*600./height());qreal dy=point.y()-pressed.y();
  if(state==0&&pressAge.elapsed()>600&&QLineF(pressed,point).length()<12){change(3);return;}if(state==3&&pressed.y()>356&&pressed.x()<488&&qAbs(point.x()-pressed.x())>100){rearranged=!rearranged;update();return;}
  if(pressed.y()<64&&dy>55){change(1);return;}if(pressed.y()>530&&dy< -55){change(2);return;}if(state==1&&dy< -55){change(0);return;}if(state==2&&dy>55){change(0);return;}
  if(state==0&&qAbs(point.x()-pressed.x())>100){desk=1-desk;update();return;}
  for(int i=hits.size()-1;i>=0;i--)if(hits[i].first.contains(point)){auto f=hits[i].second;f();break;}
 }
 void mouseDoubleClickEvent(QMouseEvent*)override{if(state==0)change(3);}
 void keyPressEvent(QKeyEvent *e)override{if(e->key()==Qt::Key_Escape)change(0);if(e->key()==Qt::Key_F2)change(3);}
};

int main(int argc,char**argv){QApplication app(argc,argv);QFontDatabase::addApplicationFont("/home/ubuntu/DshanControl/components/hmi/src/fonts/DshanSans-Regular.otf");QFontDatabase::addApplicationFont("/home/ubuntu/DshanControl/components/hmi/src/fonts/DshanSans-Bold.otf");
 Desktop w;QStringList args=app.arguments();
 if(args.contains("--self-test")){
  w.show();app.processEvents();
  auto drag=[&](QPointF a,QPointF b){QMouseEvent press(QEvent::MouseButtonPress,a,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&w,&press);QMouseEvent release(QEvent::MouseButtonRelease,b,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&w,&release);app.processEvents();};
  auto check=[](bool ok,const char *name){if(!ok)qFatal("FAIL: %s",name);qInfo("PASS: %s",name);};
  drag(QPointF(500,24),QPointF(500,170));check(w.state==1,"top swipe opens control center");
  drag(QPointF(512,458),QPointF(512,458));check(w.state==0,"control center closes");
  drag(QPointF(512,570),QPointF(512,340));check(w.state==2,"bottom swipe opens applications");
  w.search->setText(QString::fromUtf8("网络"));app.processEvents();check(w.query==QString::fromUtf8("网络"),"application search");w.search->clear();
  drag(QPointF(205,196),QPointF(205,196));check(w.group==1,"application category selection");
  drag(QPointF(512,220),QPointF(512,400));check(w.state==0,"application drawer swipe closes");
  w.change(3);app.processEvents();drag(QPointF(100,430),QPointF(350,430));check(w.rearranged,"widget position swap");
  drag(QPointF(473,124),QPointF(473,124));check(w.hidden.contains(0),"widget removal");
  w.change(4);app.processEvents();drag(QPointF(957,230),QPointF(957,230));check(w.desk==1&&w.hidden.isEmpty()&&w.state==3,"image widget selection");
  w.change(0);app.processEvents();for(int i=0;i<30;i++){w.change(i%6);app.processEvents();QImage frame(w.size(),QImage::Format_ARGB32);w.render(&frame);check(!frame.isNull(),"native state render");}
  return 0;
 }
 int out=args.indexOf("--render");if(out>=0&&out+1<args.size()){
  QString dir=args[out+1];QDir().mkpath(dir);QList<QPair<QString,int>> shots={{"UI新桌面_01组件桌面.png",0},{"UI新桌面_02顶部下拉.png",1},{"UI新桌面_03全部应用.png",2},{"UI新桌面_04编辑组件.png",3},{"UI新桌面_05添加组件.png",4},{"UI新桌面_06组件展开.png",5}};
  QStringList files;for(const auto &s:shots){w.selected=3;w.change(s.second);w.show();app.processEvents();QImage image(w.size(),QImage::Format_ARGB32);image.fill(Qt::transparent);w.render(&image);if(!image.save(dir+"/"+s.first))return 1;files<<s.first;}
  w.resize(1280,720);w.change(0);app.processEvents();QImage wide(w.size(),QImage::Format_ARGB32);wide.fill(Qt::transparent);w.render(&wide);wide.save(dir+"/UI新桌面_07宽屏1280x720.png");
  w.resize(1024,600);w.desk=1;w.change(0);app.processEvents();QImage custom(w.size(),QImage::Format_ARGB32);custom.fill(Qt::transparent);w.render(&custom);custom.save(dir+"/UI新桌面_09图片组件.png");w.desk=0;w.change(2);w.search->setText("网络");app.processEvents();QImage filtered(w.size(),QImage::Format_ARGB32);filtered.fill(Qt::transparent);w.render(&filtered);filtered.save(dir+"/UI新桌面_08应用搜索.png");
  QImage board(2136,2028,QImage::Format_RGB32);board.fill(QColor("#E6EDF6"));QPainter p(&board);QStringList titles={"01 / 组件桌面","02 / 顶部下拉：控制中心","03 / 底部上拉：全部应用","04 / 编辑组件","05 / 添加组件库","06 / 组件展开"};for(int i=0;i<files.size();i++){int x=28+(i%2)*1052,y=20+(i/2)*668;w.txt(p,x,y,1024,38,titles[i],22,Ink,true);p.drawImage(QRectF(x,y+46,1024,600),QImage(dir+"/"+files[i]));}p.end();board.save(dir+"/UI新桌面_00设计总览.png");return 0;
 }w.show();return app.exec();}
