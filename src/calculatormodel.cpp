#include "calculatormodel.h"

#include <QChar>

namespace {

/// Format a masked value in the given base, honouring the active width.
/// `signedValue` is already sign-extended relative to the width; signedness only
/// affects the decimal representation.
QString formatValue(quint64 u, qint64 signedValue,
                    CalculatorModel::Base base, bool isSigned, int width)
{
    switch (base) {
    case CalculatorModel::Base::Binary: {
        QString bits;
        for (int i = width - 1; i >= 0; --i) {
            bits += ((u >> i) & 1ULL) ? QLatin1Char('1') : QLatin1Char('0');
            if (i % 8 == 0 && i != 0)
                bits += QLatin1Char(' ');
        }
        return bits;
    }
    case CalculatorModel::Base::Octal:
        return QString::number(u, 8);
    case CalculatorModel::Base::Decimal:
        return isSigned ? QString::number(signedValue) : QString::number(u);
    case CalculatorModel::Base::Hexadecimal: {
        const int digits = width / 4;
        QString h = QString("%1").arg(u, digits, 16, QLatin1Char('0')).toUpper();
        for (int i = digits - 4; i > 0; i -= 4)
            h.insert(i, QLatin1Char(' '));   // group into 4-digit words
        return h;
    }
    }
    return {};
}

} // namespace

CalculatorModel::CalculatorModel(QObject *parent)
    : QObject(parent)
    , stack_{0, 0, 0, 0}
    , buffer_(QLatin1String("0"))
    , entering_(false)
    , liftOnNext_(true)
    , base_(Base::Decimal)
    , signed_(false)
    , bitWidth_(64)
{
}

// ---------------------------------------------------------------------------
// read-only accessors
// ---------------------------------------------------------------------------

QString CalculatorModel::binaryString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister()),
                       Base::Binary, signed_, bitWidth_);
}

QString CalculatorModel::octalString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister()),
                       Base::Octal, signed_, bitWidth_);
}

QString CalculatorModel::decimalString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister()),
                       Base::Decimal, signed_, bitWidth_);
}

QString CalculatorModel::hexadecimalString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister()),
                       Base::Hexadecimal, signed_, bitWidth_);
}

QString CalculatorModel::charString() const
{
    const unsigned char c = static_cast<unsigned char>(xRegister() & 0xFFu);
    if (c >= 0x20 && c <= 0x7E)
        return QChar(c);
    return QString(QChar(0x00B7));   // middle dot for "no printable char"
}

std::array<QString, 4> CalculatorModel::stackStrings() const
{
    // Storage order is X,Y,Z,T but the View renders top-to-bottom as T,Z,Y,X.
    const auto fmt = [this](qint64 v) {
        const quint64 u = static_cast<quint64>(v) & mask();
        return signed_ ? QString::number(signedWithinWidth(u))
                       : QString::number(u);
    };
    return { fmt(stack_[3]), fmt(stack_[2]), fmt(stack_[1]), fmt(stack_[0]) };
}

// ---------------------------------------------------------------------------
// presentation slots
// ---------------------------------------------------------------------------

void CalculatorModel::setBase(Base base)
{
    if (entering_) {
        stack_[0] = normalize(parseBuffer());
        entering_ = false;
    }
    base_ = base;
    liftOnNext_ = false;
    emit modeChanged();
    refresh();
}

void CalculatorModel::setSignedMode(bool enabled)
{
    signed_ = enabled;
    emit modeChanged();
    refresh();
}

void CalculatorModel::setBitWidth(int bits)
{
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
        return;
    if (bits == bitWidth_)
        return;
    bitWidth_ = bits;
    for (auto &v : stack_)
        v = normalize(v);          // re-mask every register to the new width
    if (entering_)
        stack_[0] = normalize(parseBuffer());
    liftOnNext_ = false;
    emit bitWidthChanged(bits);
    emit modeChanged();
    refresh();
}

// ---------------------------------------------------------------------------
// numeric entry
// ---------------------------------------------------------------------------

void CalculatorModel::inputDigit(const QString &digit)
{
    const QChar c = digit.length() == 1 ? digit.at(0).toUpper() : QChar();
    if (!isValidDigit(QString(c)))
        return;

    // Starting a fresh entry clears the stale buffer first — do this BEFORE the
    // length guard, otherwise a buffer left full by a previous ENTER blocks new
    // input even though we're not appending to it.
    if (!entering_) {
        if (liftOnNext_)
            lift();
        buffer_.clear();
        entering_ = true;
    }
    if (buffer_.length() >= maxInputLength())
        return;

    if (buffer_ == QLatin1String("0") && c != '0')
        buffer_.clear();
    buffer_ += c;

    stack_[0] = normalize(parseBuffer());
    liftOnNext_ = true;
    refresh();
}

void CalculatorModel::backspace()
{
    if (!entering_)
        return;
    buffer_.chop(1);
    if (buffer_.isEmpty())
        buffer_ = QLatin1String("0");
    stack_[0] = normalize(parseBuffer());
    refresh();
}

