#include "applicationshutdown.h"
#include <QJsonObject>
#include <QPointer>
#include <cstdio>
class HmiUiTest { public:
 static QProcess* setup(MediaSession* session,const QString& exe) { session->executable=exe;return session->process; }
};
class CloseGate : public QWidget {
 public: bool acceptClose=true;
 protected: void closeEvent(QCloseEvent* event) override;
};
#include <QCloseEvent>
void CloseGate::closeEvent(QCloseEvent* event) { if(acceptClose)event->accept();else event->ignore(); }
int main(int argc,char** argv) {
 QApplication app(argc,argv);
 const auto args=app.arguments();if(args.size()!=3)return 2;
 const QString mode=args[1];
 QWidget* window=new CloseGate;
 QPointer<CloseGate> gate=static_cast<CloseGate*>(window);
 ApplicationShutdown shutdown(app,window);
 QList<QPointer<QProcess>> children;
 int ready=0,finished=0,normal=0;bool started=false,failed=false,guardObserved=false;
 const int count=mode=="empty"?0:mode=="multiple"?2:1;
 auto closeWindow=[&]() {
   if(mode=="ignored")gate->acceptClose=false;
   if(mode=="hidden")window->hide();else window->close();
   if(mode=="ignored"||mode=="hidden") {
     QTimer::singleShot(100,&app,[&]() {
       if(!window||!gate||MediaSession::hasRetiringWorkers()||children[0]->state()!=QProcess::Running)failed=true;
       guardObserved=!failed;
       if(gate)gate->acceptClose=true;
       if(window){window->show();window->close();}
     });
   }
   // Repeated close notifications must not delete or retire anything twice.
   QTimer::singleShot(150,&app,[&](){shutdown.requestShutdown();});
 };
 for(int i=0;i<count;++i) {
   auto* s=new MediaSession(window);QProcess* child=HmiUiTest::setup(s,args[2]);children.append(child);
   QObject::connect(child,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),&app,[&](int rc,QProcess::ExitStatus status) {
     ++finished;if(rc==0&&status==QProcess::NormalExit)++normal;
     std::printf("child_finished rc=%d status=%d\n",rc,int(status));
   });
   QObject::connect(s,&MediaSession::changed,&app,[&,s](const QJsonObject& state) {
     if(state["state"]!="idle"||s->property("ready").toBool())return;
     s->setProperty("ready",true);++ready;
     if(ready==count&&!started){started=true;QTimer::singleShot(0,&app,closeWindow);}
   });
   s->start(true);
 }
 window->show();
 if(!count)QTimer::singleShot(0,&app,closeWindow);
 // Only this harness's QProcess objects are touched on a test deadline.
 QTimer::singleShot(7000,&app,[&](){failed=true;for(auto child:children)if(child)child->kill();QTimer::singleShot(100,&app,&QCoreApplication::quit);});
 int rc=app.exec();delete window;
 const bool pass=rc==0&&!failed&&!shutdown.deadlineExpired()&&finished==count&&
    (mode=="wedged"?normal==0:normal==count)&&
    ((mode!="ignored"&&mode!="hidden")||guardObserved)&&!MediaSession::hasRetiringWorkers();
 std::printf("PASS=%d count=%d finished=%d normal=%d guard=%d deadline=%d\n",pass,count,finished,normal,guardObserved,shutdown.deadlineExpired());
 return pass?0:42;
}
