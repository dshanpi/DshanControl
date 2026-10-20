#include "desktoplayout.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <utility>

namespace DesktopV1
{
namespace
{
bool reject(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}
bool exact(const QJsonObject &o, const QStringList &keys)
{
    auto a = o.keys(), b = keys;
    a.sort();
    b.sort();
    return a == b;
}
bool boundedString(const QJsonValue &v, int max)
{
    return v.isString() && v.toString().size() <= max && !v.toString().contains(QChar(0));
}
QByteArray hash(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}
} // namespace
DesktopLayoutStore::DesktopLayoutStore(QString path) : filePath(std::move(path)) {}
QStringList DesktopLayoutStore::slotIds()
{
    return {"trend", "media", "cloud", "bus", "alarms", "home", "voice"};
}
QJsonObject DesktopLayoutStore::defaults()
{
    QFile file(":/desktop/assets/default-layout.json");
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Missing desktop layout resource");
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool DesktopLayoutStore::compatible(const QString &slot, const QString &type)
{
    if (!slotIds().contains(slot))
        return false;
    if (type == "empty")
        return true;
    if (slot == "trend")
        return type == "trend" || type == "image";
    if (slot == "media")
        return type == "video" || type == "image";
    if (slot == "cloud")
        return type == "cloud";
    return QStringList{"bus", "alarms", "home", "voice"}.contains(type);
}
bool DesktopLayoutStore::validate(const QJsonObject &layout, QString *error)
{
    if (!exact(layout, {"schema_version", "revision", "active_page", "pages"}) ||
        layout["schema_version"] != 1)
        return reject(error, "布局版本不兼容，原文件已保留");
    auto revision = layout["revision"];
    if (!revision.isDouble() || !std::isfinite(revision.toDouble()) || revision.toDouble() < 1 ||
        revision.toDouble() > 9007199254740990.0 ||
        std::floor(revision.toDouble()) != revision.toDouble())
        return reject(error, "布局版本号无效");
    const auto pages = layout["pages"].toArray();
    if (!layout["pages"].isArray() || pages.size() != 2)
        return reject(error, "布局必须包含两页");
    QSet<QString> pageIds, instanceIds;
    const QRegularExpression idPattern("^[A-Za-z0-9_-]{1,64}$");
    for (const auto &entry : pages)
    {
        const auto page = entry.toObject();
        const auto id = page["id"].toString();
        if (!exact(page, {"id", "slots"}) || (id != "page0" && id != "page1") ||
            pageIds.contains(id))
            return reject(error, "工作台页面标识无效或重复");
        pageIds.insert(id);
        const auto instances = page["slots"].toArray();
        if (!page["slots"].isArray() || instances.size() != 7)
            return reject(error, "每页必须包含七个槽位");
        QSet<QString> slotSet;
        for (const auto &value : instances)
        {
            const auto w = value.toObject();
            const auto slot = w["slot_id"].toString(), instance = w["id"].toString();
            if (!exact(w, {"id", "slot_id", "type", "binding"}) ||
                !idPattern.match(instance).hasMatch() || instanceIds.contains(instance) ||
                slotSet.contains(slot) || !compatible(slot, w["type"].toString()))
                return reject(error, "组件类型、标识或槽位不兼容");
            instanceIds.insert(instance);
            slotSet.insert(slot);
            if (!w["binding"].isObject())
                return reject(error, "组件绑定必须是对象");
            const auto binding = w["binding"].toObject();
            for (auto it = binding.begin(); it != binding.end(); ++it)
            {
                const QString key = it.key();
                const int limit = key == "path" ? 4096 : key == "device_id" ? 128 : 256;
                if (!QStringList{"point_id", "secondary_point_id", "device_id", "path", "fit"}
                         .contains(key) ||
                    !boundedString(it.value(), limit))
                    return reject(error, "组件绑定字段无效");
                if (key == "path" &&
                    (!QDir::isAbsolutePath(it.value().toString()) ||
                     QDir::cleanPath(it.value().toString()) != it.value().toString()))
                    return reject(error, "媒体绑定必须使用规范的本地绝对路径");
                if (key == "fit" && it.value() != "contain" && it.value() != "cover")
                    return reject(error, "图片显示方式无效");
            }
        }
    }
    if (!pageIds.contains(layout["active_page"].toString()))
        return reject(error, "当前工作台页面不存在");
    if (QJsonDocument(layout).toJson(QJsonDocument::Compact).size() > 131072)
        return reject(error, "布局文件过大");
    return true;
}
QJsonObject DesktopLayoutStore::load(QString *notice)
{
    writableSchema = true;
    existed = QFileInfo::exists(filePath);
    baseline.clear();
    if (notice)
        notice->clear();
    if (!QDir::isAbsolutePath(filePath) || QDir::cleanPath(filePath) != filePath ||
        QFileInfo(filePath).isSymLink())
    {
        writableSchema = false;
        reject(notice, "布局路径不可用，原文件未修改");
        return defaults();
    }
    if (!existed)
        return defaults();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 131072)
    {
        writableSchema = false;
        reject(notice, "布局无法读取，原文件已保留");
        return defaults();
    }
    const auto bytes = file.read(131073);
    baseline = hash(bytes);
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(bytes, &parse);
    QString error;
    if (parse.error != QJsonParseError::NoError || !doc.isObject() ||
        !validate(doc.object(), &error))
    {
        writableSchema = false;
        reject(notice, error.isEmpty() ? QString("布局损坏，原文件已保留") : error);
        return defaults();
    }
    return doc.object();
}
bool DesktopLayoutStore::initializeDefault(QString *error)
{
    // Called after load, independently of an edit. A missing provisioned parent
    // is an error; the unprivileged GUI never creates lifecycle-owned directories.
    if (existed || !writableSchema)
        return false;
    return commit(defaults(), error);
}
bool DesktopLayoutStore::commit(const QJsonObject &layout, QString *error)
{
    if (!writableSchema)
        return reject(error, "原布局异常或版本不兼容，已保留；请先处理原文件");
    if (!validate(layout, error))
        return false;
    if (!QDir::isAbsolutePath(filePath) || !QFileInfo(QFileInfo(filePath).absolutePath()).isDir() ||
        QFileInfo(filePath).isSymLink())
        return reject(error, "布局目录不可用，请检查用户数据目录权限");
    QLockFile lock(filePath + ".lock");
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(0))
        return reject(error, lock.error() == QLockFile::PermissionError
                                 ? QString("无法写入布局目录，请检查权限")
                                 : QString("布局正被其他进程保存，请稍后重试"));
    QFile prior(filePath);
    const bool existsNow = QFileInfo::exists(filePath);
    if (existsNow != existed ||
        (existsNow && (!prior.open(QIODevice::ReadOnly) || prior.size() > 131072 ||
                       hash(prior.read(131073)) != baseline)))
        return reject(error, "布局已被其他进程修改，请重新加载后编辑");
    const auto bytes = QJsonDocument(layout).toJson(QJsonDocument::Indented);
    if (bytes.size() > 131072)
        return reject(error, "布局文件过大");
    QSaveFile output(filePath);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly) ||
        !output.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
        output.write(bytes) != bytes.size() ||
        !output.commit())
        return reject(error, "保存失败，原布局已保留：" + output.errorString());
    existed = true;
    baseline = hash(bytes);
    return true;
}
} // namespace DesktopV1

