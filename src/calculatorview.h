#pragma once

#include <QLineEdit>
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
 *     cell reports its bit/mask in a quiet status line.
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
    QLabel *statusLabel_ = nullptr;

    bool    dark_         = true;
    QString accent_;        // current base-dependent accent colour
    QString activeBase_ = QStringLiteral("DEC");   // for clipboard copy target
    QString binaryText_;                       // binary string (no field, for clipboard)
    QString charText_;                          // plain 8-char CHR string (model)
    int     activeWidth_   = 64;
    quint64 lastBitValue_  = ~quint64(0);   // cache for refreshBits early-out
    int     lastBitWidth_  = -1;
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
};
