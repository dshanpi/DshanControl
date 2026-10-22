#pragma once
// Optional software backend. The existing Cedar and unavailable builds are unchanged.
#include "../../runtime/src/storage.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QSize>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QSocketNotifier>
#include <QTimer>
#include <QUrl>
#include <QtEndian>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

class GstMediaWorker : public QObject
{
    QFile output;
    QLocalSocket frames;
    QTimer timer;
    QElapsedTimer clock, preparing, fpsClock;
    QByteArray input;
    GstElement *pipeline = nullptr, *sink = nullptr, *outputFilter = nullptr;
    QSize sourceSize, outputSize, viewport=QSize(640,480);
    GstBus *bus = nullptr;
    int heldFile = -1, sentFrames = 0, droppedFrames = 0, fpsFrames = 0;
    quint32 frameGeneration = 0;
    bool desiredPlaying = false;
    QJsonObject status;
    qint64 lastStatus = 0;
    static void exact(const QJsonObject &v, QStringList keys)
    {
        auto actual=v.keys(); actual.sort(); keys.sort();
        require(actual==keys,"Invalid media request fields","request");
    }
    void send(QJsonObject value)
    {
        value["protocol"]=1;
        output.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n'); output.flush();
    }
    void emitStatus()
    {
        auto value=status; value["event"]="status"; send(value); lastStatus=clock.elapsed();
    }
    void release()
    {
        desiredPlaying=false; ++frameGeneration; status["frame_generation"]=double(frameGeneration);
        if(pipeline) gst_element_set_state(pipeline,GST_STATE_NULL);
        if(bus) gst_object_unref(bus);
        if(sink) gst_object_unref(sink);
        if(outputFilter) gst_object_unref(outputFilter);
        if(pipeline) gst_object_unref(pipeline);
        pipeline=nullptr; sink=nullptr; bus=nullptr; outputFilter=nullptr; sourceSize=QSize();outputSize=QSize();
        if(heldFile>=0) ::close(heldFile);
        heldFile=-1;
    }
    void fail(const QString &code)
    {
        release(); status["state"]="error"; status["error"]=code;
        status["first_frame"]=false; status["visible"]=false; emitStatus();
    }
    void setState(GstState value)
    {
        require(pipeline && gst_element_set_state(pipeline,value)!=GST_STATE_CHANGE_FAILURE,
                "Cannot change playback state","playback-state-failed");
    }
    void query()
    {
        if(!pipeline)return;
        gint64 position=0,duration=0;
        if(gst_element_query_position(pipeline,GST_FORMAT_TIME,&position) && position>=0)
            status["position_ms"]=double(position/GST_MSECOND);
        if(gst_element_query_duration(pipeline,GST_FORMAT_TIME,&duration) && duration>=0)
            status["duration_ms"]=double(duration/GST_MSECOND);
        GstQuery *q=gst_query_new_seeking(GST_FORMAT_TIME);
        if(gst_element_query(pipeline,q)) {
            gboolean seekable=false; gst_query_parse_seeking(q,nullptr,&seekable,nullptr,nullptr);
            status["seekable"]=bool(seekable);
        }
        gst_query_unref(q);
    }
    // Bound RGB transport and raster work on the single-core 6ULL, including fullscreen.
    void resizeOutput()
    {
        if(!outputFilter || !sourceSize.isValid())return;
        QSize size=sourceSize;
        size.scale(QSize(qMin(320,viewport.width()),qMin(240,viewport.height())),Qt::KeepAspectRatio);
        size=QSize(qMax(8,size.width()&~1),qMax(8,size.height()&~1));
        if(size==outputSize)return;
        GstCaps *caps=gst_caps_new_simple("video/x-raw","format",G_TYPE_STRING,"RGB16",
            "width",G_TYPE_INT,size.width(),"height",G_TYPE_INT,size.height(),
            "pixel-aspect-ratio",GST_TYPE_FRACTION,1,1,nullptr);
        g_object_set(outputFilter,"caps",caps,nullptr);gst_caps_unref(caps);outputSize=size;
    }
    void sample(GstSample *sample)
    {
        GstVideoInfo info;
        if(!gst_video_info_from_caps(&info,gst_sample_get_caps(sample)) ||
           GST_VIDEO_INFO_FORMAT(&info)!=GST_VIDEO_FORMAT_RGB16 ||
           info.width<1 || info.width>640 || info.height<1 || info.height>480) {
            fail("unsupported-frame-size");return;
        }
        if(!sourceSize.isValid()) {
            GstPad *pad=nullptr;g_signal_emit_by_name(pipeline,"get-video-pad",0,&pad);
            GstCaps *caps=pad?gst_pad_get_current_caps(pad):nullptr;GstVideoInfo original;
            if(caps && gst_video_info_from_caps(&original,caps))sourceSize=QSize(original.width,original.height);
            else sourceSize=QSize(info.width,info.height);
            if(caps)gst_caps_unref(caps);if(pad)gst_object_unref(pad);
            if(sourceSize.width()>1280 || sourceSize.height()>720){fail("unsupported-source-size");return;}
            resizeOutput();
        }
        status["width"]=sourceSize.width(); status["height"]=sourceSize.height();
        if(QSize(info.width,info.height)!=outputSize)return; // Discard in-flight buffers during renegotiation.
        status["output_width"]=info.width;status["output_height"]=info.height;
        status["source_fps"]=info.fps_d>0?double(info.fps_n)/info.fps_d:0;
        if(frames.state()!=QLocalSocket::ConnectedState) {fail("frame-output-unavailable");return;}
        if(frames.bytesToWrite()>0) {++droppedFrames;return;}
        GstVideoFrame mapped;
        if(!gst_video_frame_map(&mapped,&info,gst_sample_get_buffer(sample),GST_MAP_READ)) {
            fail("frame-map-failed");return;
        }
        const int stride=GST_VIDEO_FRAME_PLANE_STRIDE(&mapped,0);
        if(stride<info.width*2) {gst_video_frame_unmap(&mapped);fail("invalid-frame-stride");return;}
        QByteArray packet(20+info.width*info.height*2,Qt::Uninitialized);
        auto *header=reinterpret_cast<uchar *>(packet.data());
        qToLittleEndian<quint32>(0x31465244,header);
        qToLittleEndian<quint32>(info.width,header+4);
        qToLittleEndian<quint32>(info.height,header+8);
        qToLittleEndian<quint32>(info.width*info.height*2,header+12);
        qToLittleEndian<quint32>(frameGeneration,header+16);
        const auto *pixels=static_cast<const char *>(GST_VIDEO_FRAME_PLANE_DATA(&mapped,0));
        for(int y=0;y<info.height;++y)
            memcpy(packet.data()+20+y*info.width*2,pixels+y*stride,info.width*2);
        gst_video_frame_unmap(&mapped);
        frames.write(packet); ++sentFrames;
        status["frames_delivered"]=sentFrames; status["frames_dropped"]=droppedFrames;
        if(!status["first_frame"].toBool()) {
            status["first_frame"]=true;
            if(desiredPlaying)status["state"]="playing";
            emitStatus();
        }
        ++fpsFrames;
        if(fpsClock.elapsed()>=1000) {
            status["display_fps"]=double(fpsFrames)*1000/fpsClock.elapsed();
            fpsFrames=0;fpsClock.restart();
        }
    }
    void tick()
    {
        if(!pipeline)return;
        while(auto *message=gst_bus_pop(bus)) {
            const auto type=GST_MESSAGE_TYPE(message);
            if(type==GST_MESSAGE_ERROR) {
                GError *error=nullptr; gchar *debug=nullptr;
                gst_message_parse_error(message,&error,&debug);
                if(error) {qWarning("GStreamer: %s",error->message);g_error_free(error);}
                g_free(debug);gst_message_unref(message);fail("decode-failed");return;
            }
            if(type==GST_MESSAGE_TAG) {
                GstTagList *tags=nullptr;gst_message_parse_tag(message,&tags);gchar *codec=nullptr;
                if(gst_tag_list_get_string(tags,GST_TAG_VIDEO_CODEC,&codec)) {
                    status["codec_name"]=QString::fromUtf8(codec);g_free(codec);
                }
                gst_tag_list_unref(tags);
            }
            if(type==GST_MESSAGE_ASYNC_DONE) {
                status["seeking"]=false;query();
                if(status["state"]=="preparing" && !desiredPlaying)status["state"]="prepared";
            }
            if(type==GST_MESSAGE_EOS) {
                query();
                if(status["loop"].toBool()) {
                    if(!gst_element_seek_simple(pipeline,GST_FORMAT_TIME,
                           GstSeekFlags(GST_SEEK_FLAG_FLUSH|GST_SEEK_FLAG_ACCURATE),0)) {
                        gst_message_unref(message);fail("loop-seek-failed");return;
                    }
                    desiredPlaying=true;setState(GST_STATE_PLAYING);
                } else {
                    desiredPlaying=false;setState(GST_STATE_PAUSED);status["state"]="completed";
                    status["position_ms"]=status["duration_ms"];
                }
                emitStatus();
            }
            gst_message_unref(message);
        }
        if(!pipeline)return;
        GstSample *value=desiredPlaying?gst_app_sink_try_pull_sample(GST_APP_SINK(sink),0)
                                     :gst_app_sink_try_pull_preroll(GST_APP_SINK(sink),0);
        if(value) {sample(value);gst_sample_unref(value);}
        if(!pipeline)return;
        if(!status["first_frame"].toBool() && preparing.elapsed()>12000) {fail("first-frame-timeout");return;}
        if(clock.elapsed()-lastStatus>=500) {query();emitStatus();}
    }
    void command(const QString &name,const QJsonObject &params)
    {
        if(name=="snapshot") {exact(params,{});emitStatus();return;}
        if(name=="rect") {
            exact(params,{"x","y","width","height"});
            for(const auto &key:{"x","y","width","height"})
                require(positiveInteger(params[key],true) && params[key].toDouble()<=8192 && params[key].toInt()%2==0,
                        "Invalid viewport","media");
            require(params["width"].toInt()>=8 && params["height"].toInt()>=8,"Invalid viewport","media");
            viewport=QSize(params["width"].toInt(),params["height"].toInt());resizeOutput();
            return; // Qt owns composition; the worker never writes the framebuffer.
        }
        if(name=="close") {
            exact(params,{});release();status["state"]="idle";status["first_frame"]=false;
            status["visible"]=false;status["position_ms"]=0;status["duration_ms"]=0;
            emitStatus();return;
        }
        if(name=="open") {
            exact(params,{"path","volume"});
            require(positiveInteger(params["volume"],true) && params["volume"].toDouble()<=100,"Invalid volume","media");
            require(frames.state()==QLocalSocket::ConnectedState,"Qt video output required","frame-output-unavailable");
            const QString path=params["path"].toString();
            require(path.startsWith('/') && !path.contains(QChar(0)),"Local file required","media");
            release();heldFile=::open(QFile::encodeName(path).constData(),O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
            struct stat st{};
            require(heldFile>=0 && ::fstat(heldFile,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0,
                    "Readable regular media file required","file-unavailable");
            pipeline=gst_element_factory_make("playbin",nullptr);
            GError *error=nullptr;
            GstElement *video=gst_parse_bin_from_description(
                "videoscale ! videoconvert ! capsfilter name=qtrgb caps=\"video/x-raw,format=RGB16,width=[1,640],height=[1,480],pixel-aspect-ratio=1/1\" ! appsink name=qtframes max-buffers=1 drop=true sync=true",true,&error);
            if(error)g_error_free(error);
            require(pipeline && video,"GStreamer video plugins missing","decoder-unavailable");
            sink=gst_bin_get_by_name(GST_BIN(video),"qtframes");
            outputFilter=gst_bin_get_by_name(GST_BIN(video),"qtrgb");
            const auto uri=QUrl::fromLocalFile(QString("/proc/self/fd/%1").arg(heldFile)).toEncoded();
            // Video-only qualification: disable audio and subtitles, no ALSA probing or fake audio capability.
            gst_object_ref_sink(video);
            g_object_set(pipeline,"uri",uri.constData(),"video-sink",video,"flags",guint(1),nullptr);
            gst_object_unref(video);
            bus=gst_element_get_bus(pipeline);
            desiredPlaying=true;sentFrames=droppedFrames=fpsFrames=0;fpsClock.restart();preparing.restart();
            status["state"]="preparing";status["first_frame"]=false;status["visible"]=true;
            status["seeking"]=false;status["error"]=QJsonValue();status["position_ms"]=0;status["duration_ms"]=0;
            status["width"]=0;status["height"]=0;status["loop"]=false;status["volume"]=params["volume"];
            status["frames_delivered"]=0;status["frames_dropped"]=0;status["codec_name"]=QJsonValue();
            setState(GST_STATE_PLAYING);emitStatus();return;
        }
        if(name=="suspend") {
            exact(params,{});desiredPlaying=false;
            if(pipeline) {setState(GST_STATE_PAUSED);status["state"]=status["first_frame"]==true?"paused":"prepared";}
            status["visible"]=false;emitStatus();return;
        }
        if(name=="visible") {
            exact(params,{"value"});require(params["value"].isBool(),"Invalid visibility","media");
            if(!params["value"].toBool() && pipeline) {desiredPlaying=false;setState(GST_STATE_PAUSED);status["state"]="paused";}
            status["visible"]=params["value"];emitStatus();return;
        }
        require(pipeline,"No prepared video","media");
        if(name=="pause" || name=="play") {
            exact(params,{});desiredPlaying=name=="play";fpsFrames=0;fpsClock.restart();setState(desiredPlaying?GST_STATE_PLAYING:GST_STATE_PAUSED);
            status["state"]=desiredPlaying?(status["first_frame"]==true?"playing":"starting"):"paused";
        } else if(name=="seek") {
            exact(params,{"position_ms"});
            require(status["seekable"]==true && positiveInteger(params["position_ms"],true) &&
                    params["position_ms"].toDouble()<=status["duration_ms"].toDouble() && params["position_ms"].toDouble()<=2147483647,"Invalid seek","media");
            require(gst_element_seek_simple(pipeline,GST_FORMAT_TIME,GstSeekFlags(GST_SEEK_FLAG_FLUSH|GST_SEEK_FLAG_ACCURATE),
                        gint64(params["position_ms"].toDouble()*GST_MSECOND)),"Seek failed","seek-failed");
            status["seeking"]=true;
        } else if(name=="loop") {
            exact(params,{"value"});require(params["value"].isBool(),"Invalid loop","media");status["loop"]=params["value"];
        } else if(name=="volume" || name=="mute") {
            throw Failure("audio-not-enabled","media","This board profile is video-only");
        } else throw Failure("unknown-command","media","Unknown media command");
        query();emitStatus();
    }
    void read()
    {
        char bytes[8192];const ssize_t n=::read(STDIN_FILENO,bytes,sizeof(bytes));
        if(n==0){release();QCoreApplication::quit();return;} if(n<0)return;
        input.append(bytes,int(n));
        if(input.size()>65536){release();QCoreApplication::exit(2);return;}
        int end;
        while((end=input.indexOf('\n'))>=0) {
            const auto line=input.left(end);input.remove(0,end+1);QString id,name;
            try {
                const auto doc=strictJson(line);require(doc.isObject(),"Object required");const auto r=doc.toObject();
                exact(r,{"protocol","request_id","command","params"});
                require(r["protocol"]==1 && r["request_id"].isString() && r["request_id"].toString().size()<=64 &&
                        r["command"].isString() && r["params"].isObject(),"Invalid request");
                id=r["request_id"].toString();name=r["command"].toString();command(name,r["params"].toObject());
                send({{"event","reply"},{"request_id",id},{"accepted",true}});
            } catch(const Failure &e) {
                if(name=="open")fail(e.code);
                send({{"event","reply"},{"request_id",id},{"accepted",false},{"error",QJsonObject{{"code",e.code}}}});
            }
        }
    }
public:
    explicit GstMediaWorker(int fd)
    {
        require(output.open(fd,QIODevice::WriteOnly,QFileDevice::AutoCloseHandle),"Cannot open output");
        gst_init(nullptr,nullptr);
        status={{"backend","gstreamer-software"},{"available",true},{"state","idle"},
                {"capabilities",QJsonArray{"suspend-during-prepare","visibility-state","qt-rgb565-frames-v1"}},
                {"first_frame",false},{"hardware_decode_confirmed",false},{"audio_available",false},
                {"video_output","qt-rgb565"},{"position_ms",0},{"duration_ms",0},{"seekable",false},{"seeking",false}};
        frames.connectToServer(qEnvironmentVariable("DSHANCONTROL_MEDIA_FRAME_SOCKET"));
        if (!frames.waitForConnected(1000)) { status["available"]=false; status["state"]="unavailable"; status["error"]="frame-output-unavailable"; }
        int flags=::fcntl(STDIN_FILENO,F_GETFL);require(flags>=0 && ::fcntl(STDIN_FILENO,F_SETFL,flags|O_NONBLOCK)==0,"Input setup failed");
        auto *notifier=new QSocketNotifier(STDIN_FILENO,QSocketNotifier::Read,this);
        connect(notifier,&QSocketNotifier::activated,this,[this](int){read();});
        connect(&timer,&QTimer::timeout,this,[this](){try{tick();}catch(const Failure &e){fail(e.code);}});
        clock.start();timer.start(15);emitStatus();
    }
    ~GstMediaWorker() override {release();}
};
