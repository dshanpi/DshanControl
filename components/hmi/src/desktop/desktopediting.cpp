#include "desktopcanvas.h"
#include <QAbstractItemView>
#include <QScroller>
#include <QStandardPaths>
#include <QTranslator>
#include <QToolButton>
namespace
{
// Qt temporarily replaces the accept label when the current item is a folder.
// Keep that standard label localized too, without depending on system .qm files.
class FilePickerTranslation final : public QTranslator
{
  public:
    bool isEmpty() const override { return false; }
    FilePickerTranslation() { QCoreApplication::installTranslator(this); }
    ~FilePickerTranslation() override { QCoreApplication::removeTranslator(this); }
    QString translate(const char *context, const char *source, const char *, int) const override
    {
        const QString key = QString::fromLatin1(source);
        if (qstrcmp(context, "QFileDialog") == 0) {
            static const QHash<QString, QString> labels{
                {"&Open", "打开"}, {"Open", "打开"}, {"&Choose", "选择"},
                {"Look in:", "位置"}, {"File &name:", "文件名"},
                {"Files of type:", "文件类型"}, {"Cancel", "取消"}, {"&Cancel", "取消"}};
            return labels.value(key);
        }
        if (qstrcmp(context, "QFileSystemModel") == 0) {
            static const QHash<QString, QString> labels{
                {"Name", "名称"}, {"Size", "大小"}, {"Type", "类型"},
                {"Date Modified", "修改时间"}};
            return labels.value(key);
        }
        if (qstrcmp(context, "QPlatformTheme") == 0) {
            if (key == "Open") return QStringLiteral("打开");
            if (key == "Cancel") return QStringLiteral("取消");
        }
        return {};
    }
};

QString bindingDialogStyle()
{
    return QStringLiteral(
        "QDialog,QMessageBox{background:#f3f6fb;} "
        "QDialog QWidget,QMessageBox QWidget{font-family:'Dshan Sans';font-size:14px;color:#142c4b;} "
        "QPushButton,QComboBox,QLineEdit{min-height:44px;} "
        "QPushButton{background:white;border:1px solid #d4dfea;border-radius:10px;padding:0 12px;} "
        "QPushButton:pressed{background:#e4efff;} QPushButton:default{background:#1468fa;color:white;} "
        "QAbstractItemView::item{min-height:44px;} QToolButton{min-width:44px;min-height:44px;}");
}
QString chooseLocalFile(QWidget *parent, const QString &title, const QString &current,
                        const QString &filter)
{
    QString location = current;
    if (location.isEmpty())
    {
        location = QStringLiteral("/mnt/UDISK/media");
        if (!QDir(location).exists())
            location = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (location.isEmpty() || !QDir(location).exists())
            location = QDir::homePath();
    }
    FilePickerTranslation translation;
    QFileDialog picker(parent, title, location, filter);
    picker.setObjectName("desktopLocalFilePicker");
    picker.setOption(QFileDialog::DontUseNativeDialog);
    picker.setFileMode(QFileDialog::ExistingFile);
    picker.setLabelText(QFileDialog::LookIn, QStringLiteral("位置"));
    picker.setLabelText(QFileDialog::FileName, QStringLiteral("文件名"));
    picker.setLabelText(QFileDialog::FileType, QStringLiteral("文件类型"));
    picker.setLabelText(QFileDialog::Accept, QStringLiteral("选择"));
    picker.setLabelText(QFileDialog::Reject, QStringLiteral("取消"));
    picker.setStyleSheet(bindingDialogStyle());
    // Embedded images may omit the desktop icon theme; navigation must remain visible.
    const QHash<QString, QString> navigation{
        {"backButton", "后退"}, {"forwardButton", "前进"}, {"toParentButton", "上级"},
        {"newFolderButton", "新建"}, {"listModeButton", "列表"}, {"detailModeButton", "详情"}};
    for (auto it = navigation.cbegin(); it != navigation.cend(); ++it)
        if (auto *button = picker.findChild<QToolButton *>(it.key())) {
            button->setText(it.value());
            button->setAccessibleName(it.value());
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    QList<QUrl> shortcuts{QUrl::fromLocalFile("/")};
    for (const auto &path : {QString("/mnt/UDISK/media"),
                            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)})
        if (!path.isEmpty() && QDir(path).exists())
            shortcuts.append(QUrl::fromLocalFile(path));
    picker.setSidebarUrls(shortcuts);
    const auto *screen = parent->parentWidget() ? parent->parentWidget()->window() : nullptr;
    const QSize available = screen ? screen->size() : QSize(1024, 600);
    picker.resize(qMin(900, available.width() - 32), qMin(520, available.height() - 32));
    for (auto *view : picker.findChildren<QAbstractItemView *>())
        QScroller::grabGesture(view->viewport(), QScroller::TouchGesture);
    return picker.exec() == QDialog::Accepted ? picker.selectedFiles().value(0) : QString();
}
}
namespace DesktopV1
{
QList<QRectF> DesktopCanvas::slotRects()
{
    return {{24, 120, 464, 220},  {504, 120, 240, 220}, {760, 120, 240, 220}, {24, 356, 224, 164},
            {264, 356, 224, 164}, {504, 356, 240, 164}, {760, 356, 240, 164}};
}
int DesktopCanvas::slotAt(const QPointF &point) const
{
    const auto rects = slotRects();
    for (int i = 0; i < rects.size(); ++i)
        if (rects[i].contains(point))
            return i;
    return -1;
}
void DesktopCanvas::configureLayoutStorage(const QString &path)
{
    if (model.fixture)
        return;
    layoutStore.reset(new DesktopLayoutStore(path));
    committedLayout = layoutStore->load(&layoutNotice);
    if (layoutNotice.isEmpty() && !QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
        layoutStore->initializeDefault(&layoutNotice);
    draftLayout = {};
    editingLayout = false;
    desk = committedLayout["active_page"] == "page1" ? 1 : 0;
    refreshImages();
    refreshPointBindings();
    update();
}
QJsonObject DesktopCanvas::widgetAt(int slot) const
{
    if (slot < 0 || slot >= 7)
        return {};
    const auto root = layoutSnapshot();
    for (const auto &page : root["pages"].toArray())
        if (page.toObject()["id"] == QString("page%1").arg(desk))
            for (const auto &entry : page.toObject()["slots"].toArray())
                if (entry.toObject()["slot_id"] == DesktopLayoutStore::slotIds()[slot])
                    return entry.toObject();
    return {};
}
void DesktopCanvas::updateWidget(int slot, const QJsonObject &value)
{
    auto pages = draftLayout["pages"].toArray();
    for (int p = 0; p < pages.size(); ++p)
    {
        auto page = pages[p].toObject();
        if (page["id"] != QString("page%1").arg(desk))
            continue;
        auto entries = page["slots"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["slot_id"] == DesktopLayoutStore::slotIds()[slot])
                entries[i] = value;
        page["slots"] = entries;
        pages[p] = page;
    }
    draftLayout["pages"] = pages;
    refreshImages();
    refreshPointBindings();
    update();
}
bool DesktopCanvas::removeWidget(int slot)
{
    if (model.fixture || !editingLayout || slot < 0 || slot >= 7)
        return false;
    auto value = widgetAt(slot);
    value["type"] = "empty";
    value["binding"] = QJsonObject();
    updateWidget(slot, value);
    layoutNotice.clear();
    return true;
}
bool DesktopCanvas::placeWidget(const QString &type, int slot)
{
    if (model.fixture || !editingLayout)
        return false;
    if (slot < 0)
        slot = librarySlot;
    if (slot < 0)
        for (int i = 0; i < 7; ++i)
            if (widgetAt(i)["type"] == "empty" &&
                DesktopLayoutStore::compatible(DesktopLayoutStore::slotIds()[i], type))
            {
                slot = i;
                break;
            }
    if (slot < 0 || slot >= 7 || widgetAt(slot)["type"] != "empty" ||
        !DesktopLayoutStore::compatible(DesktopLayoutStore::slotIds()[slot], type))
    {
        layoutNotice = "请先移除一个兼容尺寸的组件";
        QToolTip::showText(mapToGlobal(QPoint(width() - 340, 150)), layoutNotice, this);
        return false;
    }
    auto value = widgetAt(slot);
    value["type"] = type;
    value["binding"] = QJsonObject();
    updateWidget(slot, value);
    librarySlot = -1;
    libraryGroup = -1;
    layoutNotice.clear();
    change(3);
    return true;
}
bool DesktopCanvas::swapWidgets(int from, int to)
{
    if (model.fixture || !editingLayout || from < 0 || from >= 7 || to < 0 || to >= 7 || from == to)
        return false;
    auto a = widgetAt(from), b = widgetAt(to);
    const auto ids = DesktopLayoutStore::slotIds();
    if (!DesktopLayoutStore::compatible(ids[to], a["type"].toString()) ||
        !DesktopLayoutStore::compatible(ids[from], b["type"].toString()))
    {
        layoutNotice = "组件尺寸不兼容，已保留原位置";
        return false;
    }
    a["slot_id"] = ids[to];
    b["slot_id"] = ids[from];
    updateWidget(from, b);
    updateWidget(to, a);
    layoutNotice.clear();
    return true;
}
bool DesktopCanvas::commitLayout()
{
    if (model.fixture)
    {
        change(0);
        return true;
    }
    if (!editingLayout || !layoutStore)
        return false;
    auto candidate = draftLayout;
    candidate["revision"] = committedLayout["revision"].toDouble() + 1;
    candidate["active_page"] = QString("page%1").arg(desk);
    if (!layoutStore->commit(candidate, &layoutNotice))
    {
        update();
        return false;
    }
    committedLayout = candidate;
    draftLayout = {};
    editingLayout = false;
    hidden.clear();
    layoutNotice = "工作台布局已保存";
    refreshPointBindings();
    change(0);
    return true;
}
void DesktopCanvas::discardLayout()
{
    if (model.fixture)
        return;
    draftLayout = {};
    editingLayout = false;
    hidden.clear();
    dragSlot = dragTarget = -1;
    librarySlot = -1;
    layoutNotice.clear();
    refreshImages();
    refreshPointBindings();
    change(0);
}
bool DesktopCanvas::resolveLayoutExit()
{
    if (!editingLayout)
        return true;
    if (draftLayout == committedLayout)
    {
        editingLayout = false;
        draftLayout = {};
        return true;
    }
    QMessageBox box(QMessageBox::Question, "工作台尚未保存", "保存当前布局后离开？",
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setStyleSheet(bindingDialogStyle());
    box.setTextFormat(Qt::PlainText);
    box.button(QMessageBox::Save)->setText("保存并离开");
    box.button(QMessageBox::Discard)->setText("放弃修改");
    box.button(QMessageBox::Cancel)->setText("继续编辑");
    box.setDefaultButton(QMessageBox::Cancel);
    const int choice = box.exec();
    if (choice == QMessageBox::Save)
        return commitLayout();
    if (choice == QMessageBox::Discard)
    {
        discardLayout();
        return true;
    }
    return false;
}
void DesktopCanvas::closeLibrary()
{
    libraryGroup = -1;
    librarySlot = -1;
    if (model.fixture)
    {
        change(0);
        return;
    }
    change(libraryReturnState);
}
void DesktopCanvas::chooseLibraryItem(int group)
{
    const QList<QStringList> types = {{"trend"}, {"image", "video"}, {"bus"},
                                      {"cloud"}, {"alarms"},         {"home", "voice"}};
    const QList<QStringList> names = {{"设备趋势"}, {"设备图片", "视频播放"},
                                      {"现场总线"}, {"云端数据"},
                                      {"告警摘要"}, {"智能家庭", "语音对话"}};
    if (group < 0 || group >= types.size())
        return;
    if (types[group].size() == 1)
    {
        placeWidget(types[group][0]);
        return;
    }
    libraryGroup = group;
    hits.clear();
    update();
}
} // namespace DesktopV1

namespace DesktopV1
{
void DesktopCanvas::refreshImages()
{
    if (!imageCache)
        return;
    QStringList paths;
    for (const auto &page : layoutSnapshot()["pages"].toArray())
        for (const auto &item : page.toObject()["slots"].toArray())
        {
            const auto widget = item.toObject();
            if (widget["type"] == "image")
                paths.append(widget["binding"].toObject()["path"].toString());
        }
    imageCache->setPaths(paths);
}
bool DesktopCanvas::bindImage(int slot, const QString &path, const QString &fit)
{
    if (model.fixture || !editingLayout || widgetAt(slot)["type"] != "image")
        return false;
    if ((!path.isEmpty() && (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path ||
                             path.size() > 4096 || path.contains(QChar(0)))) ||
        (fit != "contain" && fit != "cover"))
        return false;
    QJsonObject binding;
    if (!path.isEmpty())
    {
        binding["path"] = path;
        binding["fit"] = fit;
    }
    auto widget = widgetAt(slot);
    const bool samePath = widget["binding"].toObject()["path"] == path;
    widget["binding"] = binding;
    updateWidget(slot, widget);
    if (!path.isEmpty() && samePath)
        imageCache->reload(path);
    return true;
}
QImage DesktopCanvas::boundImage(int slot) const
{
    return imageCache ? imageCache->image(widgetAt(slot)["binding"].toObject()["path"].toString())
                      : QImage();
}
void DesktopCanvas::editImageBinding(int slot)
{
    if (!editingLayout || widgetAt(slot)["type"] != "image")
        return;
    const auto binding = widgetAt(slot)["binding"].toObject();
    QString selectedPath = binding["path"].toString();
    QDialog dialog(this);
    dialog.setWindowTitle("图片组件");
    dialog.setObjectName("desktopImageBindingDialog");
    dialog.setMinimumWidth(500);
    dialog.setStyleSheet(bindingDialogStyle());
    auto *layout = new QVBoxLayout(&dialog);
    auto *pathLabel = new QLabel(selectedPath.isEmpty() ? QString("尚未绑定图片")
                                                        : QFileInfo(selectedPath).fileName());
    pathLabel->setTextFormat(Qt::PlainText);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);
    auto *choose = new QPushButton("选择本地 PNG / JPEG");
    choose->setObjectName("desktopImageChoose");
    layout->addWidget(choose);
    connect(choose, &QPushButton::clicked, &dialog, [&]() {
        const auto file = chooseLocalFile(&dialog, "选择组件图片", selectedPath,
                                                       "图片 (*.png *.jpg *.jpeg)");
        if (!file.isEmpty())
        {
            selectedPath = QFileInfo(file).absoluteFilePath();
            pathLabel->setText(QFileInfo(file).fileName());
        }
    });
    auto *fit = new QComboBox;
    fit->addItem("完整显示", "contain");
    fit->addItem("填满并裁剪", "cover");
    fit->setCurrentIndex(binding["fit"] == "cover" ? 1 : 0);
    layout->addWidget(fit);
    auto *clear = new QPushButton("解除图片绑定");
    layout->addWidget(clear);
    connect(clear, &QPushButton::clicked, &dialog, [&]() {
        selectedPath.clear();
        pathLabel->setText("尚未绑定图片");
    });
    auto *note = new QLabel("最多 8 MiB / 400 "
                            "万像素；配置在完成编辑后保存。\n读取失败时保留绑定，重新选择可重试。");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("应用到草稿");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        bindImage(slot, selectedPath, fit->currentData().toString());
}
void DesktopCanvas::showImage(int slot)
{
    if (model.fixture)
    {
        openApp(10);
        return;
    }
    const auto image = boundImage(slot);
    if (image.isNull())
    {
        change(3);
        editImageBinding(slot);
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("设备图片");
    dialog.setStyleSheet(bindingDialogStyle());
    dialog.resize(qMin(960, width() - 32), qMin(540, height() - 32));
    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(QPixmap::fromImage(image).scaled(
        dialog.width() - 40, dialog.height() - 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(label, 1);
    auto *close = new QPushButton("返回工作台");
    close->setMinimumHeight(44);
    layout->addWidget(close);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}
void DesktopCanvas::drawBoundImage(QPainter &p, const QRectF &rect, const QJsonObject &binding)
{
    if (model.fixture)
    {
        thumbnail(p, rect);
        return;
    }
    const auto path = binding["path"].toString();
    const auto image = imageCache->image(path);
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(rect, 12, 12);
    p.setClipPath(clip);
    p.fillRect(rect, QColor("#DCE7EC"));
    if (!image.isNull())
    {
        const auto size =
            QSizeF(image.size())
                .scaled(rect.size(), binding["fit"] == "cover" ? Qt::KeepAspectRatioByExpanding
                                                               : Qt::KeepAspectRatio);
        const QRectF dest(rect.center() - QPointF(size.width() / 2, size.height() / 2), size);
        p.drawImage(dest, image);
    }
    else
        txt(p, rect.x() + 8, rect.y() + 8, rect.width() - 16, rect.height() - 16,
            imageCache->status(path), 12, QColor("#71859D"), false,
            Qt::AlignCenter | Qt::TextWordWrap);
    p.restore();
}
} // namespace DesktopV1

namespace DesktopV1
{
void DesktopCanvas::refreshPointBindings()
{
    QStringList ids;
    for (const auto &layout : {committedLayout, layoutSnapshot()})
        for (const auto &page : layout["pages"].toArray())
            for (const auto &item : page.toObject()["slots"].toArray())
            {
                const auto widget = item.toObject();
                if (widget["type"] != "trend")
                    continue;
                const auto binding = widget["binding"].toObject();
                ids << binding["point_id"].toString() << binding["secondary_point_id"].toString();
            }
    pointHistory.setBindings(ids);
    pointHistory.ingest(availablePoints, pointsConnected, QDateTime::currentMSecsSinceEpoch(),
                        pointClock.elapsed());
}
bool DesktopCanvas::bindTrend(int slot, const QString &pointId, const QString &secondaryId)
{
    if (model.fixture || !editingLayout || widgetAt(slot)["type"] != "trend")
        return false;
    const auto old = widgetAt(slot)["binding"].toObject();
    for (const auto &id : {pointId, secondaryId})
    {
        if (id.isEmpty())
            continue;
        if (!DesktopPointHistory::validId(id))
            return false;
        bool found = old["point_id"] == id || old["secondary_point_id"] == id;
        for (const auto &point : availablePoints)
            found |= point.toObject()["point_id"] == id;
        if (!found)
            return false;
    }
    QJsonObject binding;
    if (!pointId.isEmpty())
        binding["point_id"] = pointId;
    if (!secondaryId.isEmpty())
        binding["secondary_point_id"] = secondaryId;
    auto widget = widgetAt(slot);
    widget["binding"] = binding;
    updateWidget(slot, widget);
    return true;
}
QJsonObject DesktopCanvas::boundPoint(int slot, bool secondary) const
{
    const auto binding = widgetAt(slot)["binding"].toObject();
    return pointHistory.current(binding[secondary ? "secondary_point_id" : "point_id"].toString(),
                                QDateTime::currentMSecsSinceEpoch(), pointClock.elapsed());
}
QVector<TrendSample> DesktopCanvas::boundHistory(int slot, bool secondary) const
{
    return pointHistory.series(widgetAt(slot)["binding"]
                                   .toObject()[secondary ? "secondary_point_id" : "point_id"]
                                   .toString(),
                               QDateTime::currentMSecsSinceEpoch());
}
void DesktopCanvas::editTrendBinding(int slot)
{
    const auto current = widgetAt(slot)["binding"].toObject();
    QDialog dialog(this);
    dialog.setWindowTitle("趋势点位绑定");
    dialog.setMinimumWidth(600);
    dialog.setStyleSheet(bindingDialogStyle());
    auto *form = new QFormLayout(&dialog);
    QList<QComboBox *> boxes;
    for (const auto &key : {QString("point_id"), QString("secondary_point_id")})
    {
        auto *box = new QComboBox;
        box->addItem("未绑定", QString());
        for (const auto &item : availablePoints)
        {
            const auto point = item.toObject();
            const auto id = point["point_id"].toString();
            if (!DesktopPointHistory::validId(id) || box->findData(id) >= 0)
                continue;
            box->addItem(point["name"].toString(id).left(48) + " · " + id + " [" +
                             point["unit"].toString().left(16) + "]",
                         id);
        }
        const auto chosen = current[key].toString();
        int index = box->findData(chosen);
        if (index < 0)
        {
            box->addItem("已不可用 · " + chosen, chosen);
            index = box->count() - 1;
        }
        box->setCurrentIndex(index);
        boxes.append(box);
        form->addRow(boxes.size() == 1 ? "主点位" : "对照点位", box);
    }
    auto *note = new QLabel(pointsConnected ? "列表来自当前数据服务；曲线仅保留本次会话的实际采样。"
                                            : "数据服务未连接，可保留原绑定或选择未绑定。");
    note->setWordWrap(true);
    form->addRow(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    buttons->button(QDialogButtonBox::Save)->setText("应用到草稿");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        bindTrend(slot, boxes[0]->currentData().toString(), boxes[1]->currentData().toString());
}
void DesktopCanvas::drawPointTrend(QPainter &p, const QRectF &r, const QJsonObject &binding)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool any = false;
    for (int lineIndex = 0; lineIndex < 2; ++lineIndex)
    {
        const auto id = binding[lineIndex ? "secondary_point_id" : "point_id"].toString();
        const auto data = pointHistory.series(id, now);
        if (data.isEmpty())
            continue;
        any = true;
        double magnitude = 1;
        for (const auto &sample : data)
            magnitude = qMax(magnitude, std::abs(sample.value));
        double low = 1, high = -1;
        for (const auto &sample : data)
        {
            low = qMin(low, sample.value / magnitude);
            high = qMax(high, sample.value / magnitude);
        }
        const double margin = qMax(.05, (high - low) * .1);
        low -= margin;
        high += margin;
        p.save();
        p.setClipRect(r.adjusted(-2, -2, 2, 2));
        p.setPen(QPen(QColor(lineIndex ? "#159D85" : "#1468FA"), 2));
        p.setBrush(Qt::NoBrush);
        QPainterPath line;
        bool started = false;
        int lastBucket = -1;
        QPointF minimum, maximum;
        bool bucketOpen = false;
        auto flush = [&]() {
            if (!bucketOpen)
                return;
            p.drawLine(minimum, maximum);
            bucketOpen = false;
        };
        for (const auto &sample : data)
        {
            const qreal x = r.left() + r.width() * (sample.utc - (now - 1800000)) / 1800000.0;
            const qreal y =
                r.bottom() - r.height() * (sample.value / magnitude - low) / (high - low);
            const QPointF position(x, y);
            const int bucket = int(x);
            if (sample.startsSegment)
            {
                flush();
                started = false;
                lastBucket = -1;
            }
            if (!started)
            {
                line.moveTo(position);
                p.drawEllipse(position, 1.5, 1.5);
                started = true;
            }
            else if (bucket != lastBucket)
                line.lineTo(position);
            if (bucket != lastBucket)
            {
                flush();
                minimum = maximum = position;
                bucketOpen = true;
            }
            else
            {
                if (y < minimum.y())
                    minimum = position;
                if (y > maximum.y())
                    maximum = position;
            }
            lastBucket = bucket;
        }
        flush();
        p.drawPath(line);
        p.restore();
    }
    if (!any)
        txt(p, r.x(), r.y(), r.width(), r.height(),
            binding.isEmpty() ? QString("绑定点位后显示趋势")
                              : QString("等待有效采样 · 不补生成历史"),
            12, QColor("#71859D"), false, Qt::AlignCenter);
    else
        txt(p, r.x(), r.bottom() + 6, r.width(), 20, "最近 30 分钟 · 本次会话 · 两条曲线各自量程",
            11, QColor("#71859D"));
}
} // namespace DesktopV1

namespace DesktopV1
{
bool DesktopCanvas::restoreDefaultLayout()
{
    if (model.fixture || !layoutStore)
        return false;
    if (!layoutStore->restoreDefaults(&layoutNotice))
    {
        update();
        return false;
    }
    committedLayout = DesktopLayoutStore::defaults();
    draftLayout = {};
    editingLayout = false;
    desk = 0;
    hidden.clear();
    refreshImages();
    refreshPointBindings();
    layoutNotice = "默认布局已恢复；原文件保留在 .before-reset 备份";
    change(0);
    return true;
}
} // namespace DesktopV1

namespace DesktopV1
{
bool DesktopCanvas::bindVideo(int slot, const QString &path)
{
    if (model.fixture || !editingLayout || widgetAt(slot)["type"] != "video")
        return false;
    if (!path.isEmpty() && (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path ||
                            path.size() > 4096 || path.contains(QChar(0))))
        return false;
    auto widget = widgetAt(slot);
    widget["binding"] = path.isEmpty() ? QJsonObject() : QJsonObject{{"path", path}};
    updateWidget(slot, widget);
    return true;
}
bool DesktopCanvas::bindHome(int slot, const QString &deviceId)
{
    if (model.fixture || !editingLayout || widgetAt(slot)["type"] != "home")
        return false;
    if (!deviceId.isEmpty())
    {
        if (!QRegularExpression("^[A-Za-z0-9_.:-]{1,128}$").match(deviceId).hasMatch())
            return false;
        bool found = widgetAt(slot)["binding"].toObject()["device_id"] == deviceId;
        for (const auto &entry : homeSnapshot["devices"].toArray())
            found |= entry.toObject()["id"] == deviceId;
        if (!found)
            return false;
    }
    auto widget = widgetAt(slot);
    widget["binding"] = deviceId.isEmpty() ? QJsonObject() : QJsonObject{{"device_id", deviceId}};
    updateWidget(slot, widget);
    return true;
}
void DesktopCanvas::editVideoBinding(int slot)
{
    QString path=widgetAt(slot)["binding"].toObject()["path"].toString();
    QDialog dialog(this);dialog.setWindowTitle("视频组件绑定");dialog.setMinimumWidth(500);
    dialog.setStyleSheet(bindingDialogStyle());
    auto *layout=new QVBoxLayout(&dialog);
    auto *label=new QLabel(path.isEmpty()?QString("尚未绑定视频"):QFileInfo(path).fileName());
    label->setTextFormat(Qt::PlainText);label->setWordWrap(true);layout->addWidget(label);
    auto *choose=new QPushButton("选择本地视频");auto *clear=new QPushButton("解除视频绑定");
    layout->addWidget(choose);layout->addWidget(clear);
    connect(choose,&QPushButton::clicked,&dialog,[&]() {
        const auto selected=chooseLocalFile(&dialog,"绑定本地视频",path,
            "视频 (*.mp4 *.mkv *.avi *.mov *.ts *.webm)");
        if(!selected.isEmpty()){path=QFileInfo(selected).absoluteFilePath();label->setText(QFileInfo(path).fileName());}
    });
    connect(clear,&QPushButton::clicked,&dialog,[&](){path.clear();label->setText("尚未绑定视频");});
    layout->addWidget(new QLabel("绑定后点击组件进入播放页，再点击播放。",&dialog));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("应用到草稿");buttons->button(QDialogButtonBox::Cancel)->setText("取消");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted)bindVideo(slot,path);
}
void DesktopCanvas::editHomeBinding(int slot)
{
    const auto current = widgetAt(slot)["binding"].toObject()["device_id"].toString();
    QDialog dialog(this);
    dialog.setWindowTitle("家庭组件绑定");
    dialog.setMinimumWidth(500);
    dialog.setStyleSheet(bindingDialogStyle());
    auto *layout = new QVBoxLayout(&dialog);
    auto *choose = new QComboBox;
    choose->addItem("家庭总览（不绑定设备）", QString());
    for (const auto &entry : homeSnapshot["devices"].toArray())
    {
        const auto d = entry.toObject();
        choose->addItem(d["name"].toString() + " · " + d["id"].toString(), d["id"]);
    }
    if (!current.isEmpty() && choose->findData(current) < 0)
        choose->addItem("当前设备暂不可用 · " + current, current);
    choose->setCurrentIndex(qMax(0, choose->findData(current)));
    layout->addWidget(choose);
    auto *note = new QLabel("仅绑定展示入口，不会发送设备控制指令。", &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("应用到草稿");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        bindHome(slot, choose->currentData().toString());
}
void DesktopCanvas::openWidget(int slot)
{
    const auto widget = widgetAt(slot);
    const auto type = widget["type"].toString();
    const auto binding = widget["binding"].toObject();
    if (type == "image")
    {
        showImage(slot);
        return;
    }
    if(!model.fixture && type=="trend" && !binding.isEmpty()) {
        emit appContextRequested("devices",binding);return;
    }
    if (!model.fixture && (type == "home" || type == "video") && !binding.isEmpty())
    {
        emit appContextRequested(type, binding);
        return;
    }
    const QMap<QString, int> routes = {{"trend", 3},  {"video", 10}, {"cloud", 7}, {"bus", 1},
                                       {"alarms", 4}, {"home", 9},   {"voice", 8}};
    if (routes.contains(type))
        openApp(routes.value(type));
}
} // namespace DesktopV1

namespace DesktopV1 {
bool DesktopCanvas::canPlaceWidget(const QString &type) const
{
    for(int i=0;i<7;++i)
        if((librarySlot<0 || librarySlot==i) && widgetAt(i)["type"]=="empty" &&
            DesktopLayoutStore::compatible(DesktopLayoutStore::slotIds()[i],type))return true;
    return false;
}
bool DesktopCanvas::backLibraryLevel()
{
    if(state!=4 || libraryGroup<0)return false;
    libraryGroup=-1;hits.clear();update();return true;
}
}
