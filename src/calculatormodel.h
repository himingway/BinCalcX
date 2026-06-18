#pragma once

#include <QObject>
#include <QString>
#include <array>

/**
 * CalculatorModel — the "M" in MVC.
 *
 * Owns the entire calculation state. A pure, headless QObject that knows
 * nothing about rendering. The Controller translates UI gestures into slot
 * calls; the Model emits displayChanged()/bitWidthChanged()
 * whenever anything a View might show has moved.
 *
 * An HP-style RPN machine with a four-register stack (T, Z, Y, X) where X is
 * the working register. Each register carries its OWN bit width (1..64) so
 * hardware idioms work: concatenation {Y,X} joins Y and X by their widths,
 * replication {N{X}} repeats X, and reductions (&|^) collapse a value to one
 * bit. The 8/16/32/64 selector and a bit-grid slice both set X's width;
 * results are masked to the relevant width, sign interpretation is relative
 * to it, and shifts are clamped to it. Signedness is presentation only.
 * The RPN stack renders as SystemVerilog literals (8'hFF, 4'b1101, …).
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
    QString binaryString() const;        // X-width bits, space-separated bytes
    QString octalString() const;
    QString decimalString() const;
    QString hexadecimalString() const;   // padded to X width, grouped into words
    QString charString() const;          // 8-byte (full 64-bit) decode, MSB→LSB;
                                         // '.' for NUL/non-printable so the field
                                         // is always exactly 8 cells (no jitter)

    /// T, Z, Y, X as SystemVerilog literals (<w>'<base><value>), one per register.
    std::array<QString, 4> stackStrings() const;

    /// X alone as a SystemVerilog literal (for restoring the X field).
    QString xVerilogLiteral() const;
    /// A bit-slice [lo..hi] of X as a SystemVerilog literal (live preview; X is
    /// not modified). Width = hi-lo+1, value = the selected segment shifted down.
    QString sliceVerilogLiteral(int lo, int hi) const;

    Base  currentBase() const { return base_; }
    bool  signedMode()  const { return signed_; }
    int   bitWidth()    const { return stack_[0].width; }   // X's width
    /// X as an unsigned pattern, already masked to X's own width.
    quint64 xRegister() const { return static_cast<quint64>(stack_[0].value) & mask(stack_[0].width); }

public slots:
    // presentation
    void setBase(Base base);
    void setSignedMode(bool enabled);
    void setBitWidth(int bits);          // 8, 16, 32, 64 (sets X's width only)

    // numeric entry (interpreted in the current base)
    void inputDigit(const QString &digit);
    void backspace();

    /// Parse a pasted string into X. Honours C-style base prefixes
    /// (0x/0X, 0b/0B, 0o/0O) — otherwise the current base — and strips digit
    /// separators (spaces, underscores, commas) and a leading sign. The value
    /// is masked to X's width; returns false if not a valid number.
    bool loadFromText(const QString &text);

    // stack / register commands
    void clearX();        // CLx  (keeps X's width)
    void clearAll();      // CLR  (resets every register to 0 / width 64)
    void negate();        // +/-  (two's complement within the width)
    void enter();         // ENTER — lift the stack, duplicating X into Y
    void rollDown();      // Rv
    void swapXY();        // X<>Y

    // operations
    void applyBinaryOp(Op op);  // pops Y and X, pushes Y op X (masked, width = max)
    void applyNot();            // bitwise NOT of X (unary, masked)
    void toggleBit(int bit);    // flip a single bit of X (ignored if out of range)

    /// In-place single-bit shifts on X (the ◀ ▶ buttons), distinct from the
    /// SHL/SHR binary ops (which shift Y by the X amount). Left shifts truncate
    /// to X's width; right shifts are logical when unsigned and arithmetic
    /// (sign-extending) when signed, relative to X's width.
    void shiftLeft();
    void shiftRight();

    // hardware-grade operations
    void concatenate();           // {Y,X} — join Y and X by their widths
    void replicate();             // {N{X}} — X replicated N times (N = Y)
    void applySlice(int lo, int hi);  // set X's width to [lo..hi], extracting that segment
    void pushSlice(int lo, int hi);   // ENTER on a selection: slice → Y, X unchanged (keeps its width)

signals:
    void displayChanged();
    void bitWidthChanged(int bits);

private:
    struct Reg { qint64 value = 0; int width = 64; };

    void lift();          // X -> Y -> Z -> T   (oldest T is lost)
    void refresh();       // emit bitWidthChanged (if X width changed) + displayChanged
    qint64  parseBuffer() const;
    bool    isValidDigit(const QString &digit) const;
    int     radix() const;            // base_ → 2/8/10/16
    int     maxInputLength() const;
    quint64 mask(int width) const;
    qint64  normalize(qint64 v, int width) const;            // v & mask(width)
    qint64  signedWithinWidth(quint64 v, int width) const;   // sign-extended for display
    static QString verilogLiteral(const Reg &r, Base base, bool isSigned);

    std::array<Reg, 4> stack_;  // X=[0]  Y=[1]  Z=[2]  T=[3]
    QString buffer_ = QStringLiteral("0");   // string currently being typed into X
    bool    entering_  = false;              // true while the user is actively typing X
    bool    liftOnNext_ = true;              // next entry lifts the stack (HP "stack lift")
    Base    base_      = Base::Decimal;
    bool    signed_    = false;
    int     widthSetting_ = 64;              // the W-selected entry width (8/16/32/64)
    int     lastWidth_ = 64;                 // last width broadcast via bitWidthChanged
};
