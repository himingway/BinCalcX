#include "calculatorview.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Palettes — VS Code-grade. The accent is NOT here: it is base-dependent and
// passed into applyTheme() so HEX/DEC/OCT/BIN each tint the active chrome.
// ---------------------------------------------------------------------------
struct Palette {
    const char *name;
    QString window, panel, field, fieldFg;
    QString keyBg, keyHover, keyBorder, keyFg;
    QString opBg, opFg;
    QString dangerFg, dangerHover;
    QString enterBg, enterFg, enterHover;
    QString baseActiveFg;
    QString stackBg, stackZTFg, stackYFg, stackXFg;
    QString bitOffA, bitOffB, bitBorder, bitDisabled, bitDisabledBorder;
    QString gridLabel, text, subtext;
    QString toolbarBg, toolbarSep, widthCapsuleBg, widthText;
};

namespace {
const Palette kDark{
    "dark",
    "#1E1E1E", "#252526", "#252526", "#FFFFFF",           // window/panel/field/fieldFg
    "#2D2D2D", "#383838", "#3C3C3C", "#FFFFFF",           // keyBg/keyHover/keyBorder/keyFg
    "#333333", "#38BDF8",                                  // opBg/opFg  (ice-blue)
    "#F87171", "#5A3A3A",                                  // dangerFg/dangerHover
    "#059669", "#FFFFFF", "#10B981",                        // enterBg/enterFg/enterHover
    "#000000",                                              // baseActiveFg (black on accent)
    "#252526", "#858585", "#CCCCCC", "#FFFFFF",            // stackBg/ZTfg/Yfg/Xfg
    "#2A2A2E", "#323238", "#3C3C3C", "#1F1F22", "#2A2A2E", // bit*
    "#94A3B8", "#CCCCCC", "#858585",                        // gridLabel/text/subtext
    "#18181C", "#3C3C3C", "#2A2A2E", "#CCCCCC"             // toolbarBg/sep/capsule/widthText
};

const Palette kLight{
    "light",
    "#F3F3F3", "#FFFFFF", "#FFFFFF", "#1E1E1E",           // window/panel/field/fieldFg
    "#F0F0F0", "#E5E7EB", "#D1D5DB", "#1E1E1E",           // keyBg/keyHover/keyBorder/keyFg
    "#E5E7EB", "#1E40AF",                                  // opBg/opFg  (deep tech-blue)
    "#C53030", "#FEE2E2",                                  // dangerFg/dangerHover (deep brick-red)
    "#059669", "#FFFFFF", "#047857",                        // enterBg/enterFg/enterHover
    "#FFFFFF",                                              // baseActiveFg (white on accent)
    "#FFFFFF", "#6B7280", "#374151", "#1E1E1E",            // stackBg/ZTfg/Yfg/Xfg
    "#F0F0F0", "#E5E7EB", "#D1D5DB", "#F8F9FA", "#E5E7EB", // bit*
    "#9CA3AF", "#1E1E1E", "#6B7280",                        // gridLabel/text/subtext
    "#F1F5F9", "#CBD5E1", "#E2E8F0", "#64748B"             // toolbarBg/sep/capsule/widthText
};
} // namespace

QString CalculatorView::accentForBase(const QString &token, bool dark)
{
    if (dark) {
        // Dark: HEX teal-green, DEC tech-blue, OCT amber, BIN purple.
        if (token == "HEX") return "#4EC9B0";
        if (token == "DEC") return "#4FC1FF";
        if (token == "OCT") return "#D19A66";
        if (token == "BIN") return "#C586C0";
        return "#4EC9B0";
    }
    // Light: deep, high-saturation accents — crisp on white.
    if (token == "HEX") return "#0F766E";
    if (token == "DEC") return "#1D4ED8";
    if (token == "OCT") return "#B45309";
    if (token == "BIN") return "#7C3AED";
    return "#1D4ED8";
}

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
CalculatorView::CalculatorView(QWidget *parent)
    : QWidget(parent)
{
    accent_ = accentForBase("DEC", dark_);
    buildLayout();
    applyTheme(dark_, accent_);
    // Clipboard shortcuts are View-global: every QLineEdit child is filtered so
    // Ctrl+C/Ctrl+V keep working when a read-only display field holds focus.
    for (QLineEdit *le : findChildren<QLineEdit *>())
        le->installEventFilter(this);
    charDisplay_->installEventFilter(this);
    sizeDisplay_->installEventFilter(this);
}

void CalculatorView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (firstShow_) {
        firstShow_ = false;
        // The accent stylesheet is first applied during construction, before
        // this window is realised. On some platforms that first sheet doesn't
        // fully propagate to dynamically-styled children (the bit cells), so
        // the initial paint could land on a stale accent. Re-apply once we're
        // actually visible so the active base's colour (e.g. HEX green) is
        // correct from the very first frame.
        applyTheme(dark_, accent_);
    }
}

// ---------------------------------------------------------------------------
// result-push API
// ---------------------------------------------------------------------------

void CalculatorView::setHexText(const QString &text)
{
    hexDisplay_->setText(text);
    hexDisplay_->setCursorPosition(0);
}
void CalculatorView::setOctalText(const QString &text)
{
    octDisplay_->setText(text);
    octDisplay_->setCursorPosition(0);
}
void CalculatorView::setDecimalText(const QString &text)
{
    decimalDisplay_->setText(text);
    decimalDisplay_->setCursorPosition(0);
}
void CalculatorView::setBinaryText(const QString &text) { binaryText_ = text; }

QString CalculatorView::hexText() const     { return hexDisplay_->text(); }
QString CalculatorView::octalText() const   { return octDisplay_->text(); }
QString CalculatorView::decimalText() const { return decimalDisplay_->text(); }
QString CalculatorView::binaryText() const  { return binaryText_; }
void CalculatorView::setCharText(const QString &text)
{
    charText_ = text;
    renderCharDisplay();
}

void CalculatorView::renderCharDisplay()
{
    // The CHR field is always exactly 8 monospace cells. Printable bytes keep the
    // field's foreground colour; the '.' placeholders (NUL / non-printable bytes
    // and high bytes past the active width) render dimmed so real bytes read
    // clearly against the padding.
    const QString dim = dark_ ? QStringLiteral("#5A5F6A") : QStringLiteral("#9AA3B0");
    QString html;
    html.reserve(charText_.size() * 28);
    for (const QChar &ch : charText_) {
        if (ch == QLatin1Char('.'))
            html += QStringLiteral("<span style=\"color:%1;\">.</span>").arg(dim);
        else
            html += QString(ch).toHtmlEscaped();   // &, <, > are valid printable bytes
    }
    charDisplay_->setText(html);

    // Rich hover breakdown for each byte (double-click or right-click to copy).
    const quint64 x = lastBitValue_;
    QStringList parts;
    for (int b = 7; b >= 0; --b) {
        const unsigned char byte = static_cast<unsigned char>((x >> (b * 8)) & 0xFFu);
        const QChar ascii = (byte >= 0x20 && byte <= 0x7E) ? QChar(byte) : QLatin1Char('.');
        if (byte != 0 || b * 8 < activeWidth_) {
            parts << QStringLiteral("B%1: 0x%2 '%3'")
                         .arg(b)
                         .arg(byte, 2, 16, QLatin1Char('0')).toUpper()
                         .arg(ascii);
        }
    }
    charDisplay_->setToolTip(tr("8-byte ASCII (MSB to LSB) - double-click to copy\n%1")
                                 .arg(parts.join(QStringLiteral("   "))));
}

// Base-1024 place-value breakdown of a positive byte count: each non-zero
// "digit" (0..1023; the EiB place tops out at 15 for a 64-bit value) is shown
// with its IEC unit and joined by " + ". E.g. 0x120001 → "1 MiB + 128 KiB + 1 B".
static QString decomposeSize(quint64 v)
{
    static const char * const kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    QString out;
    quint64 rem = v;
    for (int u = 6; u >= 0; --u) {
        const quint64 base = 1ULL << (10 * u);   // safe: max shift is 60
        const quint64 digit = rem / base;
        rem %= base;
        if (digit == 0)
            continue;
        if (!out.isEmpty())
            out += QStringLiteral(" + ");
        out += QStringLiteral("%1 %2").arg(digit).arg(QString::fromLatin1(kUnits[u]));
    }
    return out;
}

