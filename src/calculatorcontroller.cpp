#include "calculatorcontroller.h"

#include <QSettings>

CalculatorController::CalculatorController(CalculatorModel &model,
                                           CalculatorView &view,
                                           QObject *parent)
    : QObject(parent)
    , model_(model)
    , view_(view)
    , settings_(std::make_unique<QSettings>())
{
    // View intents -> Model actions
    connect(&view_, &CalculatorView::digitPressed,      this, &CalculatorController::onDigit);
    connect(&view_, &CalculatorView::commandPressed,    this, &CalculatorController::onCommand);
    connect(&view_, &CalculatorView::binaryOpPressed,   this, &CalculatorController::onBinaryOp);
    connect(&view_, &CalculatorView::baseSelected,      this, &CalculatorController::onBaseSelected);
    connect(&view_, &CalculatorView::signedModeChanged, this, &CalculatorController::onSignedMode);
    connect(&view_, &CalculatorView::bitToggled,        this, &CalculatorController::onBitToggled);
    connect(&view_, &CalculatorView::bitWidthRequested, this, &CalculatorController::onBitWidthRequested);
    connect(&view_, &CalculatorView::themeToggled,      this, &CalculatorController::onTheme);
    connect(&view_, &CalculatorView::stayOnTopToggled,  this, &CalculatorController::onStayOnTop);

    // Model changes -> View refresh
    connect(&model_, &CalculatorModel::displayChanged,  this, &CalculatorController::refreshView);
    connect(&model_, &CalculatorModel::modeChanged,     this, &CalculatorController::refreshView);
    connect(&model_, &CalculatorModel::bitWidthChanged, this, &CalculatorController::onBitWidthChanged);

    restorePreferences();
    refreshView();
}

// ---------------------------------------------------------------------------
// View intent -> Model action
// ---------------------------------------------------------------------------

void CalculatorController::onDigit(const QString &digit)
{
    model_.inputDigit(digit);
}

void CalculatorController::onCommand(const QString &command)
{
    if (command == "ENTER")      model_.enter();
    else if (command == "CLX")   model_.clearX();
    else if (command == "CLR")   model_.clearAll();
    else if (command == "NEG")   model_.negate();
    else if (command == "NOT")   model_.applyNot();
    else if (command == "ROLL")  model_.rollDown();
    else if (command == "SWAP")  model_.swapXY();
    else if (command == "BSP")   model_.backspace();
}

void CalculatorController::onBinaryOp(const QString &opToken)
{
    model_.applyBinaryOp(toOp(opToken));
}

void CalculatorController::onBaseSelected(const QString &baseToken)
{
    const CalculatorModel::Base base = toBase(baseToken);
    model_.setBase(base);
    view_.setActiveBase(baseToken);
    settings_->setValue("calc/base", static_cast<int>(base));
}

void CalculatorController::onSignedMode(bool enabled)
{
    model_.setSignedMode(enabled);
    settings_->setValue("calc/signed", enabled);
}

void CalculatorController::onBitToggled(int bit)
{
    model_.toggleBit(bit);
}

void CalculatorController::onBitWidthRequested(int width)
{
    model_.setBitWidth(width);   // model emits bitWidthChanged -> onBitWidthChanged
}

void CalculatorController::onBitWidthChanged(int width)
{
    view_.setActiveWidth(width);     // grey out high cells >= width (no rebuild)
    refreshView();                   // refreshBits() applies the new width
    settings_->setValue("calc/bitWidth", width);
}

void CalculatorController::onTheme(bool dark)
{
    settings_->setValue("ui/theme", dark);
}

void CalculatorController::onStayOnTop(bool on)
{
    settings_->setValue("ui/stayOnTop", on);
}

// ---------------------------------------------------------------------------
// Model state -> View
// ---------------------------------------------------------------------------

void CalculatorController::refreshView()
{
    view_.setHexText(model_.hexadecimalString());
    view_.setOctalText(model_.octalString());
    view_.setDecimalText(model_.decimalString());
    view_.setCharText(model_.charString());
    view_.setStackValues(model_.stackStrings());
    view_.refreshBits(model_.xRegister());
    view_.setSignedMode(model_.signedMode());
}

// ---------------------------------------------------------------------------
// persistence
// ---------------------------------------------------------------------------

void CalculatorController::restorePreferences()
{
    // theme (dark by default)
    view_.setTheme(settings_->value("ui/theme", true).toBool());

    // bit width (64 by default)
    const int width = settings_->value("calc/bitWidth", 64).toInt();
    model_.setBitWidth(width);
    view_.setActiveWidth(width);          // force-sync even if model skips emit

    // number base + signedness
    const int baseInt = settings_->value("calc/base",
                                         static_cast<int>(CalculatorModel::Base::Decimal)).toInt();
    model_.setBase(static_cast<CalculatorModel::Base>(baseInt));
    view_.setActiveBase(baseToken(static_cast<CalculatorModel::Base>(baseInt)));
    model_.setSignedMode(settings_->value("calc/signed", true).toBool());

    // window
    view_.setStayOnTop(settings_->value("ui/stayOnTop", false).toBool());
}

// ---------------------------------------------------------------------------
// token <-> enum
// ---------------------------------------------------------------------------

CalculatorModel::Op CalculatorController::toOp(const QString &token)
{
    if (token == "ADD") return CalculatorModel::Op::Add;
    if (token == "SUB") return CalculatorModel::Op::Sub;
    if (token == "MUL") return CalculatorModel::Op::Mul;
    if (token == "DIV") return CalculatorModel::Op::Div;
    if (token == "MOD") return CalculatorModel::Op::Mod;
    if (token == "AND") return CalculatorModel::Op::And;
    if (token == "OR")  return CalculatorModel::Op::Or;
    if (token == "XOR") return CalculatorModel::Op::Xor;
    if (token == "SHL") return CalculatorModel::Op::Shl;
    if (token == "SHR") return CalculatorModel::Op::Shr;
    return CalculatorModel::Op::Add;
}

CalculatorModel::Base CalculatorController::toBase(const QString &token)
{
    if (token == "HEX") return CalculatorModel::Base::Hexadecimal;
    if (token == "DEC") return CalculatorModel::Base::Decimal;
    if (token == "OCT") return CalculatorModel::Base::Octal;
    return CalculatorModel::Base::Binary;
}

QString CalculatorController::baseToken(CalculatorModel::Base base)
{
    switch (base) {
    case CalculatorModel::Base::Hexadecimal: return "HEX";
    case CalculatorModel::Base::Decimal:     return "DEC";
    case CalculatorModel::Base::Octal:       return "OCT";
    case CalculatorModel::Base::Binary:      return "BIN";
    }
    return "DEC";
}
