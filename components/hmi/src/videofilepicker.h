#pragma once
#include "visuals.h"
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QScroller>

// Read-only media browsing. Selecting a file never starts the decoder.
class VideoFilePicker final : public QDialog {
public:
    VideoFilePicker(QWidget *parent, const QString &initial) : QDialog(parent) {
        setObjectName("mediaFilePicker");
        setWindowTitle("选择视频");
        setStyleSheet(
            "QDialog{background:#f4f4f4;}"
            "QWidget{font-family:'Dshan Sans';font-size:14px;color:#161616;}"
            "QLabel#pickerTitle{font-size:22px;font-weight:600;}"
            "QLabel#pickerHint{color:#525252;}"
            "QLabel#pickerNotice{color:#8a3800;background:#fff1e6;padding:8px;}"
            "QPushButton{min-height:46px;padding:0 14px;background:white;border:1px solid #a8a8a8;border-radius:2px;}"
            "QPushButton#pickerAccept{background:#0f62fe;color:white;border-color:#0f62fe;}"
            "QPushButton#pickerAccept:disabled{color:#8d8d8d;background:#e0e0e0;border-color:#c6c6c6;}"
            "QPushButton:disabled{color:#8d8d8d;background:#e0e0e0;border-color:#c6c6c6;}"
            "QPushButton:focus{border:2px solid #0f62fe;}"
            "QTableWidget{background:white;border:1px solid #d0d0d0;selection-background-color:#d0e2ff;selection-color:#161616;}"
            "QHeaderView::section{padding:8px;background:#e0e0e0;border:none;color:#525252;}"
            "QScrollBar:vertical{width:20px;}");
        auto *root=new QVBoxLayout(this);
        root->setContentsMargins(20,16,20,16); root->setSpacing(8);
        auto *heading=new QLabel("选择视频"); heading->setObjectName("pickerTitle");
        root->addWidget(heading);
        auto *hint=new QLabel("先选择文件，再回到播放器点击“播放”。选择文件夹后点击“打开文件夹”。");
        hint->setObjectName("pickerHint"); hint->setWordWrap(true); root->addWidget(hint);
        auto *places=new QHBoxLayout;
        auto place=[&](const QString &name,const QString &path) {
            auto *button=new QPushButton(name);
            places->addWidget(button);
            connect(button,&QPushButton::clicked,this,[this,path]() { load(path); });
        };
        place("视频目录",initial); place("U 盘","/mnt/UDISK"); place("文件系统","/");
        places->addStretch();
        auto *up=new QPushButton("上级文件夹"); up->setObjectName("pickerUp");
        places->addWidget(up);
        connect(up,&QPushButton::clicked,this,[this]() {
            QDir directory(currentDirectory); if (directory.cdUp()) load(directory.absolutePath());
        });
        root->addLayout(places);
        location=new QLabel; location->setTextFormat(Qt::PlainText); location->setWordWrap(true);
        location->setObjectName("pickerLocation"); root->addWidget(location);
        table=new DataTable(3); table->setObjectName("pickerFiles");
        table->setHorizontalHeaderLabels({"名称","类型","大小"});
        table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Fixed);
        table->setColumnWidth(1,110); table->setColumnWidth(2,110);
        table->verticalHeader()->hide(); table->verticalHeader()->setDefaultSectionSize(50);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false); table->setMinimumHeight(150);
        QScroller::grabGesture(table->viewport(),QScroller::TouchGesture);
        root->addWidget(table,1);
        notice=new QLabel; notice->setObjectName("pickerNotice");
        notice->setWordWrap(true); notice->setTextFormat(Qt::PlainText); root->addWidget(notice);
        auto *bottom=new QHBoxLayout;
        selection=new QLabel("尚未选择视频"); selection->setTextFormat(Qt::PlainText);
        selection->setWordWrap(true); selection->setObjectName("pickerSelection");
        bottom->addWidget(selection,1);
        auto *cancel=new QPushButton("取消"); cancel->setObjectName("pickerCancel"); bottom->addWidget(cancel);
        connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
        acceptButton=new QPushButton("选择视频"); acceptButton->setObjectName("pickerAccept");
        acceptButton->setMinimumWidth(130); acceptButton->setEnabled(false); bottom->addWidget(acceptButton);
        root->addLayout(bottom);
        connect(table,&QTableWidget::itemSelectionChanged,this,[this]() {
            const int row=table->currentRow();
            const auto *item=row<0 ? nullptr : table->item(row,0);
            if (!item || !item->isSelected()) { acceptButton->setEnabled(false); return; }
            const bool folder=item->data(Qt::UserRole+1).toBool();
            selection->setText((folder ? "文件夹：" : "已选视频：")+item->text());
            acceptButton->setText(folder ? "打开文件夹" : "选择视频");
            acceptButton->setEnabled(true);
        });
        connect(acceptButton,&QPushButton::clicked,this,[this]() {
            const int row=table->currentRow();
            const auto *item=row<0 ? nullptr : table->item(row,0);
            if (!item || !item->isSelected()) return;
            const QString path=item->data(Qt::UserRole).toString();
            QFileInfo file(path);
            if (!file.exists() || !file.isReadable() || file.isSymLink()) {
                notice->setText("所选项目已不可用，请检查存储或重新选择。");
                notice->show(); acceptButton->setEnabled(false); return;
            }
            if (file.isDir()) { load(path); return; }
            if (!file.isFile() || !isVideo(file)) return;
            chosen=QDir::cleanPath(file.absoluteFilePath());
            accept();
        });
        load(initial);
        const QSize size=parent ? parent->window()->size() : QSize(1024,600);
        resize(qMin(960,size.width()-24),qMin(560,size.height()-24));
    }
    QString selectedFile() const { return chosen; }
