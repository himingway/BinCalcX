#include "calculatormodel.h"

#include <QChar>
#include <algorithm>

namespace {

constexpr std::size_t kMaxHistory = 128;

/// Parse a Verilog / SystemVerilog literal like `8'hFF`, `32'b1010_0101`,
/// `8'sd-1`, `-8'd5`, `'hDEAD`. Sets `outWidth` to -1 when unsized (`'hFF`),
/// or `1..64` when sized. Returns false if `raw` does not contain `'`.
bool parseVerilogLiteral(const QString &raw, bool outerNeg,
                         int &outWidth, quint64 &outValue)
{
    const int tick = raw.indexOf(QLatin1Char('\''));
    if (tick < 0)
        return false;

    const QString widthPart = raw.left(tick).trimmed();
    int width = -1;
    if (!widthPart.isEmpty()) {
        bool okW = false;
        const int w = widthPart.toInt(&okW, 10);
        if (!okW || w <= 0)
            return false;
        width = std::min(w, 64);
    }

    QString body = raw.mid(tick + 1).trimmed();
    if (body.isEmpty())
        return false;
    if (body.at(0).toLower() == QLatin1Char('s'))
        body = body.mid(1).trimmed();
    if (body.isEmpty())
        return false;

    int radix = 0;
    const QChar b = body.at(0).toLower();
    if      (b == QLatin1Char('b')) radix = 2;
    else if (b == QLatin1Char('o')) radix = 8;
    else if (b == QLatin1Char('d')) radix = 10;
    else if (b == QLatin1Char('h')) radix = 16;
    else return false;

    QString digits = body.mid(1).trimmed();
    bool innerNeg = false;
    if (digits.startsWith(QLatin1Char('-')))      { innerNeg = true; digits = digits.mid(1).trimmed(); }
    else if (digits.startsWith(QLatin1Char('+'))) { digits = digits.mid(1).trimmed(); }

    digits.remove(QLatin1Char(' '));
    digits.remove(QLatin1Char('_'));
    digits.remove(QLatin1Char(','));
    if (digits.isEmpty())
        return false;

    bool okV = false;
    const quint64 v = digits.toULongLong(&okV, radix);
    if (!okV)
        return false;

    outWidth = width;
    outValue = (outerNeg != innerNeg) ? (0ULL - v) : v;
    return true;
}

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
        const int digits = std::max(1, (width + 3) / 4);
        QString h = QString("%1").arg(u, digits, 16, QLatin1Char('0')).toUpper();
        for (int i = digits - 4; i > 0; i -= 4)
            h.insert(i, QLatin1Char(' '));   // group into 4-digit words
        return h;
    }
    }
    return {};
}

