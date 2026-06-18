#include "calculatormodel.h"

#include <QChar>
#include <algorithm>

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
{
}

// ---------------------------------------------------------------------------
// read-only accessors
// ---------------------------------------------------------------------------

QString CalculatorModel::binaryString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister(), stack_[0].width),
                       Base::Binary, signed_, stack_[0].width);
}

QString CalculatorModel::octalString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister(), stack_[0].width),
                       Base::Octal, signed_, stack_[0].width);
}

QString CalculatorModel::decimalString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister(), stack_[0].width),
                       Base::Decimal, signed_, stack_[0].width);
}

QString CalculatorModel::hexadecimalString() const
{
    return formatValue(xRegister(), signedWithinWidth(xRegister(), stack_[0].width),
                       Base::Hexadecimal, signed_, stack_[0].width);
}

QString CalculatorModel::charString() const
{
    // Full 64-bit (8-byte) decode, MSB byte first so it reads left-to-right.
    // xRegister() is already masked to X's width, so the high bytes of a narrow
    // value are 0 → '.', which keeps the field a constant 8 cells with no jitter.
    const quint64 x = xRegister();
    QString out;
    out.reserve(8);
    for (int byteIndex = 7; byteIndex >= 0; --byteIndex) {
        const unsigned char c = static_cast<unsigned char>((x >> (byteIndex * 8)) & 0xFFu);
        if (c >= 0x20 && c <= 0x7E)
            out += QChar(c);
        else
            out += QLatin1Char('.');   // NUL / non-printable → safe placeholder
    }
    return out;
}

std::array<QString, 4> CalculatorModel::stackStrings() const
{
    // Storage order is X,Y,Z,T but the View renders top-to-bottom as T,Z,Y,X.
    return { verilogLiteral(stack_[3], base_, signed_),
             verilogLiteral(stack_[2], base_, signed_),
             verilogLiteral(stack_[1], base_, signed_),
             verilogLiteral(stack_[0], base_, signed_) };
}

QString CalculatorModel::xVerilogLiteral() const
{
    return verilogLiteral(stack_[0], base_, signed_);
}

QString CalculatorModel::sliceVerilogLiteral(int lo, int hi) const
{
    if (lo < 0 || hi < 0 || lo > hi)
        return xVerilogLiteral();
    if (hi >= stack_[0].width) hi = stack_[0].width - 1;   // don't read past X's real bits
    if (lo > hi)
        return xVerilogLiteral();
    Reg r;
    r.width = hi - lo + 1;
    r.value = static_cast<qint64>((static_cast<quint64>(stack_[0].value) >> lo) & mask(r.width));
    return verilogLiteral(r, base_, signed_);
}

// ---------------------------------------------------------------------------
// presentation slots
// ---------------------------------------------------------------------------

void CalculatorModel::setBase(Base base)
{
    if (entering_) {
        stack_[0].value = normalize(parseBuffer(), stack_[0].width);
        entering_ = false;
    }
    base_ = base;
    liftOnNext_ = false;
    refresh();   // emits displayChanged → view refreshes (base affects all displays)
}

void CalculatorModel::setSignedMode(bool enabled)
{
    signed_ = enabled;
    refresh();   // signedness is presentation only; refresh re-renders the displays
}

void CalculatorModel::setBitWidth(int bits)
{
    // The 8/16/32/64 segmented selector sets the W-selected ENTRY width, which
    // digit entry always uses (even after a concat/replicate narrows X's
    // carried width). X's own width follows it here; T/Z/Y keep theirs.
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
        return;
    widthSetting_ = bits;
    if (bits == stack_[0].width)
        return;
    stack_[0].width = bits;
    stack_[0].value = normalize(stack_[0].value, bits);
    if (entering_)
        stack_[0].value = normalize(parseBuffer(), bits);
    liftOnNext_ = false;
    refresh();   // smart-refresh emits bitWidthChanged(bits)
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
        stack_[0].width = widthSetting_;   // entry uses the W-selected width, not a narrow result's
    }
    if (buffer_.length() >= maxInputLength())
        return;   // cheap length pre-check (exact for BIN/HEX)

    // Form the candidate buffer (honouring the leading-zero rule), then reject
    // the digit if the value would overflow quint64 or the active width —
    // otherwise typing past the max (e.g. 20 nines in 64-bit DEC) wraps to 0.
    QString candidate = buffer_;
    if (candidate == QLatin1String("0") && c != '0')
        candidate.clear();
    candidate += c;

    bool ok = false;
    const quint64 v = candidate.toULongLong(&ok, radix());
    if (!ok || v > mask(stack_[0].width))
        return;

    buffer_ = candidate;
    stack_[0].value = normalize(static_cast<qint64>(v), stack_[0].width);
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
    stack_[0].value = normalize(parseBuffer(), stack_[0].width);
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
    // C-style base prefix overrides the active base; default to it otherwise.
    int radix = this->radix();
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
    stack_[0].value = normalize(static_cast<qint64>(finalv), stack_[0].width);
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
    stack_[0].value = 0;   // CLx keeps X's width (so a Set-Width survives CLx)
    entering_ = true;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::clearAll()
{
    // Zero every register at the W-selected width (CLR clears values but keeps
    // the W setting, so the stack is a clean W-width slate — not stuck at a
    // narrow concat result width).
    for (auto &r : stack_)
        r = Reg{ 0, widthSetting_ };
    buffer_ = QLatin1String("0");
    entering_ = false;
    liftOnNext_ = true;
    refresh();
}