static QString formatSiSize(quint64 v)
{
    static const char * const kSiUnits[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB"};
    if (v < 1000)
        return QStringLiteral("%1 B").arg(v);
    double val = static_cast<double>(v);
    int idx = 0;
    while (val >= 1000.0 && idx < 6) {
        val /= 1000.0;
        ++idx;
    }
    return QStringLiteral("%1 %2").arg(val, 0, 'f', val >= 100.0 ? 1 : 2).arg(QLatin1String(kSiUnits[idx]));
}

QString CalculatorView::sizeText() const
{
    // X read as a byte count → human-readable binary (IEC) size.
    const quint64 v = lastBitValue_;
    if (v == 0)
        return QStringLiteral("0 B");

    // Shorthand for "all-ones tail" masks: a value whose hex ends in a run of
    // F's (≥ 3 digits — i.e. the low 12+ bits are all 1) is one byte below a
    // round number. 0xEEFFFFFFF = 0xEF0000000 − 1 = 59 GiB + 768 MiB − 1 B, and
    // 0xFFFFFFFF = 4 GiB − 1 B. Show that round value's (strictly shorter)
    // breakdown with a " - 1 B" tail instead of the long all-1023 form. The
    // round value is a multiple of 4096, so it never carries a byte place — the
    // " - 1 B" is unambiguous. Gated on the F-run so ordinary values are untouched.
    const QString hex = QString::number(v, 16);
    int frun = 0;
    for (int i = hex.size() - 1; i >= 0 && hex.at(i) == QLatin1Char('f'); --i)
        ++frun;
    if (frun >= 3) {
        if (v == ~quint64(0))            // 2^64−1: the round value (2^64 = 16 EiB) overflows
            return QStringLiteral("16 EiB - 1 B");
        return decomposeSize(v + 1) + QStringLiteral(" - 1 B");
    }
    return decomposeSize(v);
}

void CalculatorView::renderSizeDisplay()
{
    const QString iec = sizeText();
    sizeDisplay_->setText(iec);
    sizeDisplay_->setToolTip(tr("IEC: %1  |  SI: %2  |  %3 bytes\nDouble-click to copy")
                                 .arg(iec, formatSiSize(lastBitValue_), QString::number(lastBitValue_)));
}

void CalculatorView::setStackValues(const std::array<QString, 4> &values)
{
    for (int i = 0; i < 4; ++i) {
        stackDisplays_[i]->setText(values[i]);
        stackDisplays_[i]->setCursorPosition(0);
    }
}

void CalculatorView::setStackTooltips(const std::array<QString, 4> &tips)
{
    for (int i = 0; i < 4; ++i) {
        if (stackDisplays_[i])
            stackDisplays_[i]->setToolTip(tips[i]);
    }
}

void CalculatorView::setXField(const QString &text)
{
    stackDisplays_[3]->setText(text);   // X = bottom register
    stackDisplays_[3]->setCursorPosition(0);
}

void CalculatorView::setSignedMode(bool enabled)
{
    QSignalBlocker b(signedCheckbox_);
    signedCheckbox_->setChecked(enabled);
}

void CalculatorView::setActiveBase(const QString &baseToken)
{
    activeBase_ = baseToken;
    // Accent + stylesheet FIRST, then flip checked state and re-polish the base
    // buttons/fields. Polishing against the already-updated sheet is what makes
    // the active accent (e.g. HEX green) appear on the very first paint — the
    // old order polished against the previous base's colour, so startup showed
    // the previous accent until a click forced a repaint.
    accent_ = accentForBase(baseToken, dark_);
    applyTheme(dark_, accent_);          // re-cascade accent onto bits/field/ENTER/X

    const int active = baseIndex(baseToken);
    for (int i = 0; i < 4; ++i) {
        if (!baseBtns_[i]) continue;
        baseBtns_[i]->setChecked(i == active);
        baseBtns_[i]->style()->unpolish(baseBtns_[i]);   // force :checked to re-resolve
        baseBtns_[i]->style()->polish(baseBtns_[i]);
    }
    // HEX/OCT/DEC fields (index 3, 1, 2) — BIN has no field.
    QLineEdit *fields[] = { nullptr, octDisplay_, decimalDisplay_, hexDisplay_ };
    for (int i = 1; i < 4; ++i) {
        if (!fields[i]) continue;
        fields[i]->setObjectName(i == active ? "fieldActive" : "");
        fields[i]->style()->unpolish(fields[i]);
        fields[i]->style()->polish(fields[i]);
    }
    applyDigitEnables(baseToken);
    // A bit-range selection survives a base switch (it's a range of bits, not a
    // value; the number itself is unchanged). Re-apply the [bitsel]/[kfocus]
    // styling so the highlight and focus ring render against the new accent sheet.
    repaintBitCells();
}

void CalculatorView::refreshBits(quint64 value)
{
    // Early-out when neither the bit pattern nor the active width changed —
    // avoids 64 unpolish+polish round-trips (stylesheet re-parses) on every
    // displayChanged signal.
    if (value == lastBitValue_ && activeWidth_ == lastBitWidth_)
        return;
    lastBitValue_ = value;
    lastBitWidth_ = activeWidth_;

    for (int bit = 0; bit < 64; ++bit) {
        QPushButton *btn = bitButtons_[bit];
        const char *obj;
        if (bit >= activeWidth_)                 obj = "bitDisabled";
        else if ((value >> bit) & 1ULL)          obj = "bitOn";
        else if (((bit / 4) & 1) == 0)           obj = "bitOffA";   // even nibble
        else                                     obj = "bitOffB";   // odd nibble
        btn->setObjectName(obj);
        btn->setText(QString());                 // textless cells
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }

    // The byte-size and bit-stats readouts derive from the same X value as
    // the bit grid, so they stay in lock-step.
    renderSizeDisplay();
    updateBitStats();
    if (selLo_ >= 0)
        showSelectionStatus();
}

void CalculatorView::setActiveWidth(int width)
{
    activeWidth_ = width;
    lastBitWidth_ = -1;   // invalidate refreshBits cache — width changed
    // Sync segmented-control UI. Per-register widths can be arbitrary (slice /
    // concat / replicate) — for a non-standard width, no segment is checked.
    const int active = (width == 8) ? 0 : (width == 16) ? 1
                      : (width == 32) ? 2 : (width == 64) ? 3 : -1;
    // An exclusive QButtonGroup refuses to uncheck its last checked button, so
    // temporarily relax exclusivity when entering a non-standard width (4, 12…).
    if (widthGroup_) widthGroup_->setExclusive(false);
    for (int i = 0; i < 4; ++i) {
        if (widthBtns_[i])
            widthBtns_[i]->setChecked(i == active);   // active == -1 → none checked
    }
    if (widthGroup_) widthGroup_->setExclusive(active >= 0);
    if (widthMenuBtn_) {
        const bool isCustom = (active < 0);
        widthMenuBtn_->setText(isCustom ? tr("W:%1").arg(width) : tr("W"));
        widthMenuBtn_->setProperty("custom", isCustom ? "1" : "0");
        widthMenuBtn_->style()->unpolish(widthMenuBtn_);
        widthMenuBtn_->style()->polish(widthMenuBtn_);
    }
    // Keep any active marquee selection within the valid [0..width-1] range.
    if (selLo_ >= 0 && selHi_ >= width) {
        if (selLo_ >= width) {
            clearBitSelection();
        } else {
            selHi_ = width - 1;
            selCurBit_ = qMin(selCurBit_, selHi_);
            selAnchorBit_ = qMin(selAnchorBit_, selHi_);
            repaintBitCells();
            updateSelectionControls();
            showSelectionStatus();
            emit selectionChanged(selLo_, selHi_);
        }
    }
    updateBitStats();
}

void CalculatorView::setTheme(bool dark)
{
    dark_ = dark;
    accent_ = accentForBase(activeBase_, dark_);
    applyTheme(dark_, accent_);
    QSignalBlocker b(themeBtn_);
    themeBtn_->setChecked(dark);
    themeBtn_->setText(dark ? tr("Dark") : tr("Light"));
}

void CalculatorView::setStayOnTop(bool on)
{
    // Set the flag directly here (and block the checkbox) rather than going
    // through its toggled handler. During preference restore this runs *before*
    // the first show(), so the window is born on-top — no hide/show flash at
    // startup — and we skip a redundant settings write-back via stayOnTopToggled.
    QSignalBlocker b(stayOnTopCheckbox_);
    stayOnTopCheckbox_->setChecked(on);
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
}

void CalculatorView::setHistoryEnabled(bool canUndo, bool canRedo)
{
    if (undoBtn_) undoBtn_->setEnabled(canUndo);
    if (redoBtn_) redoBtn_->setEnabled(canRedo);
}

void CalculatorView::onThemeButton(bool checked)
{
    dark_ = checked;
    themeBtn_->setText(checked ? tr("Dark") : tr("Light"));
    accent_ = accentForBase(activeBase_, dark_);
    applyTheme(dark_, accent_);
    emit themeToggled(checked);
}

// ---------------------------------------------------------------------------
// hover status + keyboard
// ---------------------------------------------------------------------------

bool CalculatorView::eventFilter(QObject *watched, QEvent *event)
{
    auto *btn = qobject_cast<QPushButton *>(watched);
    if (btn && btn->property("bit").isValid()) {
        const int bit = btn->property("bit").toInt();
        const QEvent::Type t = event->type();

        // Hover readout. Suppressed while a range selection is active so its
        // value stays visible; restored (or cleared) on leave.
        if (t == QEvent::Enter) {
            if (selLo_ < 0) {
                const quint64 place = 1ULL << bit;   // this bit's place value
                const QString inactive = (bit >= activeWidth_) ? tr(" (inactive)") : QString();
                statusLabel_->setText(tr("Bit %1 [B%2:b%3]%4 | Dec: %5 | Hex: 0x%6")
                                      .arg(bit)
                                      .arg(bit / 8)
                                      .arg(bit % 8)
                                      .arg(inactive)
                                      .arg(QString::number(place))
                                      .arg(QString::number(place, 16).toUpper()));
            }
        } else if (t == QEvent::Leave) {
            if (selLo_ >= 0) showSelectionStatus();
            else if (!statusTimer_->isActive()) statusLabel_->clear();
        }
        // Marquee range selection on the bit grid: press anchors a cell, a drag
        // that crosses into another cell extends the selection, release keeps it.
        // The press is left unconsumed so a plain click still toggles the bit;
        // only a genuine drag (cursor reaches a different cell) takes over.
        else if (t == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                suppressClick_ = false;   // fresh press → re-arm normal toggling
                focusBit_ = bit;          // a mouse click also moves the keyboard focus here
                clearBitSelection();
                selAnchorBtn_ = btn;
                selAnchorBit_ = bit;
                selCurBit_    = bit;
                bitSelecting_ = false;
            }
            // Not consumed: the button becomes the implicit mouse grabber, so
            // every move/release this press delivers routes back to it.
        } else if (t == QEvent::MouseMove && watched == selAnchorBtn_) {
            auto *me = static_cast<QMouseEvent *>(event);
            QPushButton *cur = bitButtonAt(me->globalPosition().toPoint());
            const int curBit = cur ? cur->property("bit").toInt() : -1;
            if (!bitSelecting_) {
                if (cur && curBit != selAnchorBit_) {
                    bitSelecting_ = true;   // crossed into another cell → it's a drag
                    extendBitSelection(curBit);
                }
                return false;             // still a potential click → let it through
            }
            extendBitSelection(curBit);
            return true;                  // consume moves so the anchor button stays put
        } else if (t == QEvent::MouseButtonRelease && watched == selAnchorBtn_) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                if (bitSelecting_) {
                    // Selection is non-destructive: it inspects/copies and feeds
                    // {,}/{N} as an operand slice, but does NOT change X's width.
                    // Let the release through so Qt clears its grab; flag the
                    // resulting clicked() to be ignored (no toggle).
                    suppressClick_ = true;
                    showSelectionStatus();
                    selAnchorBtn_ = nullptr;
                    bitSelecting_ = false;
                } else {
                    selAnchorBtn_ = nullptr;   // plain click → toggles normally
                }
            }
            // Not consumed
        }
    }
    // Quick double-click copy on CHR and SIZE readouts.
    if ((watched == charDisplay_ || watched == sizeDisplay_) &&
        event->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            if (watched == charDisplay_)
                copyTextWithFeedback(charText_, QStringLiteral("CHR"));
            else
                copyTextWithFeedback(sizeText(), QStringLiteral("SIZE"));
            return true;
        }
    }
    // Clipboard & keyboard forwarding when a display field holds focus.
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if ((ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) ==
                (Qt::ControlModifier | Qt::ShiftModifier) &&
            ke->key() == Qt::Key_C) {
            copyActiveValue(true);
            return true;
        }
        if (ke->matches(QKeySequence::Copy)) {
            copyActiveValue(false);
            return true;
        }
        if (ke->matches(QKeySequence::Paste)) {
            const QString clip = QGuiApplication::clipboard()->text();
            if (!clip.isEmpty())
                emit pasteRequested(clip);
            return true;
        }
        // The display fields are read-only, yet clicking one gives it focus —
        // which would otherwise deaden the whole keyboard until focus moves on
        // (every keypad button is NoFocus, so nothing else can take it). Route
        // every other key to the View's own handler; cursor/selection keys stay
        // in the field so text selection inside a display still works.
        if (qobject_cast<QLineEdit *>(watched)) {
            const int k = ke->key();
            const bool cursorKey = (k == Qt::Key_Left || k == Qt::Key_Right ||
                                    k == Qt::Key_Up || k == Qt::Key_Down ||
                                    k == Qt::Key_Home || k == Qt::Key_End ||
                                    k == Qt::Key_PageUp || k == Qt::Key_PageDown);
            if (!cursorKey && !ke->matches(QKeySequence::SelectAll)) {
                keyPressEvent(ke);
                return true;
            }
        }
    }

    // A left-click on the grid's empty area (margins, nibble gaps, inter-row
    // space — anywhere no bit cell sits) drops the current bit selection.
    if (watched == gridPanel_ && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            clearBitSelection();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CalculatorView::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const int key = event->key();

    // Clipboard & History shortcuts (checked before single-char digit dispatch).
    if ((mods & (Qt::ControlModifier | Qt::ShiftModifier)) ==
            (Qt::ControlModifier | Qt::ShiftModifier) &&
        key == Qt::Key_C) {
        copyActiveValue(true);
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copyActiveValue(false);
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        const QString clip = QGuiApplication::clipboard()->text();
        if (!clip.isEmpty())
            emit pasteRequested(clip);
        return;
    }
    if (mods & Qt::ControlModifier) {
        if (key == Qt::Key_Z) {
            emit commandPressed((mods & Qt::ShiftModifier) ? QStringLiteral("REDO")
                                                           : QStringLiteral("UNDO"));
            return;
        }
        if (key == Qt::Key_Y) { emit commandPressed(QStringLiteral("REDO")); return; }
        if (key == Qt::Key_1) { emit bitWidthRequested(8);  return; }
        if (key == Qt::Key_2) { emit bitWidthRequested(16); return; }
        if (key == Qt::Key_3) { emit bitWidthRequested(32); return; }
        if (key == Qt::Key_4) { emit bitWidthRequested(64); return; }
    }

    const QString text = event->text();
    if (!text.isEmpty() && !(mods & (Qt::ControlModifier | Qt::AltModifier))) {
        const QChar c = text.at(0).toUpper();
        const QString s(c);
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
            // Forward only digits legal for the active base and *consume* the
            // rest (return, not fall through) so an out-of-base key — e.g. BIN
            // rejecting 2–9 / A–F — is intercepted at the keyboard layer and
            // never echoed. Operators/commands below stay live in every base.
            if (isDigitValidForBase(c, activeBase_))
                emit digitPressed(s);
            return;
        }
        if (c == '&') { emit binaryOpPressed("AND"); return; }
        if (c == '|') { emit binaryOpPressed("OR");  return; }
        if (c == '^') { emit binaryOpPressed("XOR"); return; }
        if (c == '~' || c == '!') { emit commandPressed("NOT"); return; }
        if (c == '<' || c == '(') { emit binaryOpPressed("SHL"); return; }
        if (c == '>' || c == ')') { emit binaryOpPressed("SHR"); return; }
        if (c == 'R') { emit commandPressed("ROLL"); return; }
        if (c == 'S') { emit commandPressed("SWAP"); return; }
        if (c == 'N' || c == '_') { emit commandPressed("NEG"); return; }
        if (c == 'U') { signedCheckbox_->toggle(); return; }
        if (c == '[') { emit shiftRequested(true); return; }
        if (c == ']') { emit shiftRequested(false); return; }
        if (c == '{' || c == ',') {
            if (selLo_ >= 0) {
                emit sliceApplyRequested(selLo_, selHi_);
                clearBitSelection();
            }
            emit commandPressed("CONCAT");
            return;
        }
        if (c == '}') {
            if (selLo_ >= 0) {
                emit sliceApplyRequested(selLo_, selHi_);
                clearBitSelection();
            }
            emit commandPressed("REPL");
            return;
        }
        if (c == '?') { showShortcutsDialog(); return; }
    }

    const bool shiftHeld = (mods & Qt::ShiftModifier) != 0;
    switch (key) {
    // ── Bit-grid keyboard focus navigation (Shift+Arrows extend selection) ──
    case Qt::Key_Left:
        if (shiftHeld) extendKeyboardSelection(+1);
        else           moveBitFocus(+1);
        return;
    case Qt::Key_Right:
        if (shiftHeld) extendKeyboardSelection(-1);
        else           moveBitFocus(-1);
        return;
    case Qt::Key_Up:
        if (shiftHeld) extendKeyboardSelection(+16);
        else           moveBitFocus(+16);
        return;
    case Qt::Key_Down:
        if (shiftHeld) extendKeyboardSelection(-16);
        else           moveBitFocus(-16);
        return;
    case Qt::Key_Space: emit bitToggled(focusBit_); return;   // toggle the focused bit
    case Qt::Key_Plus:      emit binaryOpPressed("ADD"); return;
    case Qt::Key_Minus:     emit binaryOpPressed("SUB"); return;
    case Qt::Key_Asterisk:  emit binaryOpPressed("MUL"); return;
    case Qt::Key_Slash:     emit binaryOpPressed("DIV"); return;
    case Qt::Key_Percent:   emit binaryOpPressed("MOD"); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (shiftHeld && selLo_ >= 0) {
            emit sliceApplyRequested(selLo_, selHi_);
            clearBitSelection();
        } else if (!commitSelectionIfAny()) {
            emit commandPressed("ENTER");
        }
        return;
    case Qt::Key_Backspace: emit commandPressed("BSP"); return;
    case Qt::Key_Escape:
        // First Esc cancels an active bit-range selection; only with no
        // selection does Esc fall through to CLR (clear all).
        if (selLo_ >= 0) { clearBitSelection(); return; }
        emit commandPressed("CLR"); return;
    case Qt::Key_Delete:    emit commandPressed("CLX"); return;
    case Qt::Key_F1:        showShortcutsDialog(); return;
    case Qt::Key_F5:        emit baseSelected(QStringLiteral("HEX")); return;
    case Qt::Key_F6:        emit baseSelected(QStringLiteral("DEC")); return;
    case Qt::Key_F7:        emit baseSelected(QStringLiteral("OCT")); return;
    case Qt::Key_F8:        emit baseSelected(QStringLiteral("BIN")); return;
    default: break;
    }
    QWidget::keyPressEvent(event);
}

