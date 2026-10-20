#include "desktopkeyboard.h"
#include "../../third_party/pinyin/include/pinyinime.h"
#include <QFile>
#include <QHBoxLayout>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <memory>
namespace DesktopV1
{
namespace
{
// Main-thread singleton. No learned user dictionary or typed text is persisted.
struct PinyinDictionary
{
    bool ready = false;
    PinyinDictionary()
    {
        std::unique_ptr<QTemporaryFile> native(
            QTemporaryFile::createNativeFile(":/desktop/pinyin/dict_pinyin.dat"));
        if (native)
            ready =
                ime_pinyin::im_open_decoder(QFile::encodeName(native->fileName()).constData(), "");
        if (ready)
        {
            ime_pinyin::im_init_user_dictionary(nullptr);
            ime_pinyin::im_set_max_lens(39, 32);
        }
    }
    ~PinyinDictionary() { ime_pinyin::im_close_decoder(); }
};
QStringList decode(const QString &spelling)
{
    static PinyinDictionary dictionary;
    if (!dictionary.ready || spelling.isEmpty())
        return {};
    ime_pinyin::im_reset_search();
    const auto bytes = spelling.toLatin1();
    const auto count = ime_pinyin::im_search(bytes.constData(), size_t(bytes.size()));
    const ime_pinyin::uint16 *starts = nullptr;
    const auto syllables = ime_pinyin::im_get_spl_start_pos(starts);
    QStringList choices;
    ime_pinyin::char16 buffer[128] = {};
    for (size_t i = 0; i < count && i < 100 && choices.size() < 24; ++i)
    {
        if (!ime_pinyin::im_get_candidate(i, buffer, 128))
            continue;
        const auto choice = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer));
        // Only full-spelling candidates are committed; never silently discard a
        // suffix.
        if (size_t(choice.size()) == syllables && starts && starts[syllables] == bytes.size() &&
            !choices.contains(choice))
            choices.append(choice);
    }
    return choices;
}
} // namespace
QPushButton *DesktopKeyboard::button(const QString &text, const QString &name)
{
    auto *b = new QPushButton(text, this);
    b->setObjectName(name);
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumSize(44, 44);
    return b;
}
DesktopKeyboard::DesktopKeyboard(QWidget *parent) : QWidget(parent)
{
    setObjectName("desktopKeyboard");
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_NoMousePropagation);
    setStyleSheet("QWidget#desktopKeyboard{background:#e5edf8;border:1px solid "
                  "#c8d9ef;border-radius:16px;} QPushButton{background:white;border:1px "
                  "solid #d7e2f0;border-radius:9px;color:#142c4b;font:16px 'Dshan Sans';} "
                  "QPushButton:pressed{background:#cfe1ff;} QLabel{color:#1468fa;font:15px "
                  "'Dshan Sans';}");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    auto *candidateRow = new QHBoxLayout;
    candidateRow->setSpacing(4);
    preedit = new QLabel;
    preedit->setTextFormat(Qt::PlainText);
    preedit->setMinimumWidth(150);
    preedit->setMaximumWidth(220);
    candidateRow->addWidget(preedit, 2);
    for (int i = 0; i < 4; ++i)
    {
        auto *b = button(QString(), QString("keyboardCandidate%1").arg(i));
        candidateButtons.append(b);
        candidateRow->addWidget(b, 1);
        connect(b, &QPushButton::clicked, this, [this, i]() { choose(candidatePage * 4 + i); });
    }
    auto *previous = button("‹", "keyboardPrevious"), *next = button("›", "keyboardNext");
    previous->setMaximumWidth(48);
    next->setMaximumWidth(48);
    candidateRow->addWidget(previous);
    candidateRow->addWidget(next);
    connect(previous, &QPushButton::clicked, this, [this]() {
        candidatePage = qMax(0, candidatePage - 1);
        refresh();
    });
    connect(next, &QPushButton::clicked, this, [this]() {
        if ((candidatePage + 1) * 4 < choices.size())
            ++candidatePage;
        refresh();
    });
    layout->addLayout(candidateRow);
    for (const auto &row : QStringList{"qwertyuiop", "asdfghjkl", "zxcvbnm"})
    {
        auto *keys = new QHBoxLayout;
        keys->setSpacing(4);
        for (const auto &letter : row)
        {
            const QString text(letter);
            auto *b = button(text, "keyboardKey_" + text);
            letterButtons.append(b);
            keys->addWidget(b, 1);
            connect(b, &QPushButton::clicked, this, [this, b]() { append(b->text()); });
        }
        if (row == "zxcvbnm")
        {
            auto *back = button("⌫", "keyboardBackspace");
            keys->addWidget(back, 2);
            connect(back, &QPushButton::clicked, this, [this]() {
                if (!spelling.isEmpty())
                {
                    spelling.chop(1);
                    candidatePage = 0;
                    choices = decode(spelling);
                    refresh();
                }
                else if (target)
                    target->backspace();
            });
        }
        layout->addLayout(keys);
    }
    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(4);
    mode = button("中文拼音", "keyboardLanguage");
    bottom->addWidget(mode, 2);
    connect(mode, &QPushButton::clicked, this, [this]() {
        spelling.clear();
        choices.clear();
        chinese = !chinese;
        refresh();
    });
    auto *digits = button("123 / 字母", "keyboardNumbers");
    bottom->addWidget(digits, 2);
    connect(digits, &QPushButton::clicked, this, [this]() {
        spelling.clear();
        choices.clear();
        numbers = !numbers;
        refresh();
    });
    auto *space = button("空格 / 首选", "keyboardSpace");
    bottom->addWidget(space, 4);
    connect(space, &QPushButton::clicked, this, [this]() {
        if (!spelling.isEmpty() && !choices.isEmpty())
            choose(0);
        else if (target && spelling.isEmpty())
            target->insert(" ");
    });
    auto *literal = button("输入字母", "keyboardLiteral");
    bottom->addWidget(literal, 2);
    connect(literal, &QPushButton::clicked, this, [this]() {
        if (target && !spelling.isEmpty())
            target->insert(spelling);
        spelling.clear();
        choices.clear();
        refresh();
    });
    auto *hide = button("收起", "keyboardHide");
    bottom->addWidget(hide, 2);
    connect(hide, &QPushButton::clicked, this, &DesktopKeyboard::dismiss);
    layout->addLayout(bottom);
    refresh();
    QWidget::hide();
}
void DesktopKeyboard::showFor(QLineEdit *field)
{
    target = field;
    spelling.clear();
    choices.clear();
    candidatePage = 0;
    refresh();
    show();
    raise();
}
void DesktopKeyboard::dismiss()
{
    spelling.clear();
    choices.clear();
    target.clear();
    hide();
}
void DesktopKeyboard::escape()
{
    if (!spelling.isEmpty())
    {
        spelling.clear();
        choices.clear();
        refresh();
    }
    else
        dismiss();
}
void DesktopKeyboard::append(const QString &text)
{
    if (!target)
        return;
    if (chinese && !numbers && text.size() == 1 && text[0] >= 'a' && text[0] <= 'z')
    {
        if (spelling.size() < 39)
            spelling += text;
        candidatePage = 0;
        choices = decode(spelling);
        refresh();
    }
    else
        target->insert(text);
}
void DesktopKeyboard::choose(int index)
{
    if (target && index >= 0 && index < choices.size())
        target->insert(choices[index]);
    spelling.clear();
    choices.clear();
    candidatePage = 0;
    refresh();
}
void DesktopKeyboard::refresh()
{
    preedit->setText(spelling.isEmpty() ? (chinese ? QString("拼音输入") : QString("英文输入"))
                                        : spelling);
    mode->setText(chinese ? "中文拼音" : "英文");
    for (int i = 0; i < candidateButtons.size(); ++i)
    {
        const int index = candidatePage * 4 + i;
        candidateButtons[i]->setText(choices.value(index));
        candidateButtons[i]->setEnabled(index < choices.size());
    }
    const QString letters = "qwertyuiopasdfghjklzxcvbnm", symbols = "1234567890.-_/:@+()[]%*=,?";
    for (int i = 0; i < letterButtons.size(); ++i)
        letterButtons[i]->setText(QString(numbers ? symbols.at(i % symbols.size()) : letters[i]));
}
} // namespace DesktopV1
