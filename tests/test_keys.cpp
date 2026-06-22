// Headless View-layer coverage of CalculatorView::keyPressEvent dispatch:
// every keystroke must route to the right intent signal, and out-of-base
// keys must be intercepted before reaching the Model. Runs under
// QT_QPA_PLATFORM=offscreen. Events go straight to keyPressEvent via sendEvent
// (offscreen has no real focus/windowing). The keyboard→signal mapping is the
// View's whole job, so this is the layer that owns these regressions.
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QObject>
#include <QSignalSpy>
#include <QTest>

#include "calculatorview.h"

class TestKeys : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        view.reset(new CalculatorView);
        QVERIFY(view != nullptr);
    }
    void init() { view->setActiveBase("DEC"); }   // reset to a permissive base

    // ── digit keys emit digitPressed (upper-cased) ────────────────────────────
    void digitDec()
    {
        QCOMPARE(spiedDigit('5'), QStringLiteral("5"));
    }
    void digitHexUppercased()
    {
        view->setActiveBase("HEX");
        QCOMPARE(spiedDigit('a'), QStringLiteral("A"));   // typed lower -> upper
        QCOMPARE(spiedDigit('f'), QStringLiteral("F"));
    }

    // ── out-of-base keys are intercepted at the keyboard layer ────────────────
    void outOfBaseDigitIntercepted()
    {
        view->setActiveBase("BIN");
        QVERIFY(!digitEmitted('2'));   // 2..9 invalid in BIN
        QVERIFY(!digitEmitted('F'));   // A..F invalid in BIN
        QCOMPARE(spiedDigit('1'), QStringLiteral("1"));   // 0/1 still pass
    }

    // ── operator symbol keys -> binaryOpPressed ───────────────────────────────
    void bitwiseOpSymbols()
    {
        QCOMPARE(spiedOp('&'), QStringLiteral("AND"));
        QCOMPARE(spiedOp('|'), QStringLiteral("OR"));
        QCOMPARE(spiedOp('^'), QStringLiteral("XOR"));
    }
    void shiftOpSymbols()
    {
        QCOMPARE(spiedOp('<'), QStringLiteral("SHL"));
        QCOMPARE(spiedOp('>'), QStringLiteral("SHR"));
    }
    void arithmeticKeys()
    {
        QCOMPARE(spiedOpKey(Qt::Key_Plus),      QStringLiteral("ADD"));
        QCOMPARE(spiedOpKey(Qt::Key_Minus),     QStringLiteral("SUB"));
        QCOMPARE(spiedOpKey(Qt::Key_Asterisk),  QStringLiteral("MUL"));
        QCOMPARE(spiedOpKey(Qt::Key_Slash),     QStringLiteral("DIV"));
        QCOMPARE(spiedOpKey(Qt::Key_Percent),   QStringLiteral("MOD"));
    }

    // ── command keys -> commandPressed ────────────────────────────────────────
    void notCommand()
    {
        QCOMPARE(spiedCommand('~'), QStringLiteral("NOT"));
        QCOMPARE(spiedCommand('!'), QStringLiteral("NOT"));
    }
    void alphaCommands()
    {
        QCOMPARE(spiedCommand('R'), QStringLiteral("ROLL"));
        QCOMPARE(spiedCommand('S'), QStringLiteral("SWAP"));
        QCOMPARE(spiedCommand('N'), QStringLiteral("NEG"));
        QCOMPARE(spiedCommand('_'), QStringLiteral("NEG"));
    }
    void enterKey()
    {
        QSignalSpy spy(view.data(), &CalculatorView::commandPressed);
        sendKey(Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("ENTER"));
    }
    void backspaceKey() { QCOMPARE(spiedCommandKey(Qt::Key_Backspace), QStringLiteral("BSP")); }
    void deleteKey()    { QCOMPARE(spiedCommandKey(Qt::Key_Delete),    QStringLiteral("CLX")); }
    void escapeClearsAll()   // no active selection -> Esc falls through to CLR
    {
        QCOMPARE(spiedCommandKey(Qt::Key_Escape), QStringLiteral("CLR"));
    }

    // ── bit grid: Space toggles the focused bit; arrows move focus (mod 64) ───
    void spaceTogglesDefaultFocusedBit()
    {
        QSignalSpy spy(view.data(), &CalculatorView::bitToggled);
        sendKey(Qt::Key_Space);          // default focus = bit 0
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 0);
    }
    void arrowKeysMoveBitFocus()
    {
        // Left +1 toward MSB, Right -1 toward LSB (wraps 63<->0); Up +16, Down -16.
        sendKey(Qt::Key_Left);
        QCOMPARE(toggledBit(), 1);
        sendKey(Qt::Key_Right);          // 1 -> 0
        sendKey(Qt::Key_Right);          // 0 -> 63 (wrap)
        QCOMPARE(toggledBit(), 63);
        sendKey(Qt::Key_Up);             // 63 + 16 = 15 (mod 64)
        QCOMPARE(toggledBit(), 15);
        sendKey(Qt::Key_Down);           // 15 - 16 = 63 (wrap)
        QCOMPARE(toggledBit(), 63);
    }