/// Reinterpret a quint64 bit pattern as the equivalent qint64 two's-complement
/// value. A plain static_cast is implementation-defined for patterns ≥ 2^63;
/// this arithmetic form is fully defined in C++17.
qint64 twoComplement(quint64 u)
{
    return (u < (1ULL << 63)) ? static_cast<qint64>(u)
                              : -static_cast<qint64>(~u) - 1;
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

std::array<QString, 4> CalculatorModel::stackTooltips() const
{
    return { registerTooltip(stack_[3], QStringLiteral("T")),
             registerTooltip(stack_[2], QStringLiteral("Z")),
             registerTooltip(stack_[1], QStringLiteral("Y")),
             registerTooltip(stack_[0], QStringLiteral("X")) };
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
    setCustomBitWidth(bits);
}

void CalculatorModel::setCustomBitWidth(int bits)
{
    if (bits < 1 || bits > 64)
        return;
    const HistoryEntry before = captureState();
    widthSetting_ = bits;
    if (bits == stack_[0].width)
        return;
    stack_[0].width = bits;
    stack_[0].value = normalize(stack_[0].value, bits);
    if (entering_)
        stack_[0].value = normalize(parseBuffer(), bits);
    liftOnNext_ = false;
    recordHistory(before);
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

    const HistoryEntry before = captureState();
    const bool startingEntry = !entering_;

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
    if (buffer_.length() >= maxInputLength()) {
        if (startingEntry) restoreState(before);
        return;   // cheap length pre-check (exact for BIN/HEX)
    }

    // Form the candidate buffer (honouring the leading-zero rule), then reject
    // the digit if the value would overflow quint64 or the active width —
    // otherwise typing past the max (e.g. 20 nines in 64-bit DEC) wraps to 0.
    QString candidate = buffer_;
    if (candidate == QLatin1String("0") && c != '0')
        candidate.clear();
    candidate += c;

    bool ok = false;
    const quint64 v = candidate.toULongLong(&ok, radix());
    if (!ok || v > mask(stack_[0].width)) {
        if (startingEntry) restoreState(before);
        return;
    }

    buffer_ = candidate;
    stack_[0].value = normalize(static_cast<qint64>(v), stack_[0].width);
    liftOnNext_ = true;
    // Coalesce multi-digit typing into a single undo step (captured at start of entry).
    if (startingEntry)
        recordHistory(before);
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

    int targetWidth = stack_[0].width;
    quint64 finalv = 0;

    if (s.contains(QLatin1Char('\''))) {
        int verilogWidth = -1;
        if (!parseVerilogLiteral(s, negative, verilogWidth, finalv))
            return false;
        if (verilogWidth > 0)
            targetWidth = verilogWidth;
    } else {
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

        // Negate in the unsigned domain: `-static_cast<qint64>(v)` is UB for
        // v ≥ 2^63 (the INT64_MIN pattern), and unsigned wraparound is exactly the
        // two's-complement result the mask below then narrows to the width.
        finalv = negative ? (0ULL - v) : v;
    }

    const HistoryEntry before = captureState();
    stack_[0].width = targetWidth;
    stack_[0].value = normalize(static_cast<qint64>(finalv), targetWidth);
    buffer_ = QStringLiteral("0");
    entering_ = false;
    liftOnNext_ = true;   // a following typed digit lifts the stack, preserving this
    recordHistory(before);
    refresh();
    return true;
}

// ---------------------------------------------------------------------------
// stack / register commands
// ---------------------------------------------------------------------------

void CalculatorModel::clearX()
{
    const HistoryEntry before = captureState();
    buffer_ = QLatin1String("0");
    stack_[0].value = 0;   // CLx keeps X's width (so a Set-Width survives CLx)
    entering_ = true;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::clearAll()
{
    const HistoryEntry before = captureState();
    // Zero every register at the W-selected width (CLR clears values but keeps
    // the W setting, so the stack is a clean W-width slate — not stuck at a
    // narrow concat result width).
    for (auto &r : stack_)
        r = Reg{ 0, widthSetting_ };
    buffer_ = QLatin1String("0");
    entering_ = false;
    liftOnNext_ = true;
    recordHistory(before);
    refresh();
}

void CalculatorModel::negate()
{
    const HistoryEntry before = captureState();
    // Two's complement within X's width. Negate in the unsigned domain —
    // `-stack_[0].value` is UB when the pattern is INT64_MIN (0x8000..0),
    // which in two's complement negates to itself; unsigned wraparound gives
    // exactly the right bit pattern, and normalize() masks it to the width.
    const quint64 v = static_cast<quint64>(stack_[0].value);
    stack_[0].value = normalize(static_cast<qint64>(0ULL - v), stack_[0].width);
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::enter()
{
    const HistoryEntry before = captureState();
    if (entering_) {
        stack_[0].value = normalize(parseBuffer(), stack_[0].width);
        entering_ = false;
    }
    lift();   // lifted copy into Y keeps X's width
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::rollDown()
{
    const HistoryEntry before = captureState();
    const Reg t = stack_[3];
    stack_[3] = stack_[0];   // T <- X
    stack_[0] = stack_[1];   // X <- Y
    stack_[1] = stack_[2];   // Y <- Z
    stack_[2] = t;           // Z <- T
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::swapXY()
{
    const HistoryEntry before = captureState();
    std::swap(stack_[0], stack_[1]);
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::copyStackToX(int index)
{
    if (index < 1 || index > 3)
        return;
    const HistoryEntry before = captureState();
    if (entering_) {
        stack_[0].value = normalize(parseBuffer(), stack_[0].width);
        entering_ = false;
    }
    const Reg recalled = stack_[index];
    lift();
    stack_[0] = recalled;
    liftOnNext_ = true;
    recordHistory(before);
    refresh();
}

void CalculatorModel::undo()
{
    if (undoStack_.empty())
        return;
    redoStack_.push_back(captureState());
    if (redoStack_.size() > kMaxHistory)
        redoStack_.erase(redoStack_.begin());
    restoreState(undoStack_.back());
    undoStack_.pop_back();
    refresh();
}

void CalculatorModel::redo()
{
    if (redoStack_.empty())
        return;
    undoStack_.push_back(captureState());
    if (undoStack_.size() > kMaxHistory)
        undoStack_.erase(undoStack_.begin());
    restoreState(redoStack_.back());
    redoStack_.pop_back();
    refresh();
}

// ---------------------------------------------------------------------------
// operations
// ---------------------------------------------------------------------------

void CalculatorModel::applyBinaryOp(Op op)
{
    const HistoryEntry before = captureState();
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
    case Op::Rol: {
        const int rot = (yw > 0) ? static_cast<int>(x % static_cast<quint64>(yw)) : 0;
        result = (rot == 0) ? y : (((y << rot) | (y >> (yw - rot))) & mask(yw));
        break;
    }
    case Op::Ror: {
        const int rot = (yw > 0) ? static_cast<int>(x % static_cast<quint64>(yw)) : 0;
        result = (rot == 0) ? y : (((y >> rot) | (y << (yw - rot))) & mask(yw));
        break;
    }
    case Op::Asr: {
        const bool sign = (yw > 0) && (((y >> (yw - 1)) & 1ULL) != 0);
        if (x >= static_cast<quint64>(yw)) {
            result = sign ? mask(yw) : 0ULL;
        } else if (x == 0) {
            result = y;
        } else {
            const int s = static_cast<int>(x);
            const quint64 fill = sign ? (mask(yw) & ~mask(yw - s)) : 0ULL;
            result = (y >> s) | fill;
        }
        break;
    }
    }

    // Result width: shifts & rotates stay in Y's field; everything else widens to the max.
    const bool keepYWidth = (op == Op::Shl || op == Op::Shr ||
                             op == Op::Rol || op == Op::Ror || op == Op::Asr);
    const int rw = keepYWidth ? yw : std::max(xw, yw);
    stack_[0] = Reg{ normalize(static_cast<qint64>(result), rw), rw };
    stack_[1] = stack_[2];
    stack_[2] = stack_[3];
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::applyNot()
{
    const HistoryEntry before = captureState();
    stack_[0].value = normalize(~stack_[0].value, stack_[0].width);
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::toggleBit(int bit)
{
    if (bit < 0 || bit >= stack_[0].width)
        return;
    const HistoryEntry before = captureState();
    const quint64 m = 1ULL << bit;
    stack_[0].value = normalize(static_cast<qint64>(static_cast<quint64>(stack_[0].value) ^ m),
                                stack_[0].width);
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::shiftLeft()
{
    const HistoryEntry before = captureState();
    // X = X << 1, then physically truncate the high bits past X's width
    // (e.g. 0x80 << 1 in 8-bit becomes 0x00). Operates in place on X.
    const int w = stack_[0].width;
    const quint64 x = xRegister();
    stack_[0].value = normalize(static_cast<qint64>((x << 1) & mask(w)), w);
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::shiftRight()
{
    const HistoryEntry before = captureState();
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
    recordHistory(before);
    refresh();
}

// ---------------------------------------------------------------------------
// hardware-grade operations
// ---------------------------------------------------------------------------

void CalculatorModel::concatenate()
{
    const HistoryEntry before = captureState();
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
    recordHistory(before);
    refresh();
}

void CalculatorModel::replicate()
{
    const HistoryEntry before = captureState();
    // {N{X}} with N = Y: X repeated N times → result.width = N * X.width (capped 64).
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int w = stack_[0].width;
    const quint64 xv = static_cast<quint64>(stack_[0].value) & mask(w);
    const quint64 N = static_cast<quint64>(stack_[1].value);
    if (N == 0)
        return;   // {0{X}} — empty replication is invalid; leave the stack untouched
    const quint64 total = N * static_cast<quint64>(w);
    // total == 0 with N > 0 means N*w wrapped 2^64 (an astronomically large N) —
    // cap at 64 like any other overflow.
    const int rw = (total >= 64 || total == 0) ? 64 : static_cast<int>(total);
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
    recordHistory(before);
    refresh();
}

void CalculatorModel::byteSwap()
{
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int w = stack_[0].width;
    const int nbytes = (w + 7) / 8;
    const quint64 xv = xRegister();
    quint64 rv = 0;
    for (int i = 0; i < nbytes; ++i) {
        const quint64 b = (xv >> (8 * i)) & 0xFFULL;
        rv |= b << (8 * (nbytes - 1 - i));
    }
    stack_[0].value = normalize(static_cast<qint64>(rv), w);
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::bitReverse()
{
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int w = stack_[0].width;
    const quint64 xv = xRegister();
    quint64 rv = 0;
    for (int i = 0; i < w; ++i) {
        if ((xv >> i) & 1ULL)
            rv |= (1ULL << (w - 1 - i));
    }
    stack_[0].value = normalize(static_cast<qint64>(rv), w);
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::makeMask()
{
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int w = stack_[0].width;
    const quint64 n = xRegister();
    const quint64 rv = (n >= static_cast<quint64>(w)) ? mask(w) : mask(static_cast<int>(n));
    stack_[0].value = normalize(static_cast<qint64>(rv), w);
    liftOnNext_ = false;
    recordHistory(before);
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
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int newW = hi - lo + 1;
    const quint64 xv = xRegister();
    stack_[0].value = normalize(static_cast<qint64>((xv >> lo) & mask(newW)), newW);
    stack_[0].width = newW;
    entering_ = false;
    liftOnNext_ = false;
    recordHistory(before);
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
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    const int span = hi - lo + 1;
    const quint64 seg = (xRegister() >> lo) & mask(span);
    lift();   // X→Y→Z→T (oldest T lost); X stays as-is
    stack_[1] = Reg{ normalize(static_cast<qint64>(seg), span), span };   // Y = the slice
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::setBitRange(int lo, int hi)
{
    const quint64 rm = rangeMask(lo, hi, stack_[0].width);
    if (rm == 0) return;
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    stack_[0].value = normalize(static_cast<qint64>(xRegister() | rm), stack_[0].width);
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::clearBitRange(int lo, int hi)
{
    const quint64 rm = rangeMask(lo, hi, stack_[0].width);
    if (rm == 0) return;
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    stack_[0].value = normalize(static_cast<qint64>(xRegister() & ~rm), stack_[0].width);
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::invertBitRange(int lo, int hi)
{
    const quint64 rm = rangeMask(lo, hi, stack_[0].width);
    if (rm == 0) return;
    const HistoryEntry before = captureState();
    if (entering_) { stack_[0].value = normalize(parseBuffer(), stack_[0].width); entering_ = false; }
    stack_[0].value = normalize(static_cast<qint64>(xRegister() ^ rm), stack_[0].width);
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

void CalculatorModel::maskBitRange(int lo, int hi)
{
    const quint64 rm = rangeMask(lo, hi, stack_[0].width);
    if (rm == 0) return;
    const HistoryEntry before = captureState();
    entering_ = false;
    stack_[0].value = normalize(static_cast<qint64>(rm), stack_[0].width);
    liftOnNext_ = false;
    recordHistory(before);
    refresh();
}

// ---------------------------------------------------------------------------
// internals
// ---------------------------------------------------------------------------

CalculatorModel::HistoryEntry CalculatorModel::captureState() const
{
    return HistoryEntry{ stack_, buffer_, entering_, liftOnNext_, widthSetting_ };
}

void CalculatorModel::restoreState(const HistoryEntry &entry)
{
    stack_ = entry.stack;
    buffer_ = entry.buffer;
    entering_ = entry.entering;
    liftOnNext_ = entry.liftOnNext;
    widthSetting_ = entry.widthSetting;
}

void CalculatorModel::recordHistory(const HistoryEntry &before)
{
    if (captureState() == before)
        return;
    undoStack_.push_back(before);
    if (undoStack_.size() > kMaxHistory)
        undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

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
    // setBitWidth, clearAll, undo, redo) automatically syncs the View.
    if (stack_[0].width != lastWidth_) {
        lastWidth_ = stack_[0].width;
        emit bitWidthChanged(stack_[0].width);
    }
    emit historyChanged(!undoStack_.empty(), !redoStack_.empty());
    emit displayChanged();
}

quint64 CalculatorModel::rangeMask(int lo, int hi, int width) const
{
    if (lo < 0 || hi < 0 || lo > hi || width <= 0)
        return 0;
    if (hi >= width) hi = width - 1;
    if (lo > hi)
        return 0;
    const int span = hi - lo + 1;
    return mask(span) << lo;
}

QString CalculatorModel::registerTooltip(const Reg &r, const QString &name) const
{
    const quint64 u = static_cast<quint64>(r.value) & mask(r.width);
    const qint64 s = signedWithinWidth(u, r.width);
    const QString hex = formatValue(u, s, Base::Hexadecimal, signed_, r.width);
    const QString dec = formatValue(u, s, Base::Decimal, signed_, r.width);
    const QString oct = formatValue(u, s, Base::Octal, signed_, r.width);
    const QString bin = formatValue(u, s, Base::Binary, signed_, r.width);
    return QStringLiteral("%1 (%2-bit)\nHEX: %3\nDEC: %4\nOCT: %5\nBIN: %6")
        .arg(name).arg(r.width).arg(hex, dec, oct, bin);
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
        // m − 2^width, computed without signed overflow: (2^width − m) is
        // ≤ 2^(width−1), so it fits a qint64 before the negation.
        return -static_cast<qint64>((1ULL << width) - m);
    return twoComplement(m);
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
        qint64 s = twoComplement(u);
        if (isSigned && w < 64 && ((u >> (w - 1)) & 1ULL))
            s = -static_cast<qint64>((1ULL << w) - u);   // sign-extend to w bits
        if (isSigned)
            return QStringLiteral("%1'sd%2").arg(w).arg(QString::number(s));   // e.g. 8'sd-1
        return QStringLiteral("%1'd%2").arg(w).arg(QString::number(u));        // e.g. 8'd255
    }
    }
    return {};
}
