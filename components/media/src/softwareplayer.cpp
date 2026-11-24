// Software decode for boards without a Cedar display plane. Qt owns all painting;
// bounded RGB frames cross a private shared-memory segment, never /dev/fb0 writes.
#include "../../runtime/src/storage.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSharedMemory>
#include <QSocketNotifier>
#include <QTimer>
#include <QUuid>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr int Width=512, Height=288, FrameBytes=Width*Height*3;
class Decoder : public QProcess {
    const pid_t owner=::getpid();
    void setupChildProcess() override {
        if (::prctl(PR_SET_PDEATHSIG,SIGKILL)<0 || ::getppid()!=owner) ::_exit(127);
    }
};
void exact(const QJsonObject &value, QStringList expected) {
    auto keys=value.keys();keys.sort();expected.sort();
    require(keys==expected,"Unexpected media parameters","media");
}
class Player : public QObject {
    QFile output;
    QSharedMemory memory;
    Decoder decoder;
    QByteArray input, pixels, progress;
    QJsonObject state;
    qint64 offset=0, position=0, duration=0, sequence=0;
    int held=-1, volume=60;
    bool muted=false, paused=false, retiring=false, hasAudio=false, visible=true;
    QString source() const { return QString("/proc/%1/fd/%2").arg(::getpid()).arg(held); }
    void send(QJsonObject value) {
        value["protocol"]=1;
        output.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');output.flush();
    }
    void status() {
        state["event"]="status";state["position_ms"]=double(position);state["duration_ms"]=double(duration);
        state["volume"]=volume;state["muted"]=muted;state["visible"]=visible;
        send(state);
    }
    void stopDecoder() {
        retiring=true;
        if (decoder.state()!=QProcess::NotRunning) {
            ::kill(pid_t(decoder.processId()),SIGCONT);
            decoder.terminate();
            if (!decoder.waitForFinished(300)) {decoder.kill();decoder.waitForFinished(1000);}
        }
        retiring=false;pixels.clear();progress.clear();paused=false;
    }
    void closeFile() {stopDecoder();if(held>=0)::close(held);held=-1;}
    void startDecoder() {
        stopDecoder();offset=position;
        QStringList args{"-nostdin","-hide_banner","-loglevel","error","-threads","1",
            "-protocol_whitelist","file,pipe","-format_whitelist","mov,matroska,webm,avi,mpeg,mpegts,mjpeg,wav,mp3,flac,ogg",
            "-re","-ss",QString::number(offset/1000.0,'f',3),"-i",source(),
            "-map","0:v:0","-an","-vf","fps=10,scale=512:288:force_original_aspect_ratio=decrease,pad=512:288:(ow-iw)/2:(oh-ih)/2",
            "-threads","1","-pix_fmt","rgb24","-c:v","rawvideo","-f","rawvideo","pipe:1"};
        if (hasAudio)
            args << "-map" << "0:a:0" << "-vn" << "-af" << QString("volume=%1").arg(muted?0:volume/100.0)
                 << "-ac" << "2" << "-ar" << "48000" << "-f" << "alsa" << "default";
        args << "-progress" << "pipe:2" << "-nostats";
        decoder.start("/usr/bin/ffmpeg",args);
        require(decoder.waitForStarted(2000),"Software decoder unavailable","media");
        state["state"]="starting";state["first_frame"]=false;status();
    }
    void frames() {
        if (retiring) {decoder.readAllStandardOutput();return;}
        pixels+=decoder.readAllStandardOutput();
        // Keep only the newest complete frame when rendering cannot keep up.
        if (pixels.size()>=FrameBytes) {
            const int complete=pixels.size()/FrameBytes;
            const auto frame=pixels.mid((complete-1)*FrameBytes,FrameBytes);
            pixels.remove(0,complete*FrameBytes);
            if (!memory.lock()) {closeFile();state["state"]="error";state["error"]="frame-lock-failed";status();return;}
            memcpy(memory.data(),frame.constData(),FrameBytes);memory.unlock();
            state["state"]=paused?"paused":"playing";state["first_frame"]=true;
            send({{"event","frame"},{"key",memory.key()},{"width",Width},{"height",Height},
                  {"stride",Width*3},{"bytes",FrameBytes},{"sequence",double(++sequence)}});
            status();
        }
    }
    void execute(const QString &name,const QJsonObject &p) {
        if(name=="rect") {
            exact(p,{"x","y","width","height"});
            for(const auto &key:p.keys()) require(positiveInteger(p[key],true)&&p[key].toDouble()<=8192,"Invalid rectangle","media");
            return;
        }
        if(name=="close") {exact(p,{});closeFile();position=duration=0;state["state"]="idle";state["first_frame"]=false;status();return;}
        if(name=="open") {
            exact(p,{"path","volume"});
            require(positiveInteger(p["volume"],true)&&p["volume"].toDouble()<=100,"Invalid volume","media");
            const auto path=p["path"].toString();
            require(path.startsWith('/')&&!path.contains(QChar(0)),"Local file required","media");
            closeFile();
            held=::open(QFile::encodeName(path).constData(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
            struct stat info{};
            require(held>=0&&::fstat(held,&info)==0&&S_ISREG(info.st_mode)&&info.st_size>0,"Regular media file required","media");
            Decoder probe;
            probe.start("/usr/bin/ffprobe",{"-v","error","-protocol_whitelist","file,pipe",
                "-format_whitelist","mov,matroska,webm,avi,mpeg,mpegts,mjpeg,wav,mp3,flac,ogg",
                "-show_entries","stream=codec_type,width,height:format=duration","-of","json",source()});
            if(!probe.waitForFinished(8000)) {probe.kill();probe.waitForFinished();throw Failure("probe-timeout","media","Media inspection timed out");}
            const auto bytes=probe.readAllStandardOutput();
            require(probe.exitCode()==0&&bytes.size()<=65536,"Cannot inspect media","media");
            const auto metadata=strictJson(bytes).toObject();
            bool video=false;hasAudio=false;
            for(const auto &entry:metadata["streams"].toArray()) {
                auto stream=entry.toObject();
                if(stream["codec_type"]=="audio")hasAudio=true;
                if(stream["codec_type"]=="video") {
                    require(stream["width"].toInt()>0&&stream["height"].toInt()>0&&stream["width"].toInt()<=1280&&stream["height"].toInt()<=720,
                            "Software player supports input up to 1280x720","media");video=true;
                }
            }
            require(video,"Video stream required","media");
            bool valid=false;const double seconds=metadata["format"].toObject()["duration"].toString().toDouble(&valid);
            require(valid&&seconds>0&&seconds<=86400,"Finite media duration required","media");
            duration=qint64(seconds*1000);position=0;volume=p["volume"].toInt();visible=true;
            state["audio_available"]=hasAudio;state["error"]=QJsonValue();startDecoder();return;
        }
        require(held>=0,"Open a media file first","media");
        if(name=="pause"||name=="suspend") {
            exact(p,{});stopDecoder();paused=true;state["state"]="paused";
            if(name=="suspend")visible=false;
        } else if(name=="play") {exact(p,{});if(position>=duration)position=0;visible=true;startDecoder();return;}
        else if(name=="seek") {
            exact(p,{"position_ms"});require(positiveInteger(p["position_ms"],true)&&p["position_ms"].toDouble()<duration,"Invalid seek","media");
            const bool resume=!paused;stopDecoder();position=qint64(p["position_ms"].toDouble());
            if(resume){startDecoder();return;}paused=true;
        } else if(name=="volume"||name=="mute") {
            exact(p,{"value"});
            if(name=="volume") {require(positiveInteger(p["value"],true)&&p["value"].toDouble()<=100,"Invalid volume","media");volume=p["value"].toInt();}
            else {require(p["value"].isBool(),"Invalid mute","media");muted=p["value"].toBool();}
            if(!paused){startDecoder();return;}
        } else if(name=="visible") {exact(p,{"value"});require(p["value"].isBool(),"Invalid visibility","media");visible=p["value"].toBool();}
        else throw Failure("unknown-command","media","Unknown media operation");
        status();
    }
  public:
    Player():memory("dshan-media-"+QUuid::createUuid().toString(QUuid::Id128)) {
        require(output.open(STDOUT_FILENO,QIODevice::WriteOnly),"Cannot open media output");
        require(memory.create(FrameBytes),"Cannot create frame memory","media");
        state={{"backend","ffmpeg-software"},{"available",true},{"state","idle"},
               {"capabilities",QJsonArray{"suspend-during-prepare","visibility-state","shared-rgb-frame-v1"}},
               {"first_frame",false},{"hardware_decode_confirmed",false},{"seekable",true},{"seeking",false},
               {"width",Width},{"height",Height},{"display_fps",10}};
        connect(&decoder,&QProcess::readyReadStandardOutput,this,[this](){frames();});
        connect(&decoder,&QProcess::readyReadStandardError,this,[this](){
            progress+=decoder.readAllStandardError();
            int end;
            while((end=progress.indexOf('\n'))>=0){const auto line=progress.left(end);progress.remove(0,end+1);
                if(line.startsWith("out_time_us=")){bool ok=false;auto value=line.mid(12).toLongLong(&ok);if(ok&&value>=0)position=qMin(duration,offset+value/1000);}}
            if(progress.size()>8192)progress.clear();
        });
        connect(&decoder,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus how){
            if(retiring)return;
            state["state"]=code==0&&how==QProcess::NormalExit?"completed":"error";
            if(state["state"]=="error")state["error"]="decode-or-audio-failed";
            status();
        });
        ::fcntl(STDIN_FILENO,F_SETFL,::fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
        auto notifier=new QSocketNotifier(STDIN_FILENO,QSocketNotifier::Read,this);
        connect(notifier,&QSocketNotifier::activated,this,[this](int){
            char buffer[4096];auto count=::read(STDIN_FILENO,buffer,sizeof(buffer));
            if(count==0){closeFile();QCoreApplication::quit();return;}if(count<0)return;
            input.append(buffer,int(count));if(input.size()>65536){closeFile();QCoreApplication::exit(1);return;}
            int end;
            while((end=input.indexOf('\n'))>=0){const auto line=input.left(end);input.remove(0,end+1);QString id;
                try {auto request=strictJson(line).toObject();exact(request,{"protocol","request_id","command","params"});
                    require(request["protocol"]==1&&request["params"].isObject()&&request["request_id"].isString(),"Invalid media request","media");
                    id=request["request_id"].toString();execute(request["command"].toString(),request["params"].toObject());
                    send({{"event","reply"},{"request_id",id},{"accepted",true}});
                } catch(const Failure &error){closeFile();state["state"]="error";state["error"]=error.code;status();
                    send({{"event","reply"},{"request_id",id},{"accepted",false},{"error",QJsonObject{{"code",error.code}}}});}
            }
        });status();
    }
    ~Player() override {closeFile();}
};
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {Player player;return app.exec();}catch(const Failure &){return 1;}
}
