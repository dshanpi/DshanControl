#include "applicationhealth.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <fcntl.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <memory>

namespace {
void healthRequire(bool ok, const QString &code, const QString &detail) {
  require(ok, detail, "health", code);
}
struct Scope {
  QJsonObject contract, installed, policy;
  QString root, current, data;
  bool development;
  explicit Scope(const QJsonObject &config) {
    healthRequire(!config.contains("development_mode") || config["development_mode"].isBool(),
                  "health-contract", "Invalid development mode");
    development=config["development_mode"].toBool(false);
    contract=readJson(config["system_contract"].toString());
    root=config["application_root"].toString();
    healthRequire(QDir::isAbsolutePath(root) && root!="/" && QDir::cleanPath(root)==root &&
                  root==contract["application_root"].toString(), "health-contract", "Application root mismatch");
    if (!development) verifyPersistentStorage(root,contract);
    auto releases=QFileInfo(root+"/releases").canonicalFilePath();
    current=QFileInfo(root+"/current").canonicalFilePath();
    healthRequire(!QFileInfo(root).isSymLink() && !QFileInfo(root+"/releases").isSymLink() &&
                  !releases.isEmpty() && QFileInfo(root+"/current").isSymLink() &&
                  !current.isEmpty() && QFileInfo(current).absolutePath()==releases,
                  "health-identity", "Current application outside managed releases");
    healthRequire(!QFileInfo(current+"/installed.json").isSymLink(),"health-identity", "Application identity must not be linked");
    installed=readJson(current+"/installed.json");
    healthRequire(QRegularExpression("^[a-f0-9]{40}$").match(installed["commit"].toString()).hasMatch() &&
                  installed["data_schema"]==1 && contract["data_schema"]==1,
                  "health-identity", "Application identity/data schema missing");
    policy=config["application_health"].toObject();
    healthRequire(!config.contains("application_health") || config["application_health"].isObject(),
                  "health-contract", "Invalid health policy");
    data=contract["data_root"].toString();
  }
};
qint64 privatePid(const QString &path) {
  int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
  struct stat st;
  if (fd<0) throw Failure("health-process", "health", "Required process PID unavailable");
  bool safe=::fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==::geteuid() && !(st.st_mode&0022);
  char raw[65];ssize_t count=::read(fd,raw,sizeof(raw));::close(fd);
  healthRequire(safe && count>0 && count<ssize_t(sizeof(raw)),"health-process", "Untrusted process PID file");
  auto text=QString::fromLatin1(raw,int(count)).trimmed();
  bool ok=false;auto pid=text.toLongLong(&ok);
  healthRequire(ok && QRegularExpression("^[0-9]+$").match(text).hasMatch() && pid>1 && pid<=2147483647,
                "health-process", "Invalid process PID");
  return pid;
}
qint64 owned(const QString &name,const QString &pidRoot,const QStringList &expected) {
  const auto pid=privatePid(pidRoot+'/'+name+".pid");
  auto base="/proc/"+QString::number(pid);
  auto stat=readBytes(base+"/stat",4096);auto split=stat.lastIndexOf(") ");
  healthRequire(split>=0 && stat.size()>split+2 && stat[split+2]!='Z' && stat[split+2]!='X',
                "health-process", "Application process is not alive");
  const auto exe=QFileInfo(base+"/exe").canonicalFilePath();
  const auto wanted=QFileInfo(expected.first()).canonicalFilePath();
  auto args=readBytes(base+"/cmdline",65536).split('\0');
  if (!args.isEmpty() && args.last().isEmpty()) args.removeLast();
  QList<QByteArray> argv;for (const auto &a:expected) argv.append(a.toUtf8());
  healthRequire(!exe.isEmpty() && !wanted.isEmpty() && exe==wanted && args==argv,
                "health-process", "Application PID belongs to a different executable or invocation");
  return pid;
}
QJsonObject webHealth(const QUrl &url) {
  healthRequire(url.isValid() && url.scheme()=="http" && url.host()=="127.0.0.1" &&
                url.userInfo().isEmpty() && url.path()=="/api/health" && !url.hasQuery() && !url.hasFragment(),
                "health-contract", "Health endpoint must be fixed loopback API");
  QNetworkAccessManager manager;manager.setProxy(QNetworkProxy::NoProxy);QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
  QNetworkReply *reply=manager.get(request);reply->setReadBufferSize(65537);
  QEventLoop loop;QTimer timer;timer.setSingleShot(true);bool timeout=false,large=false;QByteArray bytes;
  auto read=[&] {
    if(large || !reply->isOpen())return;
    if(reply->bytesAvailable()>0)bytes+=reply->read(65537-bytes.size());
    if(bytes.size()>65536){large=true;reply->abort();}
  };
  QObject::connect(reply,&QNetworkReply::readyRead,&loop,read);
  QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
  QObject::connect(&timer,&QTimer::timeout,&loop,[&]{timeout=true;reply->abort();});
  timer.start(3000);loop.exec();timer.stop();read();
  auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  auto error=reply->error();
  healthRequire(!timeout,"health-web-timeout", "Application health response timed out");
  healthRequire(!large,"health-web-size", "Application health response exceeds limit");
  healthRequire(code==200 && error==QNetworkReply::NoError,"health-web-http", "Application health HTTP failed");
  try {
    auto result=strictJson(bytes);
    healthRequire(result.isObject(),"health-web-identity", "Invalid health response");
    return result.toObject();
  } catch(const Failure &) {
    throw Failure("health-web-identity","health","Invalid health response");
  }
}
void database(const QString &path,const QStringList &tables,bool core) {
  healthRequire(QFileInfo(path).isFile() && !QFileInfo(path).isSymLink(),"health-database", "Database is missing or linked");
  sqlite3 *raw=nullptr;auto encoded=QFile::encodeName(path);
  const int rc=sqlite3_open_v2(encoded.constData(),&raw,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,nullptr);
  std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,sqlite3_close);
  healthRequire(rc==SQLITE_OK,"health-database", "Database cannot be opened read-only");
  sqlite3_busy_timeout(raw,2000);QElapsedTimer elapsed;elapsed.start();
  sqlite3_progress_handler(raw,1000,[](void *p)->int{return static_cast<QElapsedTimer*>(p)->elapsed()>2000 ? 1 : 0;},&elapsed);
  sqlite3_stmt *stmt=nullptr;
  auto query=[&](const QByteArray &sql){
    if(stmt){sqlite3_finalize(stmt);stmt=nullptr;}
    healthRequire(sqlite3_prepare_v2(raw,sql.constData(),-1,&stmt,nullptr)==SQLITE_OK,
                  "health-database", "Database schema/query rejected");
  };
  try {
    query("PRAGMA quick_check");
    healthRequire(sqlite3_step(stmt)==SQLITE_ROW && sqlite3_column_text(stmt,0) &&
                  QByteArray(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)))=="ok" &&
                  sqlite3_step(stmt)==SQLITE_DONE,"health-database", "Database consistency check failed");
    for(const auto &table:tables){
      query("SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?");
      auto name=table.toUtf8();sqlite3_bind_text(stmt,1,name.constData(),name.size(),SQLITE_TRANSIENT);
      healthRequire(sqlite3_step(stmt)==SQLITE_ROW && sqlite3_column_int(stmt,0)==1,
                    "health-database-schema", "Expected application table missing");
    }
    if(core){
      query("SELECT value FROM metadata WHERE key='schema_version'");
      healthRequire(sqlite3_step(stmt)==SQLITE_ROW && sqlite3_column_text(stmt,0) &&
                    QByteArray(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)))=="1",
                    "health-database-schema", "Unsupported application database schema");
    }
    sqlite3_finalize(stmt);stmt=nullptr;
    sqlite3_progress_handler(raw,0,nullptr,nullptr);
  } catch(...){if(stmt)sqlite3_finalize(stmt);sqlite3_progress_handler(raw,0,nullptr,nullptr);throw;}
}
double uptime() { struct timespec now;healthRequire(::clock_gettime(CLOCK_BOOTTIME,&now)==0,
      "health-clock", "Monotonic boot clock unavailable");return double(now.tv_sec)+now.tv_nsec/1e9; }