bool CalculatorModel::loadFromText(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty())
        return false;

    // Leading sign.
    bool negative = false;
    if (s.startsWith('-')) { negative = true; s = s.mid(1).trimmed(); }
    else if (s.startsWith('+')) { s = s.mid(1).trimmed(); }
    if (s.isEmpty())
        return false;

    // C-style base prefix overrides the active base; default to it otherwise.
    int radix = base_ == Base::Hexadecimal ? 16 :
                base_ == Base::Decimal     ? 10 :
                base_ == Base::Octal       ? 8  : 2;
    if (s.size() >= 2 && s.at(0) == '0') {
        const QChar p = s.at(1).toLower();
        if (p == 'x')      { radix = 16; s = s.mid(2); }
        else if (p == 'b') { radix = 2;  s = s.mid(2); }
        else if (p == 'o') { radix = 8;  s = s.mid(2); }
    }

    // Drop the digit separators the model itself emits (spaces) plus the ones
    // other tools copy out (underscores, commas).
    s.remove(' ');
    s.remove('_');
    s.remove(',');
    if (s.isEmpty())
        return false;

    bool ok = false;
    const quint64 v = s.toULongLong(&ok, radix);
    if (!ok)
        return false;

    const quint64 finalv = negative ? static_cast<quint64>(-static_cast<qint64>(v)) : v;
    stack_[0] = normalize(static_cast<qint64>(finalv));   // masked to width / sign
    buffer_ = QStringLiteral("0");
    entering_ = false;
    liftOnNext_ = true;   // a following typed digit lifts the stack, preserving this
    refresh();
    return true;
}

// ---------------------------------------------------------------------------
// stack / register commands
// ---------------------------------------------------------------------------

void CalculatorModel::clearX()
{
    buffer_ = QLatin1String("0");
    stack_[0] = 0;
    entering_ = true;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::clearAll()
{
    stack_ = {0, 0, 0, 0};
    buffer_ = QLatin1String("0");
    entering_ = false;
    liftOnNext_ = true;
    refresh();
}

void CalculatorModel::negate()
{
    // two's complement within the active width
    stack_[0] = normalize(-stack_[0]);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::enter()
{
    if (entering_) {
        stack_[0] = normalize(parseBuffer());
        entering_ = false;
    }
    lift();
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::rollDown()
{
    const qint64 t = stack_[3];
    stack_[3] = stack_[0];   // T <- X
    stack_[0] = stack_[1];   // X <- Y
    stack_[1] = stack_[2];   // Y <- Z
    stack_[2] = t;           // Z <- T
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::swapXY()
{
    std::swap(stack_[0], stack_[1]);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

// ---------------------------------------------------------------------------
// operations
// ---------------------------------------------------------------------------

void CalculatorModel::applyBinaryOp(Op op)
{
    if (entering_) {
        stack_[0] = normalize(parseBuffer());
        entering_ = false;
    }

    const quint64 x = static_cast<quint64>(stack_[0]) & mask();
    const quint64 y = static_cast<quint64>(stack_[1]) & mask();
    quint64 result = 0;

    switch (op) {
    case Op::Add: result = y + x; break;
    case Op::Sub: result = y - x; break;
    case Op::Mul: result = y * x; break;
    case Op::Div: result = (x != 0) ? (y / x) : 0; break;
    case Op::Mod: result = (x != 0) ? (y % x) : 0; break;
    case Op::And: result = y & x; break;
    case Op::Or:  result = y | x; break;
    case Op::Xor: result = y ^ x; break;
    case Op::Shl: result = (x < static_cast<quint64>(bitWidth_)) ? (y << x) : 0; break;
    case Op::Shr: result = (x < static_cast<quint64>(bitWidth_)) ? (y >> x) : 0; break;
    }

    stack_[0] = normalize(static_cast<qint64>(result));
    stack_[1] = stack_[2];
    stack_[2] = stack_[3];
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::applyNot()
{
    stack_[0] = normalize(~stack_[0]);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::toggleBit(int bit)
{
    if (bit < 0 || bit >= bitWidth_)
        return;
    const quint64 mask = 1ULL << bit;
    stack_[0] = normalize(static_cast<qint64>(static_cast<quint64>(stack_[0]) ^ mask));
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

// ---------------------------------------------------------------------------
// internals
// ---------------------------------------------------------------------------

void CalculatorModel::lift()
{
    stack_[3] = stack_[2];
    stack_[2] = stack_[1];
    stack_[1] = stack_[0];
}

void CalculatorModel::refresh()
{
    emit displayChanged();
}

qint64 CalculatorModel::parseBuffer() const
{
    const int radix =
        base_ == Base::Hexadecimal ? 16 :
        base_ == Base::Decimal     ? 10 :
        base_ == Base::Octal       ? 8  : 2;
    bool ok = false;
    const quint64 v = buffer_.toULongLong(&ok, radix);
    return ok ? static_cast<qint64>(v) : 0;
}

bool CalculatorModel::isValidDigit(const QString &digit) const
{
    if (digit.length() != 1)
        return false;
    const QChar c = digit.at(0).toUpper();
    switch (base_) {
    case Base::Binary:    return c == '0' || c == '1';
    case Base::Octal:     return c >= '0' && c <= '7';
    case Base::Decimal:   return c >= '0' && c <= '9';
    case Base::Hexadecimal: return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
    }
    return false;
}

int CalculatorModel::maxInputLength() const
{
    switch (base_) {
    case Base::Binary:    return bitWidth_;          // one char per bit
    case Base::Octal:     return (bitWidth_ + 2) / 3 + 1;
    case Base::Decimal:   return QString::number(mask(), 10).length();
    case Base::Hexadecimal: return bitWidth_ / 4;
    }
    return 16;
}

quint64 CalculatorModel::mask() const
{
    return (bitWidth_ >= 64) ? ~0ULL : ((1ULL << bitWidth_) - 1ULL);
}

qint64 CalculatorModel::normalize(qint64 v) const
{
    return static_cast<qint64>(static_cast<quint64>(v) & mask());
}

qint64 CalculatorModel::signedWithinWidth(quint64 v) const
{
    const quint64 m = v & mask();
    if (bitWidth_ < 64 && ((m >> (bitWidth_ - 1)) & 1ULL))
        return static_cast<qint64>(m) - static_cast<qint64>(1ULL << bitWidth_);
    return static_cast<qint64>(m);
}