QString CalculatorView::bareDisplayText(QString text, const QString &baseToken)
{
    text.remove(QLatin1Char(' '));
    // HEX/BIN render width-padded with leading zeros (e.g. "0000…1234"); strip
    // them so the copied value is bare — keep one digit for zero. DEC untouched.
    if (baseToken != QLatin1String("DEC")) {
        int cut = 0;
        while (cut < text.size() - 1 && text.at(cut) == QLatin1Char('0'))
            ++cut;
        text = text.mid(cut);
    }
    return text;
}

void CalculatorView::copyTextWithFeedback(const QString &text, const QString &label)
{
    QGuiApplication::clipboard()->setText(text);
    setStatusMessage(label.isEmpty() ? tr("Copied: %1").arg(text)
                                     : tr("Copied %1: %2").arg(label, text));
}

void CalculatorView::copyActiveValue(bool withPrefix)
{
    if (selLo_ >= 0) {
        const QString text = withPrefix ? segmentVerilogLiteral() : segmentValueText(false);
        QGuiApplication::clipboard()->setText(text);
        setStatusMessage(tr("Copied bits %1..%2 = %3").arg(selHi_).arg(selLo_).arg(text));
        return;
    }
    QString text;
    if (activeBase_ == QLatin1String("BIN")) {
        text = binaryText_;
    } else {
        QLineEdit *fields[] = { nullptr, octDisplay_, decimalDisplay_, hexDisplay_ };
        QLineEdit *fld = fields[baseIndex(activeBase_)];
        text = fld ? fld->text() : decimalDisplay_->text();
    }
    text = bareDisplayText(text, activeBase_);
    if (withPrefix) {
        if (activeBase_ == QLatin1String("HEX"))      text.prepend(QStringLiteral("0x"));
        else if (activeBase_ == QLatin1String("BIN")) text.prepend(QStringLiteral("0b"));
        else if (activeBase_ == QLatin1String("OCT")) text.prepend(QStringLiteral("0o"));
    }
    copyTextWithFeedback(text);
}

void CalculatorView::setStatusMessage(const QString &msg)
{
    statusLabel_->setText(msg);
    statusTimer_->start(2200);   // auto-clear, same line the hover readout uses
}

// ---------------------------------------------------------------------------
// marquee bit-range selection
// ---------------------------------------------------------------------------
QPushButton *CalculatorView::bitButtonAt(const QPoint &globalPos) const
{
    if (!gridPanel_) return nullptr;
    QWidget *child = gridPanel_->childAt(gridPanel_->mapFromGlobal(globalPos));
    auto *btn = qobject_cast<QPushButton *>(child);
    // Skip disabled cells (past the active width) — they can't hold a value.
    return (btn && btn->isEnabled() && btn->property("bit").isValid()) ? btn : nullptr;
}

void CalculatorView::extendBitSelection(int curBit)
{
    if (curBit < 0) return;   // cursor left the grid / hit a label → hold the range
    selCurBit_ = curBit;
    selLo_ = qMin(selAnchorBit_, curBit);
    selHi_ = qMax(selAnchorBit_, curBit);
    repaintBitCells();
    updateSelectionControls();
    showSelectionStatus();
    emit selectionChanged(selLo_, selHi_);   // live-preview the slice on the X register
}

void CalculatorView::clearBitSelection()
{
    const bool had = (selLo_ >= 0);
    selLo_ = selHi_ = -1;
    selCurBit_ = selAnchorBit_ = -1;
    repaintBitCells();   // always repaint — keeps the focus ring current too
    updateSelectionControls();
    if (had && !statusTimer_->isActive()) statusLabel_->clear();
    if (had) emit selectionChanged(-1, -1);   // restore the real X register display
}

bool CalculatorView::commitSelectionIfAny()
{
    // Push the previewed slice to Y (its own width), leaving X unchanged so X
    // keeps its W-selected width. Returns false when nothing is selected, so
    // the caller (Enter) can do the normal RPN lift instead.
    if (selLo_ < 0)
        return false;
    emit slicePushRequested(selLo_, selHi_);
    clearBitSelection();
    return true;
}