private:
    DataTable *table;
    QLabel *location,*selection,*notice;
    QPushButton *acceptButton;
    QString currentDirectory,chosen;
    static bool isVideo(const QFileInfo &file) {
        return QStringList{"mp4","mkv","avi","mov","ts","webm"}.contains(file.suffix().toLower());
    }
    void load(const QString &path) {
        currentDirectory=QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        location->setText("当前位置："+currentDirectory);
        table->setRowCount(0); acceptButton->setEnabled(false); acceptButton->setText("选择视频");
        selection->setText("尚未选择视频"); notice->hide();
        findChild<QPushButton *>("pickerUp")->setEnabled(currentDirectory!="/");
        QDir directory(currentDirectory);
        if (!directory.exists() || !QFileInfo(currentDirectory).isReadable()) {
            table->setEmptyText("无法读取此文件夹。请检查存储，或选择上方的其他位置。");
            return;
        }
        table->setEmptyText("这里没有视频或子文件夹。可选择“上级文件夹”或其他存储位置。");
        int count=0;
        for (const auto &file:directory.entryInfoList(QDir::Dirs|QDir::Files|QDir::Readable|
                                  QDir::NoDotAndDotDot|QDir::NoSymLinks,QDir::DirsFirst|QDir::Name)) {
            if (!file.isDir() && !isVideo(file)) continue;
            if (count==1000) {
                notice->setText("已显示前 1000 项，请进入更具体的文件夹。"); notice->show(); break;
            }
            const int row=count++; table->insertRow(row);
            auto *name=new QTableWidgetItem(file.fileName());
            name->setData(Qt::UserRole,file.absoluteFilePath()); name->setData(Qt::UserRole+1,file.isDir());
            table->setItem(row,0,name);
            table->setItem(row,1,new QTableWidgetItem(file.isDir() ? "文件夹" : "视频"));
            table->setItem(row,2,new QTableWidgetItem(file.isDir() ? "—" :
                QString::number(file.size()/1024.0/1024.0,'f',1)+" MB"));
        }
    }
};