void CalculatorModel::negate()
{
    // two's complement within X's width
    stack_[0].value = normalize(-stack_[0].value, stack_[0].width);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::enter()
{
    if (entering_) {
        stack_[0].value = normalize(parseBuffer(), stack_[0].width);
        entering_ = false;
    }
    lift();   // lifted copy into Y keeps X's width
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::rollDown()
{
    const Reg t = stack_[3];
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
        stack_[0].value = normalize(parseBuffer(), stack_[0].width);
        entering_ = false;
    }

    const int xw = stack_[0].width;
    const int yw = stack_[1].width;
    const quint64 x = static_cast<quint64>(stack_[0].value) & mask(xw);
    const quint64 y = static_cast<quint64>(stack_[1].value) & mask(yw);
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
    // Shift amount X is clamped to Y's width (Y is the value being shifted).
    case Op::Shl: result = (x < static_cast<quint64>(yw)) ? (y << x) : 0; break;
    case Op::Shr: result = (x < static_cast<quint64>(yw)) ? (y >> x) : 0; break;
    }

    // Result width: shifts stay in Y's field; everything else widens to the max.
    const int rw = (op == Op::Shl || op == Op::Shr) ? yw : std::max(xw, yw);
    stack_[0] = Reg{ normalize(static_cast<qint64>(result), rw), rw };
    stack_[1] = stack_[2];
    stack_[2] = stack_[3];
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::applyNot()
{
    stack_[0].value = normalize(~stack_[0].value, stack_[0].width);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::toggleBit(int bit)
{
    if (bit < 0 || bit >= stack_[0].width)
        return;
    const quint64 m = 1ULL << bit;
    stack_[0].value = normalize(static_cast<qint64>(static_cast<quint64>(stack_[0].value) ^ m),
                                stack_[0].width);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::shiftLeft()
{
    // X = X << 1, then physically truncate the high bits past X's width
    // (e.g. 0x80 << 1 in 8-bit becomes 0x00). Operates in place on X.
    const int w = stack_[0].width;
    const quint64 x = xRegister();
    stack_[0].value = normalize(static_cast<qint64>((x << 1) & mask(w)), w);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::shiftRight()
{
    // X = X >> 1. Logical (MSB → 0) when unsigned; arithmetic (sign-extended)
    // when signed, so a negative value stays negative relative to the width.
    const int w = stack_[0].width;
    const quint64 x = xRegister();
    quint64 result = x >> 1;
    if (signed_ && w > 0) {
        const quint64 signBit = 1ULL << (w - 1);
        if (x & signBit)
            result |= signBit;   // re-assert the sign bit shifted out of the MSB
    }
    stack_[0].value = normalize(static_cast<qint64>(result), w);
    entering_ = false;
    liftOnNext_ = false;
    refresh();
}

// ---------------------------------------------------------------------------
// hardware-grade operations
// ---------------------------------------------------------------------------

void CalculatorModel::concatenate()
{
    // {Y,X}: join Y and X by their respective widths → result.width = yw + xw.
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int xw = stack_[0].width;
    const int yw = stack_[1].width;
    const quint64 xv = static_cast<quint64>(stack_[0].value) & mask(xw);
    const quint64 yv = static_cast<quint64>(stack_[1].value) & mask(yw);
    const int rw = (yw + xw >= 64) ? 64 : (yw + xw);
    // The `xw >= 64` branch is REQUIRED, not redundant: `yv << xw` is undefined
    // behaviour for a shift count >= 64, and the `rw` clamp above does NOT save
    // it (the shift happens before the mask). When xw >= 64 no bit of Y can land
    // in the result, so it collapses to just X. Do not "simplify" this away.
    quint64 rv = (xw >= 64) ? xv : (((yv << xw) | xv) & mask(rw));
    stack_[0] = Reg{ normalize(static_cast<qint64>(rv), rw), rw };
    stack_[1] = stack_[2];
    stack_[2] = stack_[3];
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::replicate()
{
    // {N{X}} with N = Y: X repeated N times → result.width = N * X.width (capped 64).
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int w = stack_[0].width;
    const quint64 xv = static_cast<quint64>(stack_[0].value) & mask(w);
    const quint64 N = static_cast<quint64>(stack_[1].value);
    const quint64 total = N * static_cast<quint64>(w);
    const int rw = (total >= 64) ? 64 : static_cast<int>(total);
    quint64 rv = 0;
    if (w > 0) {
        for (quint64 i = 0; i < N; ++i) {
            const quint64 shift = i * static_cast<quint64>(w);
            if (shift >= 64) break;   // cap iterations; anything higher is masked away
            rv |= (xv << shift);
        }
    }
    rv &= mask(rw);
    stack_[0] = Reg{ normalize(static_cast<qint64>(rv), rw), rw };
    stack_[1] = stack_[2];
    stack_[2] = stack_[3];
    liftOnNext_ = false;
    refresh();
}

void CalculatorModel::applySlice(int lo, int hi)
{
    // "Set Width from slice": the selected range becomes X's width, and its bits
    // are extracted (shifted down) into X. Clamp hi to X's real width so a slice
    // past the end reads only genuine bits (not fabricated zeros).
    if (lo < 0 || hi < 0 || lo > hi || hi >= 64)
        return;
    if (hi >= stack_[0].width) hi = stack_[0].width - 1;
    if (lo > hi)
        return;
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int newW = hi - lo + 1;
    const quint64 xv = xRegister();
    stack_[0].value = normalize(static_cast<qint64>((xv >> lo) & mask(newW)), newW);
    stack_[0].width = newW;
    entering_ = false;
    liftOnNext_ = false;
    refresh();   // smart-refresh emits bitWidthChanged(newW)
}

void CalculatorModel::pushSlice(int lo, int hi)
{
    // ENTER on a selection: lift the stack with the selected slice placed in Y
    // (its own width), leaving X UNCHANGED — so X keeps its W-selected width
    // (e.g. 64) and is ready for the next entry. Distinct from applySlice,
    // which narrows X to the slice. hi is clamped to X's width (genuine bits only).
    if (lo < 0 || hi < 0 || lo > hi || hi >= 64)
        return;
    if (hi >= stack_[0].width) hi = stack_[0].width - 1;
    if (lo > hi)
        return;
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int span = hi - lo + 1;
    const quint64 seg = (xRegister() >> lo) & mask(span);
    lift();   // X→Y→Z→T (oldest T lost); X stays as-is
    stack_[1] = Reg{ normalize(static_cast<qint64>(seg), span), span };   // Y = the slice
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
    // Centralize width signalling: any op that moves X or changes X's width
    // (enter, rollDown, swapXY, applyBinaryOp, concat, replicate, applySlice,
    // setBitWidth, clearAll) automatically syncs the View's selector + greying.
    if (stack_[0].width != lastWidth_) {
        lastWidth_ = stack_[0].width;
        emit bitWidthChanged(stack_[0].width);
    }
    emit displayChanged();
}

qint64 CalculatorModel::parseBuffer() const
{
    bool ok = false;
    const quint64 v = buffer_.toULongLong(&ok, radix());
    return ok ? static_cast<qint64>(v) : 0;
}

int CalculatorModel::radix() const
{
    return base_ == Base::Hexadecimal ? 16 :
           base_ == Base::Decimal     ? 10 :
           base_ == Base::Octal       ? 8  : 2;
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
    const int w = stack_[0].width;
    switch (base_) {
    case Base::Binary:    return w;          // one char per bit
    case Base::Octal:     return (w + 2) / 3 + 1;
    case Base::Decimal:   return QString::number(mask(w), 10).length();
    case Base::Hexadecimal: return (w + 3) / 4;   // ceil — exact for 8/16/32/64, correct for slice widths
    }
    return 16;
}

quint64 CalculatorModel::mask(int width) const
{
    return (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
}

qint64 CalculatorModel::normalize(qint64 v, int width) const
{
    return static_cast<qint64>(static_cast<quint64>(v) & mask(width));
}

qint64 CalculatorModel::signedWithinWidth(quint64 v, int width) const
{
    const quint64 m = v & mask(width);
    if (width > 0 && width < 64 && ((m >> (width - 1)) & 1ULL))
        return static_cast<qint64>(m) - static_cast<qint64>(1ULL << width);
    return static_cast<qint64>(m);
}

QString CalculatorModel::verilogLiteral(const Reg &r, Base base, bool isSigned)
{
    const int w = r.width;
    if (w <= 0)
        return QStringLiteral("0'd0");
    const quint64 u = static_cast<quint64>(r.value) & ((w >= 64) ? ~0ULL : ((1ULL << w) - 1ULL));
    // Minimal digits (no leading-zero padding): the width prefix already states
    // the width, and this keeps wide literals readable in the RPN field.
    switch (base) {
    case Base::Binary:
        return QStringLiteral("%1'b%2").arg(w).arg(QString::number(u, 2));
    case Base::Octal:
        return QStringLiteral("%1'o%2").arg(w).arg(QString::number(u, 8));
    case Base::Hexadecimal:
        return QStringLiteral("%1'h%2").arg(w).arg(QString::number(u, 16).toUpper());
    case Base::Decimal: {
        qint64 s = static_cast<qint64>(u);
        if (isSigned && w < 64 && ((u >> (w - 1)) & 1ULL))
            s = static_cast<qint64>(u) - static_cast<qint64>(1ULL << w);
        if (isSigned)
            return QStringLiteral("%1'sd%2").arg(w).arg(QString::number(s));   // e.g. 8'sd-1
        return QStringLiteral("%1'd%2").arg(w).arg(QString::number(u));        // e.g. 8'd255
    }
    }
    return {};
}