void CalculatorView::repaintBitCells()
{
    // Apply the two dynamic cell properties — [bitsel] (marquee selection) and
    // [kfocus] (keyboard Focus Ring) — then force a re-style. Each layers a
    // border over the cell's value-based background, so on/off stays legible;
    // the [kfocus] rule is declared last, so a cell that is both selected and
    // focused shows the focus ring.
    for (int bit = 0; bit < 64; ++bit) {
        QPushButton *btn = bitButtons_[bit];
        if (!btn) continue;
        const bool sel = (selLo_ >= 0 && bit >= selLo_ && bit <= selHi_);
        btn->setProperty("bitsel", sel ? "1" : "0");
        btn->setProperty("kfocus", bit == focusBit_ ? "1" : "0");
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void CalculatorView::moveBitFocus(int delta)
{
    // Pure modular index arithmetic on the bit index — matches the spec's wrap
    // rules exactly (Left +1, Right -1, Up +16, Down -16; 63↔0, row edges wrap).
    focusBit_ = ((focusBit_ + delta) % 64 + 64) % 64;
    repaintBitCells();
    if (selLo_ < 0)
        showBitFocusStatus();
}

void CalculatorView::extendKeyboardSelection(int delta)
{
    if (activeWidth_ <= 0) return;
    const int maxBit = activeWidth_ - 1;
    if (selLo_ < 0)
        selAnchorBit_ = qBound(0, focusBit_, maxBit);
    const int next = qBound(0, focusBit_ + delta, maxBit);
    focusBit_ = next;
    extendBitSelection(next);
}

void CalculatorView::showBitFocusStatus()
{
    if (!statusLabel_ || (statusTimer_ && statusTimer_->isActive()))
        return;
    const quint64 place = 1ULL << focusBit_;
    const QString inactive = (focusBit_ >= activeWidth_) ? tr(" (inactive)") : QString();
    statusLabel_->setText(tr("Bit %1 [B%2:b%3]%4 | Dec: %5 | Hex: 0x%6")
                              .arg(focusBit_)
                              .arg(focusBit_ / 8)
                              .arg(focusBit_ % 8)
                              .arg(inactive)
                              .arg(QString::number(place))
                              .arg(QString::number(place, 16).toUpper()));
}

void CalculatorView::showSelectionStatus()
{
    if (selLo_ < 0) return;
    const int n = selHi_ - selLo_ + 1;
    const quint64 seg = segmentValue();
    const QString bin = QString::number(seg, 2).rightJustified(n, '0');
    statusLabel_->setText(tr("Bits %1..%2 (%3b) = %4   0x%5   0b%6")
                          .arg(selHi_).arg(selLo_).arg(n)
                          .arg(QString::number(seg))
                          .arg(QString::number(seg, 16).toUpper())
                          .arg(bin));
}

void CalculatorView::updateBitStats()
{
    if (!bitStatsLabel_) return;
    const quint64 mask = (activeWidth_ >= 64) ? ~quint64(0)
                       : (activeWidth_ <= 0)  ? quint64(0)
                                              : ((1ULL << activeWidth_) - 1);
    const quint64 v = lastBitValue_ & mask;
    int pop = 0;
    int lsb = -1;
    int msb = -1;
    for (int i = 0; i < activeWidth_; ++i) {
        if ((v >> i) & 1ULL) {
            ++pop;
            if (lsb < 0) lsb = i;
            msb = i;
        }
    }
    if (pop == 0)
        bitStatsLabel_->setText(tr("Pop:0  MSB:-  LSB:-"));
    else
        bitStatsLabel_->setText(tr("Pop:%1  MSB:%2  LSB:%3").arg(pop).arg(msb).arg(lsb));
}

void CalculatorView::updateSelectionControls()
{
    const bool hasSel = (selLo_ >= 0);
    if (gridHeaderStack_)
        gridHeaderStack_->setCurrentIndex(hasSel ? 1 : 0);
    if (selTagLabel_ && hasSel)
        selTagLabel_->setText(QStringLiteral("[%1:%2]").arg(selHi_).arg(selLo_));
    if (enterBtn_) {
        if (hasSel)
            enterBtn_->setText(tr("ENTER  %1  Push [%2:%3] %4 Y")
                                   .arg(QChar(0x00B7))
                                   .arg(selHi_).arg(selLo_)
                                   .arg(QChar(0x2192)));
        else
            enterBtn_->setText(tr("ENTER"));
    }
}

quint64 CalculatorView::segmentValue() const
{
    // The integer formed by the selected bits, shifted down to bit 0.
    if (selLo_ < 0) return 0;
    const int n = selHi_ - selLo_ + 1;
    const quint64 mask = (n >= 64) ? ~quint64(0) : ((1ULL << n) - 1);
    return (lastBitValue_ >> selLo_) & mask;
}

QString CalculatorView::segmentValueText(bool withPrefix) const
{
    // The segment's value rendered in the active base — clean (no separators),
    // and for BIN padded to the selected width so every selected bit shows.
    const quint64 seg = segmentValue();
    if (selLo_ < 0) return QString();
    if (activeBase_ == "HEX")
        return (withPrefix ? QStringLiteral("0x") : QString()) + QString::number(seg, 16).toUpper();
    if (activeBase_ == "OCT")
        return (withPrefix ? QStringLiteral("0o") : QString()) + QString::number(seg, 8);
    if (activeBase_ == "BIN")
        return (withPrefix ? QStringLiteral("0b") : QString()) +
               QString::number(seg, 2).rightJustified(selHi_ - selLo_ + 1, '0');
    return QString::number(seg);   // DEC
}

QString CalculatorView::segmentVerilogLiteral() const
{
    if (selLo_ < 0) return QString();
    const int n = selHi_ - selLo_ + 1;
    const quint64 seg = segmentValue();
    if (activeBase_ == "HEX") return QStringLiteral("%1'h%2").arg(n).arg(QString::number(seg, 16).toUpper());
    if (activeBase_ == "OCT") return QStringLiteral("%1'o%2").arg(n).arg(QString::number(seg, 8));
    if (activeBase_ == "BIN") return QStringLiteral("%1'b%2").arg(n).arg(QString::number(seg, 2).rightJustified(n, '0'));
    return QStringLiteral("%1'd%2").arg(n).arg(QString::number(seg));
}

// ---------------------------------------------------------------------------
// theme application
// ---------------------------------------------------------------------------
void CalculatorView::applyTheme(bool dark, const QString &accent)
{
    const Palette &p = dark ? kDark : kLight;
    const QString &A = accent;

    // BIN has no text field to light up (HEX/OCT/DEC each get #fieldActive),
    // so the bit-grid frame is its only base indicator — tint its border with
    // the accent there. Other bases keep the neutral keyBorder so the grid
    // doesn't compete with their lit field.
    const QString gridBorder = (activeBase_ == QLatin1String("BIN")) ? A : p.keyBorder;

    // Keyboard-focus ring on the bit grid — a fixed bright green that reads in
    // both themes and stays distinct from the base accent (lit bits) and the
    // selection border (keyFg).
    const QString focusRing = dark ? QStringLiteral("#22C55E") : QStringLiteral("#16A34A");
    const QString disabledFg = dark ? QStringLiteral("#525866") : QStringLiteral("#9CA3AF");

    // Monospace font stack — used by every input field.
    const char *mono = "'JetBrains Mono','Cascadia Code','Fira Code','Consolas','Courier New',monospace";

    // %1..%28 = palette, %29..%32 = toolbar*, %33 = accent, %34 = mono, %35 = grid border, %36 = focus ring, %37 = disabledFg.
    const QString css = QStringLiteral(
        /* ── Global ── */
        "QWidget { background: %1; color: %27; }"
        "QToolTip { background: %2; color: %27; border: 1px solid %7; padding: 4px 6px; font-family: %34; }"
        "QMenu { background: %2; color: %27; border: 1px solid %7; padding: 3px; }"
        "QMenu::item { padding: 4px 16px; border-radius: 3px; }"
        "QMenu::item:selected { background: %33; color: %16; }"
        "QMenu::separator { height: 1px; background: %7; margin: 3px 6px; }"
        /* ── Input fields (all share mono + same border model) ── */
        "QLineEdit { background: %3; color: %4; border: 1px solid %7; border-radius: 3px;"
        "  padding: 2px 5px; font-family: %34;"
        "  selection-background-color: %33; }"
        "QLineEdit#fieldActive { background: %3; color: %4; border: 1px solid %33; }"
        /* ── CHR display (label, rich text — looks like a field) ── */
        "QLabel#chrDisplay { background: %3; color: %4; border: 1px solid %7; border-radius: 3px;"
        "  padding: 2px 5px; font-family: %34; }"
        /* ── SIZE display (label — same field chrome as CHR) ── */
        "QLabel#sizeDisplay { background: %3; color: %4; border: 1px solid %7; border-radius: 3px;"
        "  padding: 2px 5px; font-family: %34; }"
        "QLineEdit#stackZT { background: %17; color: %18; border: 1px solid %7; border-radius: 3px;"
        "  padding: 2px 5px; font-family: %34; }"
        "QLineEdit#stackY { background: %17; color: %19; border: 1px solid %7; border-radius: 3px;"
        "  padding: 3px 5px; font-family: %34; }"
        "QLineEdit#stackX { background: %17; color: %20; border: 1px solid %33; border-radius: 3px;"
        "  padding: 5px 6px; font-weight: bold; font-family: %34; }"
        /* ── Digit keys ── */
        "QPushButton { background: %5; color: %8; border: 1px solid %7; border-radius: 3px; padding: 4px; }"
        "QPushButton:hover { background: %6; }"
        "QPushButton:pressed { background: %33; color: #FFFFFF; }"
        "QPushButton:disabled { color: %37; background: %24; border-color: %25; }"
        /* ── Operator keys ── */
        "QPushButton#op { background: %9; color: %10; border: 1px solid %7; }"
        "QPushButton#op:hover { background: %6; }"
        /* ── Danger keys (CLx / CLR) — same bg as digits, coral text ── */
        "QPushButton#danger { background: %5; color: %11; border: 1px solid %7; }"
        "QPushButton#danger:hover { background: %6; border: 1px solid %12; }"
        /* ── ENTER ── */
        "QPushButton#enter { background: %13; color: %14; border: 1px solid %13; font-weight: bold; }"
        "QPushButton#enter:hover { background: %15; }"
        /* ── Base selector buttons ── */
        "QPushButton#base { background: %5; color: %28; border: 1px solid %7; padding: 0px; }"
        "QPushButton#base:hover { color: %8; background: %6; }"
        "QPushButton#base:checked { background: %33; color: %16; border: 1px solid %33; }"
        /* ── ◀ ▶ in-place shift keys ── */
        "QPushButton#shiftBtn { background: transparent; border: 1px solid transparent; border-radius: 3px; color: %28; padding: 0; }"
        "QPushButton#shiftBtn:hover { color: %33; background: %5; border-color: %7; }"
        "QPushButton#shiftBtn:pressed { color: %33; }"
        /* ── Bit cells ── */
        "QPushButton#bitOffA { background: %21; border: 1px solid %23; border-radius: 2px; }"
        "QPushButton#bitOffB { background: %22; border: 1px solid %23; border-radius: 2px; }"
        "QPushButton#bitOn { background: %33; border: 1px solid %33; border-radius: 2px; }"
        "QPushButton#bitDisabled { background: %24; border: 1px solid %25; border-radius: 2px; }"
        "QPushButton#bitOffA:hover, QPushButton#bitOffB:hover { border: 1px solid %33; }"
        "QPushButton#bitOn:hover { border: 1px solid %8; }"
        /* ── Bit cells in a marquee selection — contrast border over the value bg ── */
        "QPushButton#bitOffA[bitsel=\"1\"], QPushButton#bitOffB[bitsel=\"1\"],"
        "QPushButton#bitOn[bitsel=\"1\"], QPushButton#bitDisabled[bitsel=\"1\"]"
        " { border: 2px solid %8; border-radius: 2px; }"
        /* ── Keyboard-focus ring — declared AFTER [bitsel] so it wins on a cell
         *    that is both selected and focused (equal specificity → later wins) ── */
        "QPushButton#bitOffA[kfocus=\"1\"], QPushButton#bitOffB[kfocus=\"1\"],"
        "QPushButton#bitOn[kfocus=\"1\"], QPushButton#bitDisabled[kfocus=\"1\"]"
        " { border: 2px solid %36; border-radius: 2px; }"
        /* ── Labels / check-boxes ── */
        "QLabel { color: %27; background: transparent; }"
        "QLabel#gridLabel { color: %26; }"
        "QLabel#bitStats { color: %28; font-family: %34; }"
        "QLabel#selTag { color: %33; font-weight: bold; font-family: %34; }"
        "QLabel#status { color: %28; }"
        "QCheckBox { color: %27; background: transparent; spacing: 5px; }"
        /* ── Group box ── */
        "QGroupBox { color: %28; border: 1px solid %7; border-radius: 4px; margin-top: 10px;"
        "  padding-top: 6px; font-weight: bold; background: %2; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; background: %1; }"
        /* ── Toolbar — flat color-block, no border ── */
        "QWidget#toolbar { background: %29; border: none; border-radius: 0; color: %27; }"
        "QWidget#toolbar QLabel { color: %27; }"
        "QWidget#toolbar QCheckBox { color: %27; }"
        "QWidget#gridPanel { background: %1; border: 1px solid %35; border-radius: 4px; }"
        /* ── Toolbar separator ── */
        "QFrame#toolbarSep { background: %30; border: none; max-height: 16px; }"
        /* ── Width capsule (segmented-control container) ── */
        "QFrame#widthCapsule { background: %31; border: none; border-radius: 6px; }"
        /* ── Tool buttons (generic) ── */
        "QToolButton { background: %5; color: %8; border: 1px solid %7; border-radius: 3px; padding: 3px 7px; }"
        "QToolButton:hover { background: %6; }"
        "QToolButton:checked { background: %33; color: %16; border: 1px solid %33; }"
        "QToolButton:disabled { color: %37; background: %24; border-color: %25; }"
        /* ── Top-bar buttons ── */
        "QToolButton#topBarBtn { min-height: 20px; max-height: 20px; padding: 0px 6px;"
        "  color: %8; background: %5; border: 1px solid %7; }"
        "QToolButton#topBarBtn:hover { background: %6; }"
        "QToolButton#topBarBtn:disabled { color: %37; background: %29; border-color: %25; }"
        /* ── Custom width selector button ── */
        "QToolButton#widthMenuBtn { min-height: 20px; max-height: 20px; padding: 0px 6px;"
        "  color: %27; background: transparent; border: 1px solid transparent; border-radius: 3px; font-weight: bold; }"
        "QToolButton#widthMenuBtn:hover { background: %5; border-color: %7; }"
        "QToolButton#widthMenuBtn[custom=\"1\"] { color: %33; background: %5; border-color: %33; }"
        /* ── Inline slice action pills inside bit-grid header ── */
        "QToolButton#sliceBarBtn { min-height: 18px; max-height: 18px; padding: 0px 5px;"
        "  color: %8; background: %5; border: 1px solid %7; border-radius: 3px; }"
        "QToolButton#sliceBarBtn:hover { background: %6; border-color: %33; }"
        /* ── Width segmented buttons (inside capsule) ── */
        "QToolButton#widthBtn { background: transparent; color: %32; border: none; border-radius: 4px;"
        "  min-height: 20px; max-height: 20px; padding: 0px 6px; }"
        "QToolButton#widthBtn:hover { color: %8; }"
        "QToolButton#widthBtn:checked { background: %13; color: %14; font-weight: bold; }"
    )
    .arg(p.window, p.panel, p.field, p.fieldFg, p.keyBg, p.keyHover, p.keyBorder, p.keyFg, p.opBg)         // 1..9
    .arg(p.opFg, p.dangerFg, p.dangerHover, p.enterBg, p.enterFg, p.enterHover, p.baseActiveFg)             // 10..16
    .arg(p.stackBg, p.stackZTFg, p.stackYFg, p.stackXFg)                                                    // 17..20
    .arg(p.bitOffA, p.bitOffB, p.bitBorder, p.bitDisabled, p.bitDisabledBorder, p.gridLabel, p.text, p.subtext) // 21..28
    .arg(p.toolbarBg, p.toolbarSep, p.widthCapsuleBg, p.widthText)                                           // 29..32
    .arg(A)                                                                                                    // 33
    .arg(mono)                                                                                                 // 34
    .arg(gridBorder)                                                                                          // 35
    .arg(focusRing)                                                                                            // 36
    .arg(disabledFg);                                                                                          // 37

    setStyleSheet(css);

    // Re-tint the CHR placeholder dots for the new theme (dots only — the
    // printable bytes inherit the field foreground set above).
    renderCharDisplay();
}

// ---------------------------------------------------------------------------
// DPI / scaling strategy
// ---------------------------------------------------------------------------
// Every fixed size in this UI is expressed in *logical* (device-independent)
// pixels, and every font uses point sizes. Qt6 maps logical→physical via the
// platform scale factor for us, so:
//   - a 16px bit cell is the same physical size at 100% and 200%;
//   - a 9pt glyph is the same physical size at every scale;
//   - therefore all sizes and proportions stay consistent across 1.0/1.25/1.5/2.0.
// The cardinal rule: NEVER multiply a size by devicePixelRatioF() — that
// double-scales (logical already carries the scale) and bloats the layout on
// high-DPI displays. main() sets PassThrough rounding so fractional factors are
// honoured exactly.
static constexpr int kBitCellPx = 17;   // logical px; tuned to the 9pt mono glyph
static constexpr int kNibbleGapPx = 5;  // extra gap between 4-bit nibble groups
static constexpr int kByteGapPx = 12;   // wider break between 8-bit byte lanes

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------
void CalculatorView::buildLayout()
{
    setWindowTitle(tr("BinCalcX - Programmer's Calculator"));
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setFocusPolicy(Qt::StrongFocus);

    auto root = new QVBoxLayout(this);
    root->setContentsMargins(5, 5, 5, 5);
    root->setSpacing(4);

    // --- toolbar (flat color-block, no border) ---
    auto toolbar = new QWidget(this);
    toolbar->setObjectName("toolbar");
    toolbar->setFixedHeight(30);
    auto tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(8, 2, 8, 2);
    tb->setSpacing(0);

    // [Brand] BinCalcX
    auto title = new QLabel(tr("BinCalcX"), toolbar);
    QFont tf = title->font(); tf.setBold(true); tf.setPointSize(8); title->setFont(tf);
    tb->addWidget(title);

    tb->addSpacing(10);

    // [Theme toggle] Dark / Light
    themeBtn_ = new QToolButton(toolbar);
    themeBtn_->setObjectName("topBarBtn");
    themeBtn_->setCheckable(true);
    themeBtn_->setChecked(dark_);
    themeBtn_->setFocusPolicy(Qt::NoFocus);
    themeBtn_->setText(dark_ ? tr("Dark") : tr("Light"));
    themeBtn_->setToolTip(tr("Toggle dark / light theme"));
    connect(themeBtn_, &QToolButton::toggled, this, &CalculatorView::onThemeButton);
    tb->addWidget(themeBtn_);

    // Soft separator
    auto sep = new QFrame(toolbar);
    sep->setObjectName("toolbarSep");
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    tb->addSpacing(8);
    tb->addWidget(sep);
    tb->addSpacing(8);

    // [History] Undo / Redo
    undoBtn_ = new QToolButton(toolbar);
    undoBtn_->setObjectName("topBarBtn");
    undoBtn_->setFocusPolicy(Qt::NoFocus);
    undoBtn_->setText(tr("Undo"));
    undoBtn_->setEnabled(false);
    undoBtn_->setToolTip(tr("Undo  (Ctrl+Z)"));
    connect(undoBtn_, &QToolButton::clicked, this, [this]{ emit commandPressed(QStringLiteral("UNDO")); });
    tb->addWidget(undoBtn_);
    tb->addSpacing(3);

    redoBtn_ = new QToolButton(toolbar);
    redoBtn_->setObjectName("topBarBtn");
    redoBtn_->setFocusPolicy(Qt::NoFocus);
    redoBtn_->setText(tr("Redo"));
    redoBtn_->setEnabled(false);
    redoBtn_->setToolTip(tr("Redo  (Ctrl+Y)"));
    connect(redoBtn_, &QToolButton::clicked, this, [this]{ emit commandPressed(QStringLiteral("REDO")); });
    tb->addWidget(redoBtn_);

    // → push width group to the right
    tb->addStretch(1);

    // [Width] clickable label/menu (presets + custom 1..64) + segmented capsule
    widthMenuBtn_ = new QToolButton(toolbar);
    widthMenuBtn_->setObjectName("widthMenuBtn");
    widthMenuBtn_->setFocusPolicy(Qt::NoFocus);
    widthMenuBtn_->setText(tr("W"));
    widthMenuBtn_->setToolTip(tr("Bit width (click for presets / custom 1..64)"));
    QFont wf = widthMenuBtn_->font(); wf.setBold(true); wf.setPointSize(8); widthMenuBtn_->setFont(wf);
    widthMenuBtn_->setMinimumWidth(widthMenuBtn_->fontMetrics().horizontalAdvance(QStringLiteral("W:64")) + 14);
    connect(widthMenuBtn_, &QToolButton::clicked, this, &CalculatorView::showWidthMenu);
    tb->addWidget(widthMenuBtn_);
    tb->addSpacing(4);

    // Segmented-control capsule
    auto capsule = new QFrame(toolbar);
    capsule->setObjectName("widthCapsule");
    auto capLay = new QHBoxLayout(capsule);
    capLay->setContentsMargins(3, 2, 3, 2);
    capLay->setSpacing(0);

    widthGroup_ = new QButtonGroup(this);
    widthGroup_->setExclusive(true);
    const QStringList widths = {"8", "16", "32", "64"};
    for (const auto &w : widths) {
        auto b = makeWidthButton(w, w.toInt());
        capLay->addWidget(b);
    }
    tb->addWidget(capsule);

    tb->addSpacing(12);

    // [Pin] stay-on-top — toggle via the toolbar checkbox or Ctrl+T.
    stayOnTopCheckbox_ = new QCheckBox(tr("Top"), toolbar);
    stayOnTopCheckbox_->setFocusPolicy(Qt::NoFocus);
    stayOnTopCheckbox_->setToolTip(tr("Keep window on top  (Ctrl+T)"));
    connect(stayOnTopCheckbox_, &QCheckBox::toggled, this, [this](bool on){
        // WindowStaysOnTopHint forces a native-window recreate; only re-show
        // when already visible, then reclaim the foreground so the toggle
        // neither dumps focus nor flashes.
        const bool wasVisible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        if (wasVisible) {
            show();
            raise();
            activateWindow();
        }
        emit stayOnTopToggled(on);
    });
    tb->addWidget(stayOnTopCheckbox_);
    tb->addSpacing(8);

    // [Help] Keyboard shortcuts dialog
    helpBtn_ = new QToolButton(toolbar);
    helpBtn_->setObjectName("topBarBtn");
    helpBtn_->setFocusPolicy(Qt::NoFocus);
    helpBtn_->setText(tr("?"));
    helpBtn_->setToolTip(tr("Keyboard shortcuts  (F1)"));
    connect(helpBtn_, &QToolButton::clicked, this, &CalculatorView::showShortcutsDialog);
    tb->addWidget(helpBtn_);

    auto pinShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this);
    connect(pinShortcut, &QShortcut::activated, this, [this]{
        stayOnTopCheckbox_->setChecked(!stayOnTopCheckbox_->isChecked());
    });
    root->addWidget(toolbar);

    // --- top: bit grid (left) + RPN stack (right) ---
    auto topHBox = new QHBoxLayout;
    topHBox->setSpacing(4);

    gridPanel_ = new QWidget(this);
    gridPanel_->setObjectName("gridPanel");
    gridPanel_->installEventFilter(this);   // empty-area clicks → deselect
    auto gridBox = new QVBoxLayout(gridPanel_);
    gridBox->setContentsMargins(6, 5, 6, 7);
    gridBox->setSpacing(3);

    // Header inside gridPanel_: [BIN] base selector + bit stats / slice action bar
    auto gridHeader = new QHBoxLayout;
    gridHeader->setContentsMargins(0, 0, 0, 0);
    gridHeader->setSpacing(6);

    auto binBtn = makeBaseButton(tr("BIN"), "BIN");
    binBtn->setFixedSize(38, 20);
    binBtn->setToolTip(tr("Binary input mode  (F8)"));
    gridHeader->addWidget(binBtn);

    gridHeaderStack_ = new QStackedWidget(gridPanel_);
    gridHeaderStack_->setFixedHeight(20);

    bitStatsLabel_ = new QLabel(tr("Pop:0  MSB:-  LSB:-"), gridHeaderStack_);
    bitStatsLabel_->setObjectName("bitStats");
    QFont bsf = bitStatsLabel_->font(); bsf.setPointSize(8); bitStatsLabel_->setFont(bsf);
    bitStatsLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    gridHeaderStack_->addWidget(bitStatsLabel_);

    auto selBar = new QWidget(gridHeaderStack_);
    auto selLay = new QHBoxLayout(selBar);
    selLay->setContentsMargins(0, 0, 0, 0);
    selLay->setSpacing(2);
    selLay->addStretch(1);

    selTagLabel_ = new QLabel(selBar);
    selTagLabel_->setObjectName("selTag");
    selTagLabel_->setFont(bsf);
    selLay->addWidget(selTagLabel_);
    selLay->addSpacing(3);

    auto extBtn = makeSliceActionBtn(tr("Ext"), tr("Extract [hi:lo] into X and set width  (Shift+Enter)"));
    connect(extBtn, &QToolButton::clicked, this, [this]{
        if (selLo_ >= 0) {
            emit sliceApplyRequested(selLo_, selHi_);
            clearBitSelection();
        }
    });
    auto set1Btn = makeSliceActionBtn(tr("1s"), tr("Set selected bits [hi:lo] to 1"));
    connect(set1Btn, &QToolButton::clicked, this, [this]{
        if (selLo_ >= 0) emit sliceSetRequested(selLo_, selHi_);
    });
    auto clr0Btn = makeSliceActionBtn(tr("0s"), tr("Clear selected bits [hi:lo] to 0"));
    connect(clr0Btn, &QToolButton::clicked, this, [this]{
        if (selLo_ >= 0) emit sliceClearRequested(selLo_, selHi_);
    });
    auto invBtn = makeSliceActionBtn(tr("~"), tr("Invert selected bits [hi:lo]"));
    connect(invBtn, &QToolButton::clicked, this, [this]{
        if (selLo_ >= 0) emit sliceInvertRequested(selLo_, selHi_);
    });
    auto mskBtn = makeSliceActionBtn(tr("Msk"), tr("Replace X with bitmask for [hi:lo]"));
    connect(mskBtn, &QToolButton::clicked, this, [this]{
        if (selLo_ >= 0) {
            emit sliceMaskRequested(selLo_, selHi_);
            clearBitSelection();
        }
    });
    selLay->addWidget(extBtn);
    selLay->addWidget(set1Btn);
    selLay->addWidget(clr0Btn);
    selLay->addWidget(invBtn);
    selLay->addWidget(mskBtn);
    gridHeaderStack_->addWidget(selBar);

    gridHeader->addWidget(gridHeaderStack_, 1);
    gridBox->addLayout(gridHeader);

    auto grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(1);

    for (int row = 0; row < 4; ++row) {
        const int high = 63 - 16 * row;          // row's top bit
        const int rowOffset = (row >= 2) ? 1 : 0; // spacer row between 63..32 and 31..0
        const int headerRow = row * 2 + rowOffset;
        const int dataRow   = headerRow + 1;

        for (int g = 0; g < 4; ++g) {            // 4 nibbles per row
            const int nibHigh = high - g * 4;
            const int nibLow  = nibHigh - 3;

            // Left boundary label (high bit of nibble)
            auto hdrL = new QLabel(QString::number(nibHigh), gridPanel_);
            hdrL->setObjectName("gridLabel");
            QFont hf = hdrL->font(); hf.setPointSize(7); hdrL->setFont(hf);
            hdrL->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            grid->addWidget(hdrL, headerRow, g * 5);

            // Right boundary label (low bit of nibble)
            auto hdrR = new QLabel(QString::number(nibLow), gridPanel_);
            hdrR->setObjectName("gridLabel");
            hdrR->setFont(hf);
            hdrR->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            grid->addWidget(hdrR, headerRow, g * 5 + 3);

            for (int b = 0; b < 4; ++b) {
                const int bit = nibHigh - b;
                auto btn = new QPushButton(gridPanel_);
                btn->setFixedSize(kBitCellPx, kBitCellPx);
                btn->setFocusPolicy(Qt::NoFocus);
                btn->setText(QString());
                btn->setProperty("bit", bit);
                btn->installEventFilter(this);
                connect(btn, &QPushButton::clicked, this, [this, bit]{
                    if (suppressClick_) { suppressClick_ = false; return; }  // drag-select
                    emit bitToggled(bit);
                });
                bitButtons_[bit] = btn;
                grid->addWidget(btn, dataRow, g * 5 + b);
            }
            // Nibble / Byte group gap: wider break at g==1 separates bytes.
            if (g < 3) {
                const int gap = (g == 1) ? kByteGapPx : kNibbleGapPx;
                grid->addItem(new QSpacerItem(gap, 0,
                                              QSizePolicy::Fixed, QSizePolicy::Minimum),
                              dataRow, g * 5 + 4);
            }
        }
    }
    grid->addItem(new QSpacerItem(0, 3, QSizePolicy::Minimum, QSizePolicy::Fixed), 4, 0);
    gridBox->addLayout(grid);
    topHBox->addWidget(gridPanel_);

    // RPN stack (right, expanded)
    auto rpnGroup = new QGroupBox(tr("RPN STACK"), this);
    auto rpnGrid = new QGridLayout(rpnGroup);
    rpnGrid->setContentsMargins(6, 12, 6, 6);
    rpnGrid->setVerticalSpacing(4);
    rpnGrid->setColumnStretch(1, 1);
    const QString labels[4] = {tr("T"), tr("Z"), tr("Y"), tr("X")};
    const char *objName[4]  = {"stackZT", "stackZT", "stackY", "stackX"};
    for (int i = 0; i < 4; ++i) {
        auto lbl = new QLabel(labels[i], rpnGroup);
        lbl->setMinimumWidth(14);
        lbl->setAlignment(Qt::AlignCenter);
        QFont lf = lbl->font(); lf.setBold(true);
        if (i >= 2) lf.setPointSize(10);
        lbl->setFont(lf);
        stackDisplays_[i] = new QLineEdit(rpnGroup);
        stackDisplays_[i]->setReadOnly(true);
        stackDisplays_[i]->setObjectName(objName[i]);
        stackDisplays_[i]->setContextMenuPolicy(Qt::CustomContextMenu);
        stackDisplays_[i]->setProperty("stackRow", i);
        connect(stackDisplays_[i], &QWidget::customContextMenuRequested, this,
                [this, w = stackDisplays_[i]](const QPoint &pos){ showFieldContextMenu(w, pos); });
        if (i == 3) stackDisplays_[i]->setMinimumHeight(30);
        rpnGrid->addWidget(lbl, i, 0);
        rpnGrid->addWidget(stackDisplays_[i], i, 1);
    }
    const int minStackFieldW =
        stackDisplays_[3]->fontMetrics().horizontalAdvance(QStringLiteral("64'd18446744073709551615")) + 22;
    stackDisplays_[3]->setMinimumWidth(minStackFieldW);
    topHBox->addWidget(rpnGroup, 1);
    root->addLayout(topHBox);

    // --- base rows (short fields, 2 compact rows + SIZE) ---
    auto baseRows = new QVBoxLayout;
    baseRows->setSpacing(3);

    const int baseFieldH = fontMetrics().height() + 8;

    auto row1 = new QHBoxLayout;
    row1->setSpacing(4);
    auto hexBtn = makeBaseButton(tr("HEX"), "HEX");
    hexBtn->setToolTip(tr("Hexadecimal input mode  (F5)"));
    row1->addWidget(hexBtn);
    hexDisplay_ = new QLineEdit(this); hexDisplay_->setReadOnly(true);
    hexDisplay_->setFixedHeight(baseFieldH);
    hexDisplay_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hexDisplay_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){ showFieldContextMenu(hexDisplay_, pos); });
    row1->addWidget(hexDisplay_, 5);

    auto octBtn = makeBaseButton(tr("OCT"), "OCT");
    octBtn->setToolTip(tr("Octal input mode  (F7)"));
    row1->addWidget(octBtn);
    octDisplay_ = new QLineEdit(this); octDisplay_->setReadOnly(true);
    octDisplay_->setFixedHeight(baseFieldH);
    octDisplay_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(octDisplay_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){ showFieldContextMenu(octDisplay_, pos); });
    row1->addWidget(octDisplay_, 6);
    baseRows->addLayout(row1);

    //  [DEC] [DEC field] [✓ Signed] [◀] [▶] [CHR] [CHR field]
    auto row2 = new QHBoxLayout;
    row2->setSpacing(4);
    auto decBtn = makeBaseButton(tr("DEC"), "DEC");
    decBtn->setToolTip(tr("Decimal input mode  (F6)"));
    row2->addWidget(decBtn);
    decimalDisplay_ = new QLineEdit(this); decimalDisplay_->setReadOnly(true);
    decimalDisplay_->setFixedHeight(baseFieldH);
    decimalDisplay_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(decimalDisplay_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){ showFieldContextMenu(decimalDisplay_, pos); });
    row2->addWidget(decimalDisplay_, 2);
    signedCheckbox_ = new QCheckBox(tr("Signed"), this);
    signedCheckbox_->setChecked(false);
    signedCheckbox_->setFocusPolicy(Qt::NoFocus);
    signedCheckbox_->setToolTip(tr("Toggle signed / two's-complement decimal view  (U)"));
    connect(signedCheckbox_, &QCheckBox::toggled, this, &CalculatorView::signedModeChanged);
    row2->addWidget(signedCheckbox_);

    shiftLeftBtn_  = makeShiftButton(QString(QChar(0x25C0)), true);   // ◀
    shiftRightBtn_ = makeShiftButton(QString(QChar(0x25B6)), false);  // ▶
    shiftLeftBtn_->setFixedHeight(baseFieldH);
    shiftRightBtn_->setFixedHeight(baseFieldH);
    row2->addWidget(shiftLeftBtn_);
    row2->addWidget(shiftRightBtn_);

    auto charLabel = new QLabel(tr("CHR"), this);
    charLabel->setMinimumWidth(28); charLabel->setAlignment(Qt::AlignCenter);
    QFont clf = charLabel->font(); clf.setBold(true); charLabel->setFont(clf);
    row2->addWidget(charLabel);
    charDisplay_ = new QLabel(this);                       // rich text → dimmed dots
    charDisplay_->setObjectName("chrDisplay");
    charDisplay_->setTextFormat(Qt::RichText);
    charDisplay_->setAlignment(Qt::AlignCenter);
    charDisplay_->setTextInteractionFlags(Qt::NoTextInteraction);
    charDisplay_->setFixedHeight(baseFieldH);
    charDisplay_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(charDisplay_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){ showFieldContextMenu(charDisplay_, pos); });
    const int chrW = charDisplay_->fontMetrics().horizontalAdvance(QStringLiteral("DEADBEEF")) + 20;
    charDisplay_->setFixedWidth(chrW);
    row2->addWidget(charDisplay_);
    baseRows->addLayout(row2);

    //  [SIZE] [X read as a byte count — human-readable binary units]
    auto row3 = new QHBoxLayout;
    row3->setSpacing(4);
    auto sizeLabel = new QLabel(tr("SIZE"), this);
    sizeLabel->setMinimumWidth(36);
    sizeLabel->setAlignment(Qt::AlignCenter);
    QFont zf = sizeLabel->font(); zf.setBold(true); sizeLabel->setFont(zf);
    row3->addWidget(sizeLabel);
    sizeDisplay_ = new QLabel(this);
    sizeDisplay_->setObjectName("sizeDisplay");
    sizeDisplay_->setAlignment(Qt::AlignCenter);
    sizeDisplay_->setTextInteractionFlags(Qt::NoTextInteraction);
    sizeDisplay_->setToolTip(tr("X read as a byte count, in binary units "
                                "(KiB = 1024, MiB, GiB, …)."));
    sizeDisplay_->setMinimumHeight(baseFieldH);
    sizeDisplay_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sizeDisplay_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){ showFieldContextMenu(sizeDisplay_, pos); });
    row3->addWidget(sizeDisplay_, 1);
    baseRows->addLayout(row3);
    root->addLayout(baseRows);

    // --- controls: keypad (4x4) + ops (6x4) side by side ---
    auto controlsHBox = new QHBoxLayout;
    controlsHBox->setSpacing(4);

    auto keyGrid = new QGridLayout;
    keyGrid->setHorizontalSpacing(3); keyGrid->setVerticalSpacing(3);
    const QString keys[4][4] = {
        {"7", "8", "9", "F"}, {"4", "5", "6", "E"},
        {"1", "2", "3", "D"}, {"0", "A", "B", "C"}
    };
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            keyGrid->addWidget(makeDigit(keys[r][c]), r, c);
    controlsHBox->addLayout(keyGrid);

    auto opGrid = new QGridLayout;
    opGrid->setHorizontalSpacing(3); opGrid->setVerticalSpacing(3);
    struct D { int r, c; const char *label; const char *token; bool isOp; bool danger; const char *tip; };
    const D defs[] = {
        {0,0,"+","ADD",1,0,"Add  Y + X   (+)"},
        {0,1,"-","SUB",1,0,"Subtract  Y - X   (-)"},
        {0,2,"*","MUL",1,0,"Multiply  Y * X   (*)"},
        {0,3,"/","DIV",1,0,"Divide  Y / X   (/)"},
        {0,4,"Mod","MOD",1,0,"Modulo  Y % X   (%)"},
        {0,5,"Rol","ROL",1,0,"Rotate Y left by X bits within Y's width"},
        {1,0,"And","AND",1,0,"Bitwise AND  Y & X   (&)"},
        {1,1,"Or","OR",1,0,"Bitwise OR  Y | X   (|)"},
        {1,2,"Xor","XOR",1,0,"Bitwise XOR  Y ^ X   (^)"},
        {1,3,"<<","SHL",1,0,"Shift Y left by X bits   (< or ()"},
        {1,4,">>","SHR",1,0,"Logical shift Y right by X bits   (> or ))"},
        {1,5,"Ror","ROR",1,0,"Rotate Y right by X bits within Y's width"},
        {2,0,"+/-","NEG",0,0,"Two's-complement negate X   (N or _)"},
        {2,1,"Not","NOT",0,0,"Bitwise NOT X   (~ or !)"},
        {2,2,"Rv","ROLL",0,0,"Roll stack down   (R)"},
        {2,3,"X<>Y","SWAP",0,0,"Swap X and Y   (S)"},
        {2,4,"Bsp","BSP",0,0,"Backspace last digit   (Backspace)"},
        {2,5,">>>","ASR",1,0,"Arithmetic shift Y right by X bits (sign-extended)"},
        {3,0,"CLx","CLX",0,1,"Clear X register   (Delete)"},
        {3,1,"CLR","CLR",0,1,"Clear entire stack   (Esc)"},
        {3,4,"Bswap","BSWAP",0,0,"Byte-swap X within its width (endian flip)"},
    };
    for (const D &d : defs) {
        QPushButton *b;
        if (d.isOp)
            b = makeOp(d.label, d.token, tr(d.tip));
        else
            b = makeCommand(d.label, d.token, d.danger ? "danger" : nullptr, tr(d.tip));
        opGrid->addWidget(b, d.r, d.c);
    }
    auto sliceOpBtn = [this](const QString &label, const QString &token, const QString &tip) {
        auto b = new QPushButton(label, this);
        b->setMinimumHeight(31); b->setMinimumWidth(44);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(tip);
        connect(b, &QPushButton::clicked, this, [this, token]{
            if (selLo_ >= 0) {
                emit sliceApplyRequested(selLo_, selHi_);   // X ← selected slice
                clearBitSelection();
            }
            emit commandPressed(token);
        });
        return b;
    };
    opGrid->addWidget(sliceOpBtn(tr("{,}"), QStringLiteral("CONCAT"),
                                 tr("Concatenate {Y,X} — a selected slice becomes X   ({ or ,)")), 3, 2);
    opGrid->addWidget(sliceOpBtn(tr("{N}"), QStringLiteral("REPL"),
                                 tr("Replicate {N{X}} — a selected slice becomes X   (})")), 3, 3);

    auto maskBtn = new QPushButton(tr("Mask"), this);
    maskBtn->setMinimumHeight(31); maskBtn->setMinimumWidth(44);
    maskBtn->setFocusPolicy(Qt::NoFocus);
    maskBtn->setToolTip(tr("Generate low X-bit mask (1<<X)-1, or mask of selected [hi:lo] bits"));
    connect(maskBtn, &QPushButton::clicked, this, [this]{
        if (selLo_ >= 0) {
            emit sliceMaskRequested(selLo_, selHi_);
            clearBitSelection();
        } else {
            emit commandPressed(QStringLiteral("MASK"));
        }
    });
    opGrid->addWidget(maskBtn, 3, 5);

    controlsHBox->addLayout(opGrid, 1);
    root->addLayout(controlsHBox);

    // --- full-width ENTER ---
    // A selected slice is pushed to Y (X keeps its width); else normal lift.
    enterBtn_ = new QPushButton(tr("ENTER"), this);
    enterBtn_->setObjectName("enter");
    enterBtn_->setMinimumHeight(34);
    enterBtn_->setMinimumWidth(34);
    enterBtn_->setFocusPolicy(Qt::NoFocus);
    connect(enterBtn_, &QPushButton::clicked, this, [this]{
        if (!commitSelectionIfAny())
            emit commandPressed("ENTER");
    });
    root->addWidget(enterBtn_);

    // --- status line (hover readout) ---
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("status");
    QFont sf = statusLabel_->font(); sf.setPointSize(8); statusLabel_->setFont(sf);
    statusLabel_->setMinimumHeight(16);
    root->addWidget(statusLabel_);

    // Auto-clears the status line (used by hover readout + clipboard feedback).
    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    connect(statusTimer_, &QTimer::timeout, statusLabel_, &QLabel::clear);

    // compact, capped window — lock to content after layout settles
    setActiveBase("DEC");
    refreshBits(0);
    setFocus();
}

