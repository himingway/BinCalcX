#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include <array>
#include <map>

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
    QLineEdit *charDisplay_    = nullptr;
    std::array<QLineEdit *, 4> stackDisplays_{};

    // controls
    QCheckBox    *signedCheckbox_   = nullptr;
    QCheckBox    *stayOnTopCheckbox_= nullptr;
    QToolButton  *themeBtn_         = nullptr;
    QButtonGroup *widthGroup_       = nullptr;
    std::map<int, QToolButton *> widthButtons_;

    std::map<QString, QPushButton *> baseButtons_;
    std::map<QString, QLineEdit *>   baseFields_;
    std::map<QString, QPushButton *> digitButtons_;

    std::array<QPushButton *, 64> bitButtons_{};
    QLabel *statusLabel_ = nullptr;

    bool    dark_         = true;
    QString accent_;        // current base-dependent accent colour
    QString activeBase_ = QStringLiteral("DEC");   // for clipboard copy target
    int     activeWidth_   = 64;
    QTimer *statusTimer_ = nullptr;                 // auto-clears the status line
    bool    firstShow_    = true;                    // re-apply theme once realised

    void buildLayout();
    void copyActiveValue();       // Ctrl+C → clipboard, current base
    void applyTheme(bool dark, const QString &accent);
    static QString accentForBase(const QString &baseToken, bool dark = true);

    QPushButton *makeDigit(const QString &text);
    QPushButton *makeOp(const QString &label, const QString &opToken);
    QPushButton *makeCommand(const QString &label, const QString &cmdToken,
                             const char *objectName = nullptr);
    QPushButton *makeBaseButton(const QString &label, const QString &token);
    QToolButton *makeWidthButton(const QString &label, int width);
    void applyDigitEnables(const QString &baseToken);
};
