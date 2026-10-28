#pragma once
#include <QFileDialog>
#include <QCoreApplication>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QMessageBox>
#include <QFileInfo>
#include <QScroller>
#include <QToolButton>
#include <QTranslator>

// Scoped localization also covers Qt changing "Select" to "Open" for directories.
namespace TouchFiles {
class Translation final : public QTranslator {
public:
    bool isEmpty() const override { return false; }
    explicit Translation(QObject *parent = nullptr) : QTranslator(parent) { QCoreApplication::installTranslator(this); }
    ~Translation() override { QCoreApplication::removeTranslator(this); }
    QString translate(const char *context, const char *source, const char *, int) const override {
        const QString key = QString::fromLatin1(source);
        if (qstrcmp(context, "QFileDialog") == 0 || qstrcmp(context, "QPlatformTheme") == 0 ||
            qstrcmp(context, "QDialogButtonBox") == 0 || qstrcmp(context, "QMessageBox") == 0) {
            static const QHash<QString, QString> words{
                {"OK", "确定"}, {"&OK", "确定"}, {"Yes", "确认"}, {"&Yes", "确认"},
                {"No", "取消"}, {"&No", "取消"}, {"Close", "关闭"}, {"&Close", "关闭"},
                {"Apply", "应用"}, {"Reset", "重置"}, {"Save All", "全部保存"},
                {"Open", "打开"}, {"&Open", "打开"}, {"Choose", "选择"}, {"&Choose", "选择"},
                {"Cancel", "取消"}, {"&Cancel", "取消"}, {"Save", "保存"}, {"&Save", "保存"},
                {"Look in:", "位置"}, {"File &name:", "文件名"}, {"Files of type:", "文件类型"},
                {"Back", "后退"}, {"Forward", "前进"}, {"Parent Directory", "上级文件夹"},
                {"List View", "列表"}, {"Detail View", "详细列表"}, {"New Folder", "新建文件夹"},
                {"Show ", "显示 "}, {"All Files (*)", "所有文件 (*)"}};
            return words.value(key);
        }
        if (qstrcmp(context, "QFileSystemModel") == 0) {
            static const QHash<QString, QString> words{
                {"Name", "名称"}, {"Size", "大小"}, {"Type", "类型"}, {"Date Modified", "修改时间"},
                {"Folder", "文件夹"}, {"Directory", "文件夹"}, {"File", "文件"}};
            return words.value(key);
        }
        return {};
    }
};
inline void configure(QFileDialog &picker) {
    picker.setOption(QFileDialog::DontUseNativeDialog);
    picker.setViewMode(QFileDialog::Detail);
    picker.setLabelText(QFileDialog::LookIn, "位置");
    picker.setLabelText(QFileDialog::FileName, "文件名");
    picker.setLabelText(QFileDialog::FileType, "文件类型");
    picker.setLabelText(QFileDialog::Accept,
                        picker.acceptMode() == QFileDialog::AcceptSave ? "保存" : "选择");
    picker.setLabelText(QFileDialog::Reject, "取消");
    picker.setStyleSheet(
        "QFileDialog{background:#f4f4f4;}"
        "QFileDialog QWidget{font-family:'Dshan Sans';font-size:14px;color:#161616;}"
        "QPushButton,QToolButton,QComboBox,QLineEdit{min-height:46px;}"
        "QPushButton,QToolButton{background:white;border:1px solid #a8a8a8;border-radius:2px;padding:0 10px;}"
        "QPushButton:default{background:#0f62fe;color:white;border-color:#0f62fe;}"
        "QPushButton:disabled,QToolButton:disabled{color:#8d8d8d;background:#e0e0e0;}"
        "QPushButton:focus,QToolButton:focus,QLineEdit:focus{border:2px solid #0f62fe;}"
        "QAbstractItemView{background:white;selection-background-color:#d0e2ff;selection-color:#161616;}"
        "QAbstractItemView::item{min-height:46px;}"
        "QHeaderView::section{background:#e0e0e0;padding:6px;border:0;}"
        "QScrollBar:vertical{width:18px;}");
    const QHash<QString,QString> navigation{
        {"backButton","后退"}, {"forwardButton","前进"}, {"toParentButton","上级"},
        {"newFolderButton","新建"}, {"listModeButton","列表"}, {"detailModeButton","详情"}};
    for (auto it=navigation.cbegin(); it!=navigation.cend(); ++it)
        if (auto *button=picker.findChild<QToolButton *>(it.key())) {
            button->setText(it.value());
            button->setAccessibleName(it.value());
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    for (auto *view : picker.findChildren<QAbstractItemView *>())
        QScroller::grabGesture(view->viewport(), QScroller::TouchGesture);
    const QSize size = picker.parentWidget() ? picker.parentWidget()->window()->size() : QSize(1024,600);
    picker.resize(qMin(960,size.width()-24),qMin(560,size.height()-24));
}
inline QString saveFile(QWidget *parent, const QString &title, const QString &path,
                        const QString &filter) {
    Translation translation;
    QFileDialog picker(parent,title,path,filter);
    picker.setObjectName("exportFilePicker");
    picker.setAcceptMode(QFileDialog::AcceptSave);
    picker.setFileMode(QFileDialog::AnyFile);
    picker.setDefaultSuffix("csv");
    picker.setOption(QFileDialog::DontConfirmOverwrite);
    configure(picker);
    if (picker.exec()!=QDialog::Accepted) return {};
    const QString selected=picker.selectedFiles().value(0);
    if (QFileInfo::exists(selected)) {
        QMessageBox confirmation(QMessageBox::Question,"文件已存在",
            "是否覆盖“" + QFileInfo(selected).fileName() + "”？",QMessageBox::Yes|QMessageBox::No,parent);
        confirmation.setTextFormat(Qt::PlainText);
        confirmation.button(QMessageBox::Yes)->setText("覆盖保存");
        confirmation.button(QMessageBox::No)->setText("取消");
        confirmation.setDefaultButton(QMessageBox::No);
        if (confirmation.exec()!=QMessageBox::Yes) return {};
    }
    return selected;
}
}