namespace DesktopV1
{
bool DesktopLayoutStore::restoreDefaults(QString *error)
{
    if (!QDir::isAbsolutePath(filePath) || QDir::cleanPath(filePath) != filePath ||
        QFileInfo(filePath).isSymLink() || !QFileInfo(QFileInfo(filePath).absolutePath()).isDir())
        return reject(error, "布局路径不可用，未恢复默认");
    QLockFile lock(filePath + ".lock");
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(0))
        return reject(error, "布局目录不可写或正在保存，请稍后重试");
    QFile prior(filePath);
    const bool existsNow = QFileInfo::exists(filePath);
    QByteArray bytes;
    if (existsNow != existed ||
        (existsNow && (!prior.open(QIODevice::ReadOnly) || prior.size() > 131072)))
        return reject(error, "原布局发生变化或超过备份上限，已保留，请先检查原文件");
    if (existsNow)
    {
        bytes = prior.read(131073);
        prior.close();
        if (bytes.size() > 131072 || hash(bytes) != baseline)
            return reject(error, "原布局已变化，重新加载后再恢复");
        const auto backup = filePath + ".before-reset";
        if (QFileInfo(backup).isSymLink())
            return reject(error, "备份路径不可用，原布局已保留");
        QSaveFile copy(backup);
        copy.setDirectWriteFallback(false);
        if (!copy.open(QIODevice::WriteOnly) || copy.write(bytes) != bytes.size() || !copy.commit())
            return reject(error, "备份未完成，原布局未修改");
    }
    const auto replacement = QJsonDocument(defaults()).toJson(QJsonDocument::Indented);
    QSaveFile output(filePath);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly) || output.write(replacement) != replacement.size() ||
        !output.commit())
        return reject(error, "恢复失败，原布局及备份已保留");
    existed = true;
    baseline = hash(replacement);
    writableSchema = true;
    return true;
}
} // namespace DesktopV1
