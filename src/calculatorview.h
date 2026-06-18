#pragma once

#include <QLineEdit>
#include <QPoint>
#include <QPushButton>
#include <QWidget>

#include <array>

class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QLabel;
class QTimer;
class QShowEvent;
class QToolButton;

/**
 * CalculatorView — the "V" in MVC (BinCalcX visual spec).
 *
 * Passive widget: emits high-level intents, exposes setters the Controller
 * pushes results into. Intent tokens are plain strings so the View never
 * imports Model types.
 *
 *   commands : ENTER | CLX | CLR | NEG | NOT | ROLL | SWAP | BSP
 *   ops      : ADD | SUB | MUL | DIV | MOD | AND | OR | XOR | SHL | SHR
 *   bases    : BIN | OCT | DEC | HEX
 *
 * Visual model:
 *   - VS Code-grade dark/light, driven by two palettes + a *base-dependent
 *     accent* (HEX green, DEC blue, OCT amber, BIN purple). The accent cascades
 *     onto lit bits, the active field, ENTER and the X register.
 *   - A fixed 4x16 = 64 textless bit grid. Cells past the active width are
 *     greyed out (hardware-truncation feedback) instead of removed. Hovering a
 *     cell reports its bit and decimal/hex place value in a quiet status line.
 */
class CalculatorView : public QWidget
{
    Q_OBJECT
public:
    explicit CalculatorView(QWidget *parent = nullptr);

    void setHexText(const QString &text);
    void setOctalText(const QString &text);
    void setDecimalText(const QString &text);
    void setBinaryText(const QString &text);  // stored for clipboard (no field)
    void setCharText(const QString &text);
    void setStackValues(const std::array<QString, 4> &values);
    void setXField(const QString &text);          // override the X register (slice preview)
    void setSignedMode(bool enabled);
    void setActiveBase(const QString &baseToken);   // accent + keypad enables
    void refreshBits(quint64 value);                 // restyle the 64 cells
    void setActiveWidth(int width);                  // grey-out cells >= width
    void setTheme(bool dark);
    void setStayOnTop(bool on);
    void setStatusMessage(const QString &msg);   // brief transient line (paste, …)

signals:
    void digitPressed(const QString &digit);
    void commandPressed(const QString &command);
    void binaryOpPressed(const QString &op);
    void baseSelected(const QString &baseToken);
    void signedModeChanged(bool enabled);
    void bitToggled(int bit);
    void bitWidthRequested(int width);
    void shiftRequested(bool left);   // ◀ (left=true) shifts X<<1, ▶ shifts X>>1
    void themeToggled(bool dark);
    void stayOnTopToggled(bool on);
    void pasteRequested(const QString &text);    // clipboard text → Model
    void sliceApplyRequested(int lo, int hi);    // "Set Width from slice" → Model
    void slicePushRequested(int lo, int hi);     // ENTER on a slice → push to Y, X unchanged
    void selectionChanged(int lo, int hi);       // -1,-1 when cleared → live X preview

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onThemeButton(bool checked);

private:
    // displays
    QLineEdit *hexDisplay_     = nullptr;
    QLineEdit *octDisplay_     = nullptr;
    QLineEdit *decimalDisplay_ = nullptr;
    QLabel    *charDisplay_    = nullptr;   // rich text → dimmed '.' placeholders
    std::array<QLineEdit *, 4> stackDisplays_{};

    // controls
    QCheckBox    *signedCheckbox_   = nullptr;
    QCheckBox    *stayOnTopCheckbox_= nullptr;
    QPushButton  *shiftLeftBtn_     = nullptr;   // ◀  X << 1
    QPushButton  *shiftRightBtn_    = nullptr;   // ▶  X >> 1
    QToolButton  *themeBtn_         = nullptr;
    QButtonGroup *widthGroup_       = nullptr;
    // Index: [0]=8  [1]=16  [2]=32  [3]=64
    std::array<QToolButton *, 4>  widthBtns_{};

