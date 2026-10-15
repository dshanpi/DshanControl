// Test adapter for pure boot-map/mountinfo parsers. Never installed on a board image.
#include "../../components/runtime/src/systemstorage.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
int main(int argc, char **argv) {
  QCoreApplication app(argc,argv);
  try {
    require(argc==2,"Fixture input file required");
    const auto input=readJson(QString::fromLocal8Bit(argv[1]));
    QJsonObject output;
    if(input["operation"]=="sysfs-attribute")
      output={{"value",QString::fromUtf8(readSysfsAttribute(input["path"].toString(),
          input["limit"].toInt()))}};
    else if(input["operation"]=="boot-map")
      output={{"device",bootPersistentDevice(input["cmdline"].toString().toUtf8())}};
    else if(input["operation"]=="mount-info")
      output={{"mounted",persistentMountPresent(input["mountinfo"].toString().toUtf8(),
          input["mount"].toString(),input["identity"].toString())}};
    else throw Failure("invalid-fixture","test","Invalid parser fixture operation");
    QTextStream(stdout)<<QJsonDocument(output).toJson(QJsonDocument::Compact)<<'\n';return 0;
  } catch(const Failure &failure) {
    QTextStream(stderr)<<QJsonDocument(QJsonObject{{"code",failure.code},{"stage",failure.stage},
        {"message",failure.detail}}).toJson(QJsonDocument::Compact)<<'\n';return 1;
  }
}