// ---------------------------------------------------------------------------
// dialogs & context menus
// ---------------------------------------------------------------------------
void CalculatorView::showWidthMenu()
{
    QMenu menu(this);
    const int presets[] = {4, 8, 12, 16, 24, 32, 48, 64};
    for (int w : presets) {
        const QString lbl = (w == activeWidth_)
                                ? QStringLiteral("%1  %2 bits").arg(QChar(0x2022)).arg(w)
                                : QStringLiteral("   %1 bits").arg(w);
        QAction *act = menu.addAction(lbl);
        connect(act, &QAction::triggered, this, [this, w]{
            if (w == 8 || w == 16 || w == 32 || w == 64)
                emit bitWidthRequested(w);
            else
                emit customBitWidthRequested(w);
        });
    }
    menu.addSeparator();
    QAction *customAct = menu.addAction(tr("Custom width (1-64)..."));
    connect(customAct, &QAction::triggered, this, [this]{
        bool ok = false;
        const int w = QInputDialog::getInt(this, tr("Custom Bit Width"),
                                           tr("Bit width (1 - 64):"),
                                           activeWidth_, 1, 64, 1, &ok);
        if (ok) {
            if (w == 8 || w == 16 || w == 32 || w == 64)
                emit bitWidthRequested(w);
            else
                emit customBitWidthRequested(w);
        }
    });
    menu.exec(widthMenuBtn_->mapToGlobal(QPoint(0, widthMenuBtn_->height())));
}

