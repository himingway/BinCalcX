#pragma once

#include "calculatormodel.h"
#include "calculatorview.h"

#include <QObject>
#include <QSettings>
#include <memory>

/**
 * CalculatorController — the "C" in MVC.
 *
 * Owns no calculation state. It translates the View's high-level intent signals
 * into Model slot calls, refreshes the View on every Model change, and persists
 * the user's preferences (theme, bit width, base, signedness, stay-on-top) to
 * QSettings, restoring them at startup. The View and Model remain unaware of
 * each other.
 */
class CalculatorController : public QObject
{
    Q_OBJECT
public:
    CalculatorController(CalculatorModel &model, CalculatorView &view,
                         QObject *parent = nullptr);

private slots:
    void onDigit(const QString &digit);
    void onCommand(const QString &command);
    void onBinaryOp(const QString &opToken);
    void onBaseSelected(const QString &baseToken);
    void onSignedMode(bool enabled);
    void onBitToggled(int bit);
    void onBitWidthRequested(int width);
    void onBitWidthChanged(int width);
    void onTheme(bool dark);
    void onStayOnTop(bool on);
    void onPaste(const QString &text);
    void refreshView();

private:
    static CalculatorModel::Op   toOp(const QString &token);
    static CalculatorModel::Base toBase(const QString &token);
    static QString baseToken(CalculatorModel::Base base);

    void restorePreferences();

    CalculatorModel &model_;
    CalculatorView  &view_;
    std::unique_ptr<QSettings> settings_;
};
