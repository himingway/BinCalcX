#include "calculatormodel.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void check(bool ok, const char *what, long long got, long long want)
{
    if (!ok) { ++failures; std::printf("FAIL  %-34s got=%lld want=%lld\n", what, got, want); }
    else      std::printf("ok    %-34s = %lld\n", what, got);
}

int main()
{
    CalculatorModel m;

    // 12 ENTER 5 +  => 17
    m.setBase(CalculatorModel::Base::Decimal);
    m.inputDigit("1"); m.inputDigit("2");
    m.enter();
    m.inputDigit("5");
    m.applyBinaryOp(CalculatorModel::Op::Add);
    check((long long)m.xRegister() == 17, "12 ENTER 5 +", (long long)m.xRegister(), 17);

    // 2 ENTER 3 *  => 6
    m.clearAll();
    m.inputDigit("2"); m.enter(); m.inputDigit("3");
    m.applyBinaryOp(CalculatorModel::Op::Mul);
    check((long long)m.xRegister() == 6, "2 ENTER 3 *", (long long)m.xRegister(), 6);

    // 17 ENTER 5 -  => 12  (y - x)
    m.clearAll();
    m.inputDigit("1"); m.inputDigit("7"); m.enter();
    m.inputDigit("5");
    m.applyBinaryOp(CalculatorModel::Op::Sub);
    check((long long)m.xRegister() == 12, "17 ENTER 5 -", (long long)m.xRegister(), 12);

    // 20 ENTER 7 /  => 2, 20 ENTER 7 MOD => 6
    m.clearAll();
    m.inputDigit("2"); m.inputDigit("0"); m.enter();
    m.inputDigit("7");
    m.applyBinaryOp(CalculatorModel::Op::Div);
    check((long long)m.xRegister() == 2, "20 ENTER 7 /", (long long)m.xRegister(), 2);
    m.clearAll();
    m.inputDigit("2"); m.inputDigit("0"); m.enter();
    m.inputDigit("7");
    m.applyBinaryOp(CalculatorModel::Op::Mod);
    check((long long)m.xRegister() == 6, "20 ENTER 7 MOD", (long long)m.xRegister(), 6);

    // bitwise AND: 0xF0 AND 0x0F => 0
    m.setBase(CalculatorModel::Base::Hexadecimal);
    m.clearAll();
    m.inputDigit("F"); m.inputDigit("0"); m.enter();
    m.inputDigit("0"); m.inputDigit("F");
    m.applyBinaryOp(CalculatorModel::Op::And);
    check((long long)m.xRegister() == 0, "0xF0 AND 0x0F", (long long)m.xRegister(), 0);

    // 0xFF XOR 0x0F => 0xF0
    m.clearAll();
    m.inputDigit("F"); m.inputDigit("F"); m.enter();
    m.inputDigit("0"); m.inputDigit("F");
    m.applyBinaryOp(CalculatorModel::Op::Xor);
    check((long long)m.xRegister() == 0xF0, "0xFF XOR 0x0F", (long long)m.xRegister(), 0xF0);

    // 1 SHL 4 => 16
    m.clearAll();
    m.inputDigit("1"); m.enter();
    m.inputDigit("4");
    m.applyBinaryOp(CalculatorModel::Op::Shl);
    check((long long)m.xRegister() == 16, "1 SHL 4", (long long)m.xRegister(), 16);

    // NOT 0 => all-ones (-1 signed)
    m.clearAll();
    m.applyNot();
    check((long long)m.xRegister() == -1, "NOT 0", (long long)m.xRegister(), -1);

    // toggleBit(0) on 0 => 1 ; toggleBit(3) => 8
    m.clearAll();
    m.toggleBit(0);
    check((long long)m.xRegister() == 1, "toggle bit0", (long long)m.xRegister(), 1);
    m.toggleBit(3);
    check((long long)m.xRegister() == 9, "toggle bit3 -> 9", (long long)m.xRegister(), 9);

    // negate: 5 +/- => -5
    m.clearAll();
    m.inputDigit("5");
    m.negate();
    check((long long)m.xRegister() == -5, "5 NEG", (long long)m.xRegister(), -5);

    // base change preserves value: enter 255 dec, switch to hex => FF
    m.clearAll();
    m.setBase(CalculatorModel::Base::Decimal);
    m.inputDigit("2"); m.inputDigit("5"); m.inputDigit("5");
    m.setBase(CalculatorModel::Base::Hexadecimal);
    check((long long)m.xRegister() == 0xFF, "255 -> hex FF", (long long)m.xRegister(), 0xFF);

    // swap: 7 ENTER 3 then swap => X should become 7
    m.clearAll();
    m.inputDigit("7"); m.enter(); m.inputDigit("3");
    m.swapXY();
    check((long long)m.xRegister() == 7, "swap (X<->Y)", (long long)m.xRegister(), 7);

    // stack display order: T,Z,Y,X — after 2 ENTER 3 +, X(bottom)=5, T=0
    m.clearAll();
    m.inputDigit("2"); m.enter(); m.inputDigit("3");
    m.applyBinaryOp(CalculatorModel::Op::Add);
    auto ss = m.stackStrings();
    check(ss[3].toStdString() == "64'h5", "stack row X verilog 64'h5", 0, 0);
    check(ss[0].toStdString() == "64'h0", "stack row T verilog 64'h0", 0, 0);


    // unsigned display: set value 0xFFFFFFFFFFFFFFFF, signed => -1, unsigned => big
    m.clearAll();
    m.setBase(CalculatorModel::Base::Hexadecimal);
    for (int i = 0; i < 16; ++i) m.inputDigit("F");
    m.setSignedMode(true);
    check(m.decimalString().toStdString() == "-1", "signed -1 str", 0, 0);
    m.setSignedMode(false);
    check(m.decimalString().toStdString() == "18446744073709551615", "unsigned max str", 0, 0);

    // --- bit width: 8-bit mode masks and sign-extends ---
    m.clearAll();
    m.setBitWidth(8);
    m.setBase(CalculatorModel::Base::Hexadecimal);
    m.setSignedMode(true);
    m.inputDigit("F"); m.inputDigit("F");               // X = 0xFF
    check((long long)m.xRegister() == 0xFF, "8-bit: FF kept", (long long)m.xRegister(), 0xFF);
    check(m.decimalString().toStdString() == "-1", "8-bit: FF signed -1", 0, 0);
    m.setSignedMode(false);
    check(m.decimalString().toStdString() == "255", "8-bit: FF unsigned 255", 0, 0);

    // 8-bit wrap: 200 + 100 = 300 & 0xFF = 44
    m.clearAll(); m.setBase(CalculatorModel::Base::Decimal);
    m.inputDigit("2"); m.inputDigit("0"); m.inputDigit("0"); m.enter();
    m.inputDigit("1"); m.inputDigit("0"); m.inputDigit("0");
    m.applyBinaryOp(CalculatorModel::Op::Add);
    check((long long)m.xRegister() == 44, "8-bit wrap 200+100=44", (long long)m.xRegister(), 44);

    // out-of-range bit ignored in 8-bit mode
    m.clearAll(); m.toggleBit(10);
    check((long long)m.xRegister() == 0, "8-bit: toggle bit10 ignored", (long long)m.xRegister(), 0);

    // shift clamp by width: 1 SHL 8 == 0 in 8-bit; 1 SHL 7 == 128
    m.clearAll(); m.inputDigit("1"); m.enter();
    m.inputDigit("8"); m.applyBinaryOp(CalculatorModel::Op::Shl);
    check((long long)m.xRegister() == 0, "8-bit: 1 SHL 8 clamped", (long long)m.xRegister(), 0);
    m.clearAll(); m.inputDigit("1"); m.enter();
    m.inputDigit("7"); m.applyBinaryOp(CalculatorModel::Op::Shl);
    check((long long)m.xRegister() == 128, "8-bit: 1 SHL 7 = 128", (long long)m.xRegister(), 128);

    // 16-bit sign extension
    m.clearAll(); m.setBitWidth(16);
    m.setBase(CalculatorModel::Base::Hexadecimal); m.setSignedMode(true);
    m.inputDigit("F"); m.inputDigit("F"); m.inputDigit("F"); m.inputDigit("F");
    check(m.decimalString().toStdString() == "-1", "16-bit: FFFF signed -1", 0, 0);

    // --- clipboard: loadFromText — prefixes, separators, sign, width masking ---
    // (capture ok/value separately: arg-eval order would otherwise print a
    //  stale register alongside a passing check.)
    m.setBitWidth(64);
    m.setBase(CalculatorModel::Base::Decimal);
    { const bool ok = m.loadFromText("0xFF");     const long long g = (long long)m.xRegister();
      check(ok && g == 0xFF, "paste 0xFF -> 255", g, 0xFF); }
    { const bool ok = m.loadFromText("0b1010");   const long long g = (long long)m.xRegister();
      check(ok && g == 10, "paste 0b1010 -> 10", g, 10); }
    { const bool ok = m.loadFromText("1,000");    const long long g = (long long)m.xRegister();
      check(ok && g == 1000, "paste '1,000' -> 1000", g, 1000); }
    m.setBase(CalculatorModel::Base::Hexadecimal);
    { const bool ok = m.loadFromText("DEAD BEEF"); const long long g = (long long)m.xRegister();
      check(ok && g == 0xDEADBEEF, "paste 'DEAD BEEF' -> DEADBEEF", g, 0xDEADBEEF); }
    m.setBitWidth(8);
    { const bool ok = m.loadFromText("-1");       const long long g = (long long)m.xRegister();
      check(ok && g == 0xFF, "paste -1 (8-bit) -> FF", g, 0xFF); }
    { const bool ok = m.loadFromText("0x1FF");    const long long g = (long long)m.xRegister();
      check(ok && g == 0xFF, "paste 0x1FF masked 8-bit -> FF", g, 0xFF); }
    check(!m.loadFromText("nope"), "paste invalid -> rejected", 0, 0);

    // --- in-place ◀ ▶ single-bit shifts on X (distinct from SHL/SHR ops) ---
    m.setBitWidth(64);
    m.setBase(CalculatorModel::Base::Hexadecimal);
    m.setSignedMode(false);

    // left shift by 1: 0x1 -> 0x2
    m.clearAll(); m.loadFromText("0x1");
    m.shiftLeft();
    check((long long)m.xRegister() == 2, "shiftLeft: 0x1 -> 0x2", (long long)m.xRegister(), 2);

    // left shift truncates to the width: 0x80 (8-bit) -> 0x00
    m.setBitWidth(8);
    m.clearAll(); m.loadFromText("0x80");
    m.shiftLeft();
    check((long long)m.xRegister() == 0, "shiftLeft 8-bit: 0x80 -> 0x00", (long long)m.xRegister(), 0);

    // right shift, unsigned (logical): 0x80 (8-bit) -> 0x40
    m.clearAll(); m.loadFromText("0x80");
    m.shiftRight();
    check((long long)m.xRegister() == 0x40, "shiftRight logical 8-bit: 0x80 -> 0x40", (long long)m.xRegister(), 0x40);

    // right shift, signed (arithmetic): 0x80 (8-bit, -128) -> 0xC0 (-64)
    m.setSignedMode(true);
    m.clearAll(); m.loadFromText("0x80");
    m.shiftRight();
    check((long long)m.xRegister() == 0xC0, "shiftRight arith 8-bit: 0x80 -> 0xC0", (long long)m.xRegister(), 0xC0);

    // sign extension keeps -1: 0xFF (8-bit) -> 0xFF
    m.clearAll(); m.loadFromText("0xFF");
    m.shiftRight();
    check((long long)m.xRegister() == 0xFF, "shiftRight arith 8-bit: 0xFF -> 0xFF", (long long)m.xRegister(), 0xFF);
    m.setSignedMode(false);

    // --- CHR: full 8-byte (64-bit) decode, MSB first, '.' placeholders ---
    m.setBitWidth(64);
    m.setBase(CalculatorModel::Base::Decimal);
    { m.clearAll(); m.loadFromText("0x41");
      check(m.charString().toStdString() == ".......A", "CHR: 0x41 -> .......A", 0, 0); }
    { m.clearAll(); m.loadFromText("0x4445414442454546");
      check(m.charString().toStdString() == "DEADBEEF", "CHR: 0x44..46 -> DEADBEEF", 0, 0); }
    // narrow width: high bytes are masked to 0 -> placeholders
    m.setBitWidth(8);
    { m.clearAll(); m.loadFromText("0x41");
      check(m.charString().toStdString() == ".......A", "CHR 8-bit: 0x41 -> .......A", 0, 0); }

    m.setBitWidth(64);   // tidy restore

    // --- decimal overflow: typing past the 64-bit max must NOT wrap to 0 ---
    m.clearAll(); m.setBase(CalculatorModel::Base::Decimal);
    for (int i = 0; i < 30; ++i) m.inputDigit("9");   // 20 nines would overflow
    check((unsigned long long)m.xRegister() == 9999999999999999999ULL,
          "20 nines -> 19 nines (no wrap to 0)", 0, 0);
    // the exact max is still typable
    m.clearAll();
    for (char ch : {'1','8','4','4','6','7','4','4','0','7','3','7','0','9','5','5','1','6','1','5'})
        m.inputDigit(QString(QChar(ch)));
    check(((unsigned long long)m.xRegister()) == 0xFFFFFFFFFFFFFFFFull,
          "64-bit max 18446744073709551615 typable", 0, 0);
    // one more digit (max+1) is rejected
    m.inputDigit("6");
    check(((unsigned long long)m.xRegister()) == 0xFFFFFFFFFFFFFFFFull,
          "max+1 rejected (stays at max)", 0, 0);
    // octal overflow: the octal max is typable, one more digit rejected
    m.clearAll(); m.setBase(CalculatorModel::Base::Octal);
    m.inputDigit("1");
    for (int i = 0; i < 21; ++i) m.inputDigit("7");   // 0o177...7 (22 digits) = max
    check(((unsigned long long)m.xRegister()) == 0xFFFFFFFFFFFFFFFFull, "octal max typable", 0, 0);
    m.inputDigit("7");   // 23 digits — overflow, rejected
    check(((unsigned long long)m.xRegister()) == 0xFFFFFFFFFFFFFFFFull, "octal max+1 rejected", 0, 0);

    // --- hardware-grade ops: reductions, concat, replicate, slice, Verilog ---
    // (arbitrary widths come from applySlice — setBitWidth only does 8/16/32/64)

    // concat {8'h0F, 4'h3} = 12'h0F3
    m.clearAll(); m.setBitWidth(8); m.setBase(CalculatorModel::Base::Hexadecimal);
    m.loadFromText("0x0F"); m.enter();                  // Y = 8'h0F
    m.loadFromText("0x3"); m.applySlice(0, 3);          // X = 4'h3
    m.concatenate();
    check((long long)m.xRegister() == 0x0F3, "concat {0F,3} = 0F3", (long long)m.xRegister(), 0x0F3);
    check(m.bitWidth() == 12, "concat width=12", m.bitWidth(), 12);

    // replicate {3{4'b0011}} = 12'b001100110011 (0x333)
    m.clearAll(); m.setBitWidth(64); m.setBase(CalculatorModel::Base::Decimal);
    m.loadFromText("3"); m.enter();                     // Y = 3 (count)
    m.loadFromText("0b0011"); m.applySlice(0, 3);       // X = 4'b0011
    m.replicate();
    check((long long)m.xRegister() == 0x333, "replicate {3{0011}} = 0x333", (long long)m.xRegister(), 0x333);
    check(m.bitWidth() == 12, "replicate width=12", m.bitWidth(), 12);

    // applySlice: bits [2..5] of 8'hFF -> 4'hF, width 4
    m.clearAll(); m.setBitWidth(8);
    m.loadFromText("0xFF");
    m.applySlice(2, 5);   // (0xFF>>2) & mask(4) = 0x3F & 0xF = 0xF
    check((long long)m.xRegister() == 0xF, "slice [2..5] of FF -> F", (long long)m.xRegister(), 0xF);
    check(m.bitWidth() == 4, "slice width=4", m.bitWidth(), 4);

    // per-register width survives ENTER (Y keeps X's width)
    m.clearAll(); m.setBitWidth(8); m.setBase(CalculatorModel::Base::Hexadecimal);
    m.loadFromText("0xFF"); m.enter();
    { auto s = m.stackStrings(); check(s[2] == QStringLiteral("8'hFF"), "Y keeps width 8 after ENTER", 0, 0); }

    // Verilog literals across bases (X = 8'hFF)
    m.clearAll(); m.setBitWidth(8); m.loadFromText("0xFF");
    m.setBase(CalculatorModel::Base::Hexadecimal);
    { auto s = m.stackStrings(); check(s[3] == QStringLiteral("8'hFF"), "verilog 8'hFF", 0, 0); }
    m.setBase(CalculatorModel::Base::Binary);
    { auto s = m.stackStrings(); check(s[3] == QStringLiteral("8'b11111111"), "verilog 8'b11111111", 0, 0); }
    m.setBase(CalculatorModel::Base::Decimal); m.setSignedMode(true);
    { auto s = m.stackStrings(); check(s[3] == QStringLiteral("8'sd-1"), "verilog signed 8'sd-1", 0, 0); }
    m.setSignedMode(false);
    { auto s = m.stackStrings(); check(s[3] == QStringLiteral("8'd255"), "verilog unsigned 8'd255", 0, 0); }

    std::printf("\n%s (%d failures)\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