void CalculatorView::showFieldContextMenu(QWidget *target, const QPoint &pos)
{
    QMenu menu(this);
    if (target == hexDisplay_ || target == octDisplay_ || target == decimalDisplay_) {
        const QString base = (target == hexDisplay_)     ? QStringLiteral("HEX")
                           : (target == octDisplay_)     ? QStringLiteral("OCT")
                                                         : QStringLiteral("DEC");
        const QString rawText = static_cast<QLineEdit *>(target)->text();
        const QString bare = bareDisplayText(rawText, base);
        connect(menu.addAction(tr("Copy %1 value").arg(base)), &QAction::triggered, this, [this, bare, base]{
            copyTextWithFeedback(bare, base);
        });
        if (base != QLatin1String("DEC")) {
            const QString prefix = (base == QLatin1String("HEX")) ? QStringLiteral("0x")
                                                                  : QStringLiteral("0o");
            connect(menu.addAction(tr("Copy %1%2").arg(prefix, bare)), &QAction::triggered,
                    this, [this, val = prefix + bare, base]{
                copyTextWithFeedback(val, base);
            });
        }
        if (activeBase_ != base) {
            connect(menu.addAction(tr("Switch active base to %1").arg(base)), &QAction::triggered,
                    this, [this, base]{ emit baseSelected(base); });
        }
        const QString clip = QGuiApplication::clipboard()->text();
        if (!clip.isEmpty()) {
            menu.addSeparator();
            connect(menu.addAction(tr("Paste into X")), &QAction::triggered,
                    this, [this, clip]{ emit pasteRequested(clip); });
        }
    } else if (target == charDisplay_) {
        connect(menu.addAction(tr("Copy ASCII (%1)").arg(charText_)), &QAction::triggered,
                this, [this]{ copyTextWithFeedback(charText_, QStringLiteral("CHR")); });
    } else if (target == sizeDisplay_) {
        const QString sz = sizeText();
        connect(menu.addAction(tr("Copy size (%1)").arg(sz)), &QAction::triggered,
                this, [this, sz]{ copyTextWithFeedback(sz, QStringLiteral("SIZE")); });
        connect(menu.addAction(tr("Copy byte count (%1)").arg(lastBitValue_)), &QAction::triggered,
                this, [this]{ copyTextWithFeedback(QString::number(lastBitValue_), QStringLiteral("Bytes")); });
    } else if (target && target->property("stackRow").isValid()) {
        const int row = target->property("stackRow").toInt();   // 0=T,1=Z,2=Y,3=X
        const int modelIdx = 3 - row;                           // 3=T,2=Z,1=Y,0=X
        const QString lit = static_cast<QLineEdit *>(target)->text();
        connect(menu.addAction(tr("Copy literal (%1)").arg(lit)), &QAction::triggered,
                this, [this, lit]{ copyTextWithFeedback(lit); });
        if (modelIdx >= 1) {
            connect(menu.addAction(tr("Recall to X")), &QAction::triggered,
                    this, [this, modelIdx]{ emit stackRecallRequested(modelIdx); });
        }
        if (modelIdx == 1) {
            connect(menu.addAction(tr("Swap X <-> Y")), &QAction::triggered,
                    this, [this]{ emit commandPressed(QStringLiteral("SWAP")); });
        }
        if (modelIdx == 0) {
            const QString clip = QGuiApplication::clipboard()->text();
            if (!clip.isEmpty()) {
                menu.addSeparator();
                connect(menu.addAction(tr("Paste into X")), &QAction::triggered,
                        this, [this, clip]{ emit pasteRequested(clip); });
            }
        }
    }
    if (!menu.actions().isEmpty())
        menu.exec(target->mapToGlobal(pos));
}