void heartbeat(const QString &path,qint64 pid,const QString &commit) {
  struct stat st,process;
  healthRequire(::stat(QFile::encodeName("/proc/"+QString::number(pid)).constData(),&process)==0 &&
                ::lstat(QFile::encodeName(path).constData(),&st)==0 && S_ISREG(st.st_mode) &&
                st.st_uid==process.st_uid && !(st.st_mode&0022),
                "health-hmi-heartbeat", "HMI heartbeat missing or unsafe");
  auto value=readJson(path);auto now=uptime();auto stamp=value["monotonic_seconds"].toDouble(-1);
  healthRequire(value["schema_version"]==1 && value["pid"].toDouble()==pid && value["commit"]==commit &&
                value["boot_id"]==QString::fromUtf8(readBytes("/proc/sys/kernel/random/boot_id",128)).trimmed() &&
                value["gui_event_loop"].isBool() && value["gui_event_loop"].toBool() &&
                value["backend_identity_verified"].isBool() && value["backend_identity_verified"].toBool() &&
                stamp>=0 && stamp<=now+0.05 && now-stamp<6,
                "health-hmi-heartbeat", "HMI event-loop/backend heartbeat is stale or mismatched");
}
} // namespace
QJsonObject applicationIdentity(const QJsonObject &config) {return Scope(config).installed;}
QJsonObject applicationHealth(const QJsonObject &config) {
  Scope scope(config);auto pidRoot=scope.policy["pid_root"].toString("/var/run/dshancontrol");
  healthRequire(QDir::isAbsolutePath(pidRoot) && !QFileInfo(pidRoot).isSymLink(),"health-contract", "Invalid process PID root");
  QJsonObject fixture=scope.policy.value("fixture_processes").toObject();
  healthRequire(!scope.policy.contains("fixture_processes") || scope.development,
                "health-contract", "Fixture process overrides require development mode");
  QMap<QString,QStringList> processes{{"web",{"/usr/bin/python3",scope.root+"/current/lib/web/app.py"}},
      {"hmi",{scope.root+"/current/bin/omnigate-hmi"}},
      {"supervisor",{"/usr/bin/python3",scope.root+"/current/lib/supervisor/supervisor.py"}}};
  if(scope.policy.contains("fixture_processes")) {
    healthRequire(scope.policy["fixture_processes"].isObject() && fixture.size()==3,
                  "health-contract", "Invalid fixture process list");
    for(auto it=processes.begin();it!=processes.end();++it){
      auto array=fixture[it.key()].toArray();healthRequire(!array.isEmpty(),"health-contract", "Missing fixture process");
      QStringList args;for(const auto &arg:array){healthRequire(arg.isString(),"health-contract", "Invalid fixture argv");args<<arg.toString();}it.value()=args;
    }
  }
  QJsonObject pids;for(auto it=processes.cbegin();it!=processes.cend();++it)pids[it.key()]=double(owned(it.key(),pidRoot,it.value()));
  auto web=webHealth(QUrl(scope.policy["http_url"].toString("http://127.0.0.1/api/health")));
  auto commit=scope.installed["commit"].toString();
  healthRequire(web["ok"].isBool() && web["ok"].toBool() && web["service"]=="omnigate-web" &&
                web["application_commit"]==commit,"health-web-identity", "Backend/current application identity mismatch");
  healthRequire(QDir::isAbsolutePath(scope.data) && scope.data!="/" && QDir::cleanPath(scope.data)==scope.data &&
                !QFileInfo(scope.data).isSymLink(),"health-contract", "Invalid managed data root");
  if(!scope.development)verifyPersistentStorage(scope.data,scope.contract);
  database(scope.data+"/edge.db",{"metadata","devices","points","audit"},true);
  database(scope.data+"/hmi.db",{"alarms","audit","telemetry"},false);
  auto required=scope.policy.value("require_hmi_heartbeat");
  healthRequire(required.isUndefined() || required.isBool(),"health-contract", "Heartbeat policy must be boolean");
  healthRequire(scope.contract["contract_id"]!="dshancontrol-platform-2" || required.toBool(false),
                "health-contract", "Platform 2 requires a functional HMI heartbeat");
  if(required.toBool(false))heartbeat(scope.policy["hmi_heartbeat"].toString("/var/run/dshancontrol/hmi-heartbeat.json"),
      qint64(pids["hmi"].toDouble()),commit);
  return {{"ok",true},{"application_commit",commit},{"processes",pids},{"databases",2},
          {"hmi_functional_heartbeat",required.toBool(false)}};
}
