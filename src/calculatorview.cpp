#include "calculatorview.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpacerItem>
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

void CalculatorView::setHexText(const QString &text)     { hexDisplay_->setText(text); }
void CalculatorView::setOctalText(const QString &text)   { octDisplay_->setText(text); }
void CalculatorView::setDecimalText(const QString &text) { decimalDisplay_->setText(text); }
void CalculatorView::setBinaryText(const QString &text) { binaryText_ = text; }
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
}

void CalculatorView::setStackValues(const std::array<QString, 4> &values)
{
    for (int i = 0; i < 4; ++i)
        stackDisplays_[i]->setText(values[i]);
}

void CalculatorView::setXField(const QString &text)
{
    stackDisplays_[3]->setText(text);   // X = bottom register
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
}

void CalculatorView::setActiveWidth(int width)
{
    activeWidth_ = width;
    lastBitWidth_ = -1;   // invalidate refreshBits cache — width changed
    // Sync segmented-control UI. Per-register widths can be arbitrary (slice /
    // concat / replicate) — for a non-standard width, no segment is checked.
    const int active = (width == 8) ? 0 : (width == 16) ? 1
                      : (width == 32) ? 2 : (width == 64) ? 3 : -1;
    for (int i = 0; i < 4; ++i) {
        if (widthBtns_[i])
            widthBtns_[i]->setChecked(i == active);   // active == -1 → none checked
    }
}