void CalculatorView::showShortcutsDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("BinCalcX Shortcuts"));
    dlg.setStyleSheet(styleSheet());

    auto lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(10);

    auto grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);

    struct Row { bool isHeader; QString key; QString desc; };
    const Row rows[] = {
        {true,  tr("Entry & Bases"), {}},
        {false, QStringLiteral("0-9, A-F"),           tr("Enter digit (current base)")},
        {false, QStringLiteral("F5 / F6 / F7 / F8"),  tr("Switch base: HEX / DEC / OCT / BIN")},
        {false, QStringLiteral("Ctrl+1 / 2 / 3 / 4"), tr("Set bit width: 8 / 16 / 32 / 64")},
        {false, QStringLiteral("U"),                  tr("Toggle Signed view")},

        {true,  tr("Arithmetic & Bitwise"), {}},
        {false, QStringLiteral("+ - * / %"),          tr("Add / Sub / Mul / Div / Mod")},
        {false, QStringLiteral("& | ^ ~"),            tr("Bitwise AND / OR / XOR / NOT")},
        {false, QStringLiteral("<  >"),               tr("Shift Y left / right by X")},
        {false, QStringLiteral("[  ]"),               tr("In-place shift X left / right by 1")},
        {false, QStringLiteral("{ or ,  /  }"),       tr("Concat {Y,X} / Replicate {N{X}}")},

        {true,  tr("Stack & Bit Grid"), {}},
        {false, QStringLiteral("Enter"),              tr("Push X (or selected slice) to Y")},
        {false, QStringLiteral("Shift+Enter"),        tr("Extract selected [hi:lo] into X")},
        {false, QStringLiteral("R / S / N"),          tr("Roll / Swap X<->Y / Negate")},
        {false, QStringLiteral("Del / Esc"),          tr("Clear X / Cancel sel or Clear all")},
        {false, QStringLiteral("Arrows / Space"),     tr("Navigate bits / Toggle focused bit")},
        {false, QStringLiteral("Shift+Arrows"),       tr("Select bit range")},

        {true,  tr("Clipboard, History & Window"), {}},
        {false, QStringLiteral("Ctrl+C / Ctrl+Shift+C"), tr("Copy bare / Prefixed (0x, 0b, SV)")},
        {false, QStringLiteral("Ctrl+V"),             tr("Paste (0x/0b/0o/Verilog literals)")},
        {false, QStringLiteral("Ctrl+Z / Ctrl+Y"),    tr("Undo / Redo")},
        {false, QStringLiteral("Ctrl+T"),             tr("Toggle stay-on-top")},
        {false, QStringLiteral("F1 / ?"),             tr("Show this shortcuts reference")},
    };

    int r = 0;
    for (const Row &item : rows) {
        if (item.isHeader) {
            if (r > 0)
                grid->setRowMinimumHeight(r++, 6);
            auto hdr = new QLabel(item.key, &dlg);
            QFont hf = hdr->font();
            hf.setBold(true);
            hf.setPointSize(9);
            hdr->setFont(hf);
            hdr->setStyleSheet(QStringLiteral("color: %1;").arg(accent_));
            grid->addWidget(hdr, r++, 0, 1, 2);
        } else {
            auto kLbl = new QLabel(item.key, &dlg);
            QFont kf = kLbl->font();
            kf.setBold(true);
            kf.setPointSize(9);
            kLbl->setFont(kf);
            kLbl->setContentsMargins(10, 0, 0, 0);
            kLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            auto dLbl = new QLabel(item.desc, &dlg);
            QFont df = dLbl->font();
            df.setPointSize(9);
            dLbl->setFont(df);
            dLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            grid->addWidget(kLbl, r, 0);
            grid->addWidget(dLbl, r, 1);
            ++r;
        }
    }
    lay->addLayout(grid);

    auto closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto closeBtn = new QPushButton(tr("Close"), &dlg);
    closeBtn->setMinimumWidth(72);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    closeRow->addWidget(closeBtn);
    lay->addLayout(closeRow);

    dlg.adjustSize();
    dlg.setFixedSize(dlg.sizeHint());
    dlg.exec();
}

