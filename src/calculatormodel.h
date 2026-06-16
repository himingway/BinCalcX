#pragma once

#include <QObject>
#include <QString>
#include <array>

/**
 * CalculatorModel — the "M" in MVC.
 *
 * Owns the entire calculation state. A pure, headless QObject that knows
 * nothing about rendering. The Controller translates UI gestures into slot
 * calls; the Model emits displayChanged()/modeChanged()/bitWidthChanged()
 * whenever anything a View might show has moved.
 *
 * An HP-style RPN machine with a four-register stack (T, Z, Y, X) where X is
 * the working register. Every value lives inside a configurable bit width
 * (8 / 16 / 32 / 64); results are masked to that width, sign interpretation is
 * relative to it, and shifts are clamped to it. Signedness is presentation only.
 */
class CalculatorModel : public QObject
{
    Q_OBJECT
public:
    enum class Base { Binary, Octal, Decimal, Hexadecimal };
    Q_ENUM(Base)

    enum class Op {
        Add, Sub, Mul, Div, Mod,
        And, Or, Xor, Shl, Shr
    };
    Q_ENUM(Op)

    explicit CalculatorModel(QObject *parent = nullptr);

    // ---- read-only accessors (all derive from the X register) ----
    QString binaryString() const;        // width bits, space-separated bytes
    QString octalString() const;
    QString decimalString() const;
    QString hexadecimalString() const;   // padded to width, grouped into words
    QString charString() const;          // low byte as ASCII (· if not printable)

    /// T, Z, Y, X formatted in decimal (mirrors the reference layout).
    std::array<QString, 4> stackStrings() const;

    Base  currentBase() const { return base_; }
    bool  signedMode()  const { return signed_; }
    int   bitWidth()    const { return bitWidth_; }
    /// X as an unsigned pattern, already masked to the current width.
    quint64 xRegister() const { return static_cast<quint64>(stack_[0]) & mask(); }

public slots:
    // presentation
    void setBase(Base base);
    void setSignedMode(bool enabled);
    void setBitWidth(int bits);          // 8, 16, 32, 64

    // numeric entry (interpreted in the current base)
    void inputDigit(const QString &digit);
    void backspace();

    // stack / register commands
    void clearX();        // CLx
    void clearAll();      // CLR
    void negate();        // +/-  (two's complement within the width)
    void enter();         // ENTER — lift the stack, duplicating X into Y
    void rollDown();      // Rv
    void swapXY();        // X<>Y

    // operations
    void applyBinaryOp(Op op);  // pops Y and X, pushes Y op X (masked)
    void applyNot();            // bitwise NOT of X (unary, masked)
    void toggleBit(int bit);    // flip a single bit of X (ignored if out of range)

signals:
    void displayChanged();
    void modeChanged();
    void bitWidthChanged(int bits);

private:
    void lift();          // X -> Y -> Z -> T   (oldest T is lost)
    void refresh();       // emit displayChanged()
    qint64  parseBuffer() const;
    bool    isValidDigit(const QString &digit) const;
    int     maxInputLength() const;
    quint64 mask() const;
    qint64  normalize(qint64 v) const;            // v & mask()
    qint64  signedWithinWidth(quint64 v) const;   // sign-extended for display

    std::array<qint64, 4> stack_;  // X=[0]  Y=[1]  Z=[2]  T=[3]
    QString buffer_;               // string currently being typed into X
    bool    entering_;             // true while the user is actively typing X
    bool    liftOnNext_;           // next entry lifts the stack (HP "stack lift")
    Base    base_;
    bool    signed_;
    int     bitWidth_;             // 8, 16, 32, 64
};
