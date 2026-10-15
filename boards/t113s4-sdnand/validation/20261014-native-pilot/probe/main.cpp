#include <QCoreApplication>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <linux/fb.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <cstdio>
#include <stdexcept>
#include <cstring>
static void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static void event(int fd, int type, int code, int value) {
  input_event e{}; gettimeofday(&e.time, nullptr); e.type=type; e.code=code; e.value=value;
  check(write(fd, &e, sizeof(e))==sizeof(e), "event write failed");
}
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 try {
  QFile compatible("/proc/device-tree/compatible");
  check(compatible.open(QIODevice::ReadOnly)&&compatible.readAll().contains("allwinner,t113_s4"),"fixed S4 required");
  auto args=app.arguments(); check(args.size()>=2,"operation required");
  if (args[1]=="capture") {
   check(args.size()==2,"capture takes no output path");
   int fd=open("/dev/fb0",O_RDONLY|O_CLOEXEC); check(fd>=0,"fixed fb unavailable");
   fb_var_screeninfo v{}; fb_fix_screeninfo f{};
   check(ioctl(fd,FBIOGET_VSCREENINFO,&v)==0&&ioctl(fd,FBIOGET_FSCREENINFO,&f)==0,"fb geometry unavailable");
   check(v.xres==1024&&v.yres==600&&v.bits_per_pixel==32&&f.line_length>=4096&&f.line_length<=8192&&v.xoffset==0&&(v.yoffset+v.yres)*f.line_length<=f.smem_len,"unexpected framebuffer geometry");
   check(v.red.length==8&&v.green.length==8&&v.blue.length==8,"unexpected framebuffer channels");
   QImage image(v.xres,v.yres,QImage::Format_RGB32); QByteArray line(f.line_length,0);
   for(unsigned y=0;y<v.yres;++y){
    check(pread(fd,line.data(),line.size(),(y+v.yoffset)*f.line_length)==line.size(),"fb read failed");
    for(unsigned x=0;x<v.xres;++x){ unsigned p; memcpy(&p,line.constData()+(x+v.xoffset)*4,4);
     image.setPixel(x,y,qRgb((p>>v.red.offset)&255,(p>>v.green.offset)&255,(p>>v.blue.offset)&255));
    }
   }
   close(fd); QFile output("/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/frame.ppm"); check(output.open(QIODevice::WriteOnly),"capture output unavailable"); check(output.write("P6\n1024 600\n255\n")==16,"header write failed");
   for(int y=0;y<600;++y) { QByteArray row(1024*3,0); for(int x=0;x<1024;++x) { auto p=image.pixel(x,y); row[x*3]=qRed(p); row[x*3+1]=qGreen(p); row[x*3+2]=qBlue(p); } check(output.write(row)==row.size(),"capture write failed"); } check(output.flush(),"capture flush failed");
   puts("FIXED_FRAMEBUFFER_CAPTURED 1024x600");
  } else if(args[1]=="tap") {
   check(args.size()==4,"tap requires coordinates"); bool a,b; int x=args[2].toInt(&a),y=args[3].toInt(&b);
   check(a&&b&&x>=0&&x<1024&&y>=0&&y<600,"coordinates out of bounds");
   int fd=open("/dev/input/event4",O_WRONLY|O_CLOEXEC); check(fd>=0,"fixed input unavailable");
   char name[128]{}; check(ioctl(fd,EVIOCGNAME(sizeof(name)),name)>=0&&QByteArray(name)=="gt9xxnew_ts","fixed Goodix required");
   input_absinfo ax{},ay{}; unsigned long caps[(ABS_MAX+8*sizeof(unsigned long))/(8*sizeof(unsigned long))]{};
   check(ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(caps)),caps)>=0,"input capabilities unavailable");
   auto supports=[&](int code) { return (caps[code/(8*sizeof(unsigned long))]>>(code%(8*sizeof(unsigned long))))&1UL; };
   check(supports(ABS_MT_POSITION_X)&&supports(ABS_MT_POSITION_Y)&&supports(ABS_MT_TRACKING_ID),"MT coordinates unavailable");
   check(ioctl(fd,EVIOCGABS(ABS_MT_POSITION_X),&ax)==0&&ioctl(fd,EVIOCGABS(ABS_MT_POSITION_Y),&ay)==0,"MT ranges unavailable");
   bool typeB=supports(ABS_MT_SLOT);
   if(typeB) event(fd,EV_ABS,ABS_MT_SLOT,0);
   event(fd,EV_KEY,BTN_TOUCH,1); event(fd,EV_ABS,ABS_MT_TRACKING_ID,0);
   if(supports(ABS_MT_TOUCH_MAJOR)) event(fd,EV_ABS,ABS_MT_TOUCH_MAJOR,1);
   if(supports(ABS_MT_WIDTH_MAJOR)) event(fd,EV_ABS,ABS_MT_WIDTH_MAJOR,1);
   event(fd,EV_ABS,ABS_MT_POSITION_X,ax.minimum+x*(ax.maximum-ax.minimum)/1023);
   event(fd,EV_ABS,ABS_MT_POSITION_Y,ay.minimum+y*(ay.maximum-ay.minimum)/599);
   if(!typeB) event(fd,EV_SYN,SYN_MT_REPORT,0);
   event(fd,EV_SYN,SYN_REPORT,0); usleep(150000);
   if(typeB) event(fd,EV_ABS,ABS_MT_TRACKING_ID,-1);
   event(fd,EV_KEY,BTN_TOUCH,0);
   event(fd,EV_SYN,SYN_REPORT,0); close(fd);
   puts("SYNTHETIC_GOODIX_TAP_SENT (not physical touch evidence)");
  } else check(false,"operation denied");
  return 0;
 } catch(const std::exception &e) { fprintf(stderr,"probe: %s\n",e.what()); return 1; }
}