// ---------------------------------------------------------------------------
// factories
// ---------------------------------------------------------------------------
QPushButton *CalculatorView::makeDigit(const QString &text)
{
    auto b = new QPushButton(text, this);
    b->setFixedSize(40, 31);
    b->setFocusPolicy(Qt::NoFocus);
    QFont f = b->font(); f.setBold(true); f.setPointSize(10); b->setFont(f);
    connect(b, &QPushButton::clicked, this, [this, text]{ emit digitPressed(text); });
    // '0'–'9' → index 0–9,  'A'–'F' → index 10–15
    const QChar c = text.at(0);
    const int idx = (c >= 'A') ? (10 + c.unicode() - 'A') : (c.unicode() - '0');
    digitBtns_[idx] = b;
    return b;
}

QPushButton *CalculatorView::makeOp(const QString &label, const QString &opToken, const QString &tip)
{
    auto b = new QPushButton(label, this);
    b->setMinimumHeight(31); b->setMinimumWidth(44);
    b->setFocusPolicy(Qt::NoFocus);
    b->setObjectName("op");
    if (!tip.isEmpty()) b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [this, opToken]{ emit binaryOpPressed(opToken); });
    return b;
}

QPushButton *CalculatorView::makeCommand(const QString &label, const QString &cmdToken,
                                         const char *objectName, const QString &tip)
{
    auto b = new QPushButton(label, this);
    b->setMinimumHeight(31); b->setMinimumWidth(44);
    b->setFocusPolicy(Qt::NoFocus);
    if (objectName) b->setObjectName(QString::fromLatin1(objectName));
    if (!tip.isEmpty()) b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [this, cmdToken]{ emit commandPressed(cmdToken); });
    return b;
}

QPushButton *CalculatorView::makeBaseButton(const QString &label, const QString &token)
{
    auto b = new QPushButton(label, this);
    b->setObjectName("base");
    b->setCheckable(true);
    b->setFixedWidth(38);
    b->setFixedHeight(fontMetrics().height() + 8);   // match the input-field row height
    b->setFocusPolicy(Qt::NoFocus);
    QFont f = b->font(); f.setBold(true); f.setPointSize(8); b->setFont(f);
    connect(b, &QPushButton::clicked, this, [this, token]{ emit baseSelected(token); });
    baseBtns_[baseIndex(token)] = b;
    return b;
}

QPushButton *CalculatorView::makeShiftButton(const QString &glyph, bool left)
{
    auto b = new QPushButton(glyph, this);
    b->setObjectName("shiftBtn");
    b->setFixedWidth(20);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolTip(left ? tr("Shift X left by 1   (X = X << 1)   ([)")
                       : tr("Shift X right by 1   (X = X >> 1, sign-extends if Signed)   (])"));
    connect(b, &QPushButton::clicked, this, [this, left]{ emit shiftRequested(left); });
    return b;
}

QToolButton *CalculatorView::makeWidthButton(const QString &label, int width)
{
    auto b = new QToolButton(this);
    b->setObjectName("widthBtn");
    b->setText(label);
    b->setCheckable(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolTip(tr("Bit width: %1").arg(width));
    connect(b, &QToolButton::clicked, this, [this, width]{ emit bitWidthRequested(width); });
    widthGroup_->addButton(b);
    // 8→0  16→1  32→2  64→3
    const int idx = (width == 8) ? 0 : (width == 16) ? 1 : (width == 32) ? 2 : 3;
    widthBtns_[idx] = b;
    return b;
}

QToolButton *CalculatorView::makeSliceActionBtn(const QString &label, const QString &tip)
{
    auto b = new QToolButton(this);
    b->setObjectName("sliceBarBtn");
    b->setText(label);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolTip(tip);
    QFont f = b->font(); f.setPointSize(8); b->setFont(f);
    return b;
}

int CalculatorView::baseIndex(const QString &token)
{
    // Matches CalculatorModel::Base enum: Binary=0 Octal=1 Decimal=2 Hexadecimal=3
    if (token == "OCT") return 1;
    if (token == "DEC") return 2;
    if (token == "HEX") return 3;
    Q_ASSERT(token == "BIN");
    return 0;
}

bool CalculatorView::isDigitValidForBase(QChar c, const QString &baseToken)
{
    // Mirrors CalculatorModel::isValidDigit — kept in the View so the keyboard
    // handler can gate keys without importing the Model. 'c' is already an
    // upper-cased hex digit (0–9 or A–F) at the one call site.
    c = c.toUpper();
    if (baseToken == "BIN") return c == '0' || c == '1';
    if (baseToken == "OCT") return c >= '0' && c <= '7';
    if (baseToken == "DEC") return c >= '0' && c <= '9';
    return true;   // HEX — every 0–9 / A–F is legal
}

void CalculatorView::applyDigitEnables(const QString &baseToken)
{
    const int baseRank = baseIndex(baseToken);
    // digitRank: 0–1 → rank 0,  2–7 → rank 1,  8–9 → rank 2,  A–F → rank 3
    for (int i = 0; i < 16; ++i) {
        if (!digitBtns_[i]) continue;
        const int digitRank = (i <= 1) ? 0 : (i <= 7) ? 1 : (i <= 9) ? 2 : 3;
        digitBtns_[i]->setEnabled(baseRank >= digitRank);
    }
}