    // Index: Base enum (0=BIN  1=OCT  2=DEC  3=HEX)
    std::array<QPushButton *, 4>  baseBtns_{};

    // Index: digit value 0–15  (0–9, A–F)
    std::array<QPushButton *, 16> digitBtns_{};

    std::array<QPushButton *, 64> bitButtons_{};
    QWidget *gridPanel_ = nullptr;   // bit-cell container; hit-tested for marquee select
    QLabel *statusLabel_ = nullptr;

    bool    dark_         = true;
    QString accent_;        // current base-dependent accent colour
    QString activeBase_ = QStringLiteral("DEC");   // for clipboard copy target
    QString binaryText_;                       // binary string (no field, for clipboard)
    QString charText_;                          // plain 8-char CHR string (model)
    int     activeWidth_   = 64;
    quint64 lastBitValue_  = ~quint64(0);   // cache for refreshBits early-out
    int     lastBitWidth_  = -1;
    // marquee bit-range selection (press → drag → release on the bit grid)
    bool         bitSelecting_ = false;
    bool         suppressClick_ = false;   // drag-select release → swallow its clicked()
    QPushButton *selAnchorBtn_ = nullptr;   // the pressed cell (implicit mouse grabber)
    int          selAnchorBit_ = -1;
    int          selCurBit_    = -1;
    int          selLo_        = -1;        // current selection [selLo_..selHi_]; -1 = none
    int          selHi_        = -1;
    int          focusBit_     = 0;         // keyboard-focus cell (Focus Ring); Bit 0 by default
    QTimer *statusTimer_ = nullptr;                 // auto-clears the status line
    bool    firstShow_    = true;                    // re-apply theme once realised

    void buildLayout();
    void copyActiveValue();       // Ctrl+C → clipboard, current base
    void renderCharDisplay();     // turn charText_ into dimmed-placeholder HTML
    void applyTheme(bool dark, const QString &accent);
    static QString accentForBase(const QString &baseToken, bool dark = true);

    QPushButton *makeDigit(const QString &text);
    QPushButton *makeOp(const QString &label, const QString &opToken);
    QPushButton *makeCommand(const QString &label, const QString &cmdToken,
                             const char *objectName = nullptr);
    QPushButton *makeBaseButton(const QString &label, const QString &token);
    QPushButton *makeShiftButton(const QString &glyph, bool left);
    QToolButton *makeWidthButton(const QString &label, int width);
    void applyDigitEnables(const QString &baseToken);
    static int baseIndex(const QString &token);  // "BIN"→0 "OCT"→1 "DEC"→2 "HEX"→3
    // Is this hex digit char a legal digit for the given base? Mirrors
    // CalculatorModel::isValidDigit so the keyboard layer can intercept an
    // out-of-base key (e.g. '2'…'9', 'A'…'F' while BIN is active) before it
    // ever reaches the Model — matching the on-screen keypad, which disables
    // those same buttons via applyDigitEnables().
    static bool isDigitValidForBase(QChar c, const QString &baseToken);
    // marquee bit-range selection + keyboard-focus helpers
    void extendBitSelection(int curBit);   // grow selection to [anchor..cur]
    void clearBitSelection();              // drop selection + highlight
    bool commitSelectionIfAny();           // push the previewed slice → Y (X unchanged); false if none
    void repaintBitCells();                // apply [bitsel] (selection) + [kfocus] (ring)
    void moveBitFocus(int delta);          // ±1 (←/→) or ±16 (↑/↓), wrapped mod 64
    void showSelectionStatus();            // status line: "Bits hi..lo = …"
    quint64 segmentValue() const;          // raw value of [selLo_..selHi_]; 0 if none
    QString segmentValueText() const;      // segmentValue() formatted for the active base
    QPushButton *bitButtonAt(const QPoint &globalPos) const;   // cell under cursor
};