void CalculatorView::setTheme(bool dark)
{
    dark_ = dark;
    applyTheme(dark, accent_);
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

void CalculatorView::onThemeButton(bool checked)
{
    dark_ = checked;
    themeBtn_->setText(checked ? tr("Dark") : tr("Light"));
    applyTheme(checked, accent_);
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
                statusLabel_->setText(tr("Bit %1 | Dec: %2 | Hex: 0x%3")
                                      .arg(bit)
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
    // Clipboard: copy the active-base value / paste a number into X.
    if (event->matches(QKeySequence::Copy)) {
        copyActiveValue();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        const QString clip = QGuiApplication::clipboard()->text();
        if (!clip.isEmpty())
            emit pasteRequested(clip);
        return;
    }

    const QString text = event->text();
    const int key = event->key();

    if (!text.isEmpty()) {
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
    }

    switch (key) {
    // ── Bit-grid keyboard focus navigation (active while this view holds focus;
    //    a HEX/DEC/OCT field keeps focus and handles its own keys, never here) ──
    case Qt::Key_Left:  moveBitFocus(+1);  return;   // one bit toward MSB
    case Qt::Key_Right: moveBitFocus(-1);  return;   // one bit toward LSB
    case Qt::Key_Up:    moveBitFocus(+16); return;   // up one row
    case Qt::Key_Down:  moveBitFocus(-16); return;   // down one row
    case Qt::Key_Space: emit bitToggled(focusBit_); return;   // toggle the focused bit
    case Qt::Key_Plus:      emit binaryOpPressed("ADD"); return;
    case Qt::Key_Minus:     emit binaryOpPressed("SUB"); return;
    case Qt::Key_Asterisk:  emit binaryOpPressed("MUL"); return;
    case Qt::Key_Slash:     emit binaryOpPressed("DIV"); return;
    case Qt::Key_Percent:   emit binaryOpPressed("MOD"); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // A selected slice is pushed to Y (X keeps its width); else normal lift.
        if (!commitSelectionIfAny())
            emit commandPressed("ENTER");
        return;
    case Qt::Key_Backspace: emit commandPressed("BSP"); return;
    case Qt::Key_Escape:
        // First Esc cancels an active bit-range selection; only with no
        // selection does Esc fall through to CLR (clear all).
        if (selLo_ >= 0) { clearBitSelection(); return; }
        emit commandPressed("CLR"); return;
    case Qt::Key_Delete:    emit commandPressed("CLX"); return;
    default: break;
    }
    QWidget::keyPressEvent(event);
}

void CalculatorView::copyActiveValue()
{
    // If a bit range is selected, copy just that segment's value (in the active
    // base). Otherwise copy the whole active-base value. Either way the text is
    // clean (no grouping separators) so it pastes straight into code; BIN has no
    // text field, so the full value comes from the stored binary string.
    QString text;
    if (selLo_ >= 0) {
        text = segmentValueText();
        QGuiApplication::clipboard()->setText(text);
        setStatusMessage(tr("Copied bits %1..%2 = %3").arg(selHi_).arg(selLo_).arg(text));
        return;
    }
    if (activeBase_ == "BIN") {
        text = binaryText_;
    } else {
        QLineEdit *fields[] = { nullptr, octDisplay_, decimalDisplay_, hexDisplay_ };
        QLineEdit *fld = fields[baseIndex(activeBase_)];
        text = fld ? fld->text() : decimalDisplay_->text();
    }
    text.remove(' ');
    QGuiApplication::clipboard()->setText(text);
    setStatusMessage(tr("Copied: %1").arg(text));
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
    showSelectionStatus();
    emit selectionChanged(selLo_, selHi_);   // live-preview the slice on the X register
}

void CalculatorView::clearBitSelection()
{
    const bool had = (selLo_ >= 0);
    selLo_ = selHi_ = -1;
    selCurBit_ = selAnchorBit_ = -1;
    repaintBitCells();   // always repaint — keeps the focus ring current too
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
}

void CalculatorView::showSelectionStatus()
{
    if (selLo_ < 0) return;
    const int n = selHi_ - selLo_ + 1;
    const quint64 seg = segmentValue();
    const QString bin = QString::number(seg, 2).rightJustified(n, '0');
    statusLabel_->setText(tr("Bits %1..%2  =  %3   0x%4   0b%5")
                          .arg(selHi_).arg(selLo_)
                          .arg(QString::number(seg))
                          .arg(QString::number(seg, 16).toUpper())
                          .arg(bin));
}

quint64 CalculatorView::segmentValue() const
{
    // The integer formed by the selected bits, shifted down to bit 0.
    if (selLo_ < 0) return 0;
    const int n = selHi_ - selLo_ + 1;
    const quint64 mask = (n >= 64) ? ~quint64(0) : ((1ULL << n) - 1);
    return (lastBitValue_ >> selLo_) & mask;
}

QString CalculatorView::segmentValueText() const
{
    // The segment's value rendered in the active base — clean (no separators),
    // and for BIN padded to the selected width so every selected bit shows.
    const quint64 seg = segmentValue();
    if (selLo_ < 0) return QString();
    if (activeBase_ == "HEX") return QString::number(seg, 16).toUpper();
    if (activeBase_ == "OCT") return QString::number(seg, 8);
    if (activeBase_ == "BIN") return QString::number(seg, 2).rightJustified(selHi_ - selLo_ + 1, '0');
    return QString::number(seg);   // DEC
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

    // Monospace font stack — used by every input field.
    const char *mono = "'JetBrains Mono','Cascadia Code','Fira Code','Consolas','Courier New',monospace";

    // %1..%28 = palette, %29..%32 = toolbar*, %33 = accent, %34 = mono, %35 = grid border, %36 = focus ring.
    const QString css = QStringLiteral(
        /* ── Global ── */
        "QWidget { background: %1; color: %27; }"
        /* ── Input fields (all share mono + same border model) ── */
        "QLineEdit { background: %3; color: %4; border: 1px solid %7; border-radius: 3px;"
        "  padding: 2px 5px; font-family: %34;"
        "  selection-background-color: %33; }"
        "QLineEdit#fieldActive { background: %3; color: %4; border: 1px solid %33; }"
        /* ── CHR display (label, rich text — looks like a field) ── */
        "QLabel#chrDisplay { background: %3; color: %4; border: 1px solid %7; border-radius: 3px;"
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
        "QPushButton:disabled { color: %28; background: %2; border-color: %2; }"
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
        "QPushButton#base { background: %5; color: %28; border: 1px solid %7; }"
        "QPushButton#base:checked { background: %33; color: %16; border: 1px solid %33; }"
        /* ── ◀ ▶ in-place shift keys — flat, borderless; accent on hover ── */
        "QPushButton#shiftBtn { background: transparent; border: none; color: %28; padding: 0; }"
        "QPushButton#shiftBtn:hover { color: %33; }"
        "QPushButton#shiftBtn:pressed { color: %33; }"
        /* ── Bit cells ── */
        "QPushButton#bitOffA { background: %21; border: 1px solid %23; border-radius: 2px; }"
        "QPushButton#bitOffB { background: %22; border: 1px solid %23; border-radius: 2px; }"
        "QPushButton#bitOn { background: %33; border: 1px solid %33; border-radius: 2px; }"
        "QPushButton#bitDisabled { background: %24; border: 1px solid %25; border-radius: 2px; }"
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
        "QLabel#status { color: %28; }"
        "QCheckBox { color: %27; background: transparent; spacing: 5px; }"
        /* ── Group box ── */
        "QGroupBox { color: %28; border: 1px solid %7; border-radius: 4px; margin-top: 10px;"
        "  padding-top: 6px; font-weight: bold; background: %2; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
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
        /* ── Top-bar theme toggle ── */
        "QToolButton#topBarBtn { min-height: 20px; max-height: 20px; padding: 0px 6px;"
        "  color: %8; background: %5; border: 1px solid %7; }"
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
    .arg(focusRing);                                                                                            // 36

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
static constexpr int kBitCellPx = 16;   // logical px; tuned to the 9pt mono glyph
static constexpr int kNibbleGapPx = 6;  // extra gap between 4-bit nibble groups

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

    tb->addSpacing(12);

    // [Theme toggle] Dark / Light
    themeBtn_ = new QToolButton(toolbar);
    themeBtn_->setObjectName("topBarBtn");
    themeBtn_->setCheckable(true);
    themeBtn_->setChecked(dark_);
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

    // [Base: BIN] — binary-entry toggle. Same #base chrome as the HEX/OCT/DEC
    // selectors below (checkable, accent-filled when active), placed between the
    // theme toggle and the W width group. Registers into baseBtns_[0], so it
    // checks/unchecks in lock-step with the others via setActiveBase().
    tb->addWidget(makeBaseButton(tr("BIN"), "BIN"));
    tb->addSpacing(8);

    // → push width group to the right
    tb->addStretch(1);

    // [Width] label + segmented capsule
    auto wLbl = new QLabel(tr("W"), toolbar);
    QFont wf = wLbl->font(); wf.setBold(true); wf.setPointSize(8); wLbl->setFont(wf);
    tb->addWidget(wLbl);
    tb->addSpacing(6);

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

    tb->addSpacing(16);

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
    auto grid = new QGridLayout(gridPanel_);
    grid->setContentsMargins(6, 5, 6, 7);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(1);

    for (int row = 0; row < 4; ++row) {
        const int high = 63 - 16 * row;          // row's top bit
        const int headerRow = row * 2;
        const int dataRow   = row * 2 + 1;

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
            // Nibble-group gap: a fixed-width spacer in the boundary column
            // (g*5+4) widens the break between 4-bit groups so each nibble
            // reads as a unit. Omitted after the last nibble (g==3) — no gap
            // is needed at a row's right edge.
            if (g < 3) {
                grid->addItem(new QSpacerItem(kNibbleGapPx, 0,
                                              QSizePolicy::Fixed, QSizePolicy::Minimum),
                              dataRow, g * 5 + 4);
            }
        }
    }
    topHBox->addWidget(gridPanel_, 3);

    // RPN stack (right, expanded)
    auto rpnGroup = new QGroupBox(tr("RPN  T Z Y X"), this);
    auto rpnGrid = new QGridLayout(rpnGroup);
    rpnGrid->setContentsMargins(6, 12, 6, 6);
    rpnGrid->setVerticalSpacing(3);
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
        if (i == 3) stackDisplays_[i]->setMinimumHeight(30);
        rpnGrid->addWidget(lbl, i, 0);
        rpnGrid->addWidget(stackDisplays_[i], i, 1);
    }
    topHBox->addWidget(rpnGroup, 2);
    root->addLayout(topHBox);

    // --- base rows (short fields, 2 compact rows) ---
    auto baseRows = new QVBoxLayout;
    baseRows->setSpacing(2);

    // Shared row height for the input fields, the ◀ ▶ shift keys and the base
    // selectors — so the shift keys sit pixel-aligned with the fields.
    const int baseFieldH = fontMetrics().height() + 8;

    auto row1 = new QHBoxLayout;
    row1->setSpacing(4);
    row1->addWidget(makeBaseButton(tr("HEX"), "HEX"));
    hexDisplay_ = new QLineEdit(this); hexDisplay_->setReadOnly(true);
    hexDisplay_->setFixedHeight(baseFieldH);
    row1->addWidget(hexDisplay_, 2);
    row1->addWidget(makeBaseButton(tr("OCT"), "OCT"));
    octDisplay_ = new QLineEdit(this); octDisplay_->setReadOnly(true);
    octDisplay_->setFixedHeight(baseFieldH);
    row1->addWidget(octDisplay_, 2);
    baseRows->addLayout(row1);

    //  [DEC] [DEC field] [✓ Signed] [◀] [▶] [CHR] [CHR field]
    auto row2 = new QHBoxLayout;
    row2->setSpacing(4);
    row2->addWidget(makeBaseButton(tr("DEC"), "DEC"));
    decimalDisplay_ = new QLineEdit(this); decimalDisplay_->setReadOnly(true);
    decimalDisplay_->setFixedHeight(baseFieldH);
    row2->addWidget(decimalDisplay_, 2);   // narrower than before — frees width for CHR
    signedCheckbox_ = new QCheckBox(tr("Signed"), this);
    signedCheckbox_->setChecked(false);
    signedCheckbox_->setFocusPolicy(Qt::NoFocus);
    connect(signedCheckbox_, &QCheckBox::toggled, this, &CalculatorView::signedModeChanged);
    row2->addWidget(signedCheckbox_);

    // ◀ ▶ — in-place single-bit shifts on X (distinct from the << >> binary ops,
    // which shift Y by the X amount). Left = X<<1 (truncated), right = X>>1
    // (logical, or arithmetic when Signed). Glyphs via code points (mirrors the
    // existing QChar(0x00B7) usage) so the source stays pure-ASCII — MSVC-safe.
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
    // Wide enough to seat 8 monospace glyphs comfortably (e.g. "DEADBEEF").
    const int chrW = charDisplay_->fontMetrics().horizontalAdvance(QStringLiteral("DEADBEEF")) + 20;
    charDisplay_->setFixedWidth(chrW);
    row2->addWidget(charDisplay_);
    baseRows->addLayout(row2);
    root->addLayout(baseRows);

    // --- controls: keypad (4x4) + ops (5x4) side by side ---
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
    struct D { int r, c; const char *label; const char *token; bool isOp; bool danger; };
    const D defs[] = {
        {0,0,"+","ADD",1,0},{0,1,"-","SUB",1,0},{0,2,"*","MUL",1,0},{0,3,"/","DIV",1,0},{0,4,"Mod","MOD",1,0},
        {1,0,"And","AND",1,0},{1,1,"Or","OR",1,0},{1,2,"Xor","XOR",1,0},{1,3,"<<","SHL",1,0},{1,4,">>","SHR",1,0},
        {2,0,"+/-","NEG",0,0},{2,1,"Not","NOT",0,0},{2,2,"Rv","ROLL",0,0},{2,3,"X<>Y","SWAP",0,0},{2,4,"Bsp","BSP",0,0},
        {3,0,"CLx","CLX",0,1},{3,1,"CLR","CLR",0,1},
    };
    for (const D &d : defs) {
        QPushButton *b;
        if (d.isOp)
            b = makeOp(d.label, d.token);
        else
            b = makeCommand(d.label, d.token, d.danger ? "danger" : nullptr);
        opGrid->addWidget(b, d.r, d.c);
    }
    // {,} concat and {N} replicate sit in row 3 (CLx CLR {,} {N}), keeping the
    // op grid at 4 rows so it matches the 4-row digit keypad height. A marquee
    // selection, if active, becomes X's slice for the op (extracted, width =
    // span) — without changing X while merely inspecting/copying.
    auto sliceOpBtn = [this](const QString &label, const QString &token, const QString &tip) {
        auto b = new QPushButton(label, this);
        b->setMinimumHeight(30); b->setMinimumWidth(34);
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
                                 tr("Concatenate {Y,X} — a selected slice becomes X")), 3, 2);
    opGrid->addWidget(sliceOpBtn(tr("{N}"), QStringLiteral("REPL"),
                                 tr("Replicate {N{X}} — a selected slice becomes X")), 3, 3);
    controlsHBox->addLayout(opGrid, 1);
    root->addLayout(controlsHBox);

    // --- full-width ENTER ---
    // A selected slice is pushed to Y (X keeps its width); else normal lift.
    auto enterBtn = new QPushButton(tr("ENTER"), this);
    enterBtn->setObjectName("enter");
    enterBtn->setMinimumHeight(34);
    enterBtn->setMinimumWidth(34);
    enterBtn->setFocusPolicy(Qt::NoFocus);
    connect(enterBtn, &QPushButton::clicked, this, [this]{
        if (!commitSelectionIfAny())
            emit commandPressed("ENTER");
    });
    root->addWidget(enterBtn);

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
// factories
// ---------------------------------------------------------------------------
QPushButton *CalculatorView::makeDigit(const QString &text)
{
    auto b = new QPushButton(text, this);
    b->setFixedSize(36, 30);
    b->setFocusPolicy(Qt::NoFocus);
    QFont f = b->font(); f.setBold(true); f.setPointSize(10); b->setFont(f);
    connect(b, &QPushButton::clicked, this, [this, text]{ emit digitPressed(text); });
    // '0'–'9' → index 0–9,  'A'–'F' → index 10–15
    const QChar c = text.at(0);
    const int idx = (c >= 'A') ? (10 + c.unicode() - 'A') : (c.unicode() - '0');
    digitBtns_[idx] = b;
    return b;
}

QPushButton *CalculatorView::makeOp(const QString &label, const QString &opToken)
{
    auto b = new QPushButton(label, this);
    b->setMinimumHeight(30); b->setMinimumWidth(34);
    b->setFocusPolicy(Qt::NoFocus);
    b->setObjectName("op");
    connect(b, &QPushButton::clicked, this, [this, opToken]{ emit binaryOpPressed(opToken); });
    return b;
}

QPushButton *CalculatorView::makeCommand(const QString &label, const QString &cmdToken,
                                         const char *objectName)
{
    auto b = new QPushButton(label, this);
    b->setMinimumHeight(30); b->setMinimumWidth(34);
    b->setFocusPolicy(Qt::NoFocus);
    if (objectName) b->setObjectName(QString::fromLatin1(objectName));
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
    b->setToolTip(left ? tr("Shift X left by 1   (X = X << 1)")
                       : tr("Shift X right by 1   (X = X >> 1, sign-extends if Signed)"));
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