private:
    QScopedPointer<CalculatorView> view;

    void sendKey(int qtKey, Qt::KeyboardModifiers mods = Qt::NoModifier) const
    {
        QKeyEvent p(QEvent::KeyPress, qtKey, mods);
        QKeyEvent r(QEvent::KeyRelease, qtKey, mods);
        QCoreApplication::sendEvent(view.data(), &p);
        QCoreApplication::sendEvent(view.data(), &r);
    }
    void sendChar(QChar ch) const
    {
        const int key = ch.toUpper().toLatin1();   // text-bearing key for a glyph
        QKeyEvent p(QEvent::KeyPress, key, Qt::NoModifier, ch);
        QKeyEvent r(QEvent::KeyRelease, key, Qt::NoModifier, ch);
        QCoreApplication::sendEvent(view.data(), &p);
        QCoreApplication::sendEvent(view.data(), &r);
    }
    // one keystroke → the single emitted arg (empty/−1 if not exactly one fire)
    QString spiedDigit(QChar ch) const
    {
        QSignalSpy spy(view.data(), &CalculatorView::digitPressed);
        sendChar(ch);
        return spy.count() == 1 ? spy.takeFirst().at(0).toString() : QString();
    }
    bool digitEmitted(QChar ch) const
    {
        QSignalSpy spy(view.data(), &CalculatorView::digitPressed);
        sendChar(ch);
        return spy.count() > 0;
    }
    QString spiedOp(QChar ch) const
    {
        QSignalSpy spy(view.data(), &CalculatorView::binaryOpPressed);
        sendChar(ch);
        return spy.count() == 1 ? spy.takeFirst().at(0).toString() : QString();
    }
    QString spiedOpKey(int key) const
    {
        QSignalSpy spy(view.data(), &CalculatorView::binaryOpPressed);
        sendKey(key);
        return spy.count() == 1 ? spy.takeFirst().at(0).toString() : QString();
    }
    QString spiedCommand(QChar ch) const
    {
        QSignalSpy spy(view.data(), &CalculatorView::commandPressed);
        sendChar(ch);
        return spy.count() == 1 ? spy.takeFirst().at(0).toString() : QString();
    }
    QString spiedCommandKey(int key) const
    {
        QSignalSpy spy(view.data(), &CalculatorView::commandPressed);
        sendKey(key);
        return spy.count() == 1 ? spy.takeFirst().at(0).toString() : QString();
    }
    int toggledBit() const   // read current focus by toggling (Space doesn't move focus)
    {
        QSignalSpy spy(view.data(), &CalculatorView::bitToggled);
        sendKey(Qt::Key_Space);
        return spy.count() == 1 ? spy.takeFirst().at(0).toInt() : -1;
    }
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestKeys t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_keys.moc"
