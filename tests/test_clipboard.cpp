// Headless View-layer coverage for the clipboard: Ctrl+C (copyActiveValue)
// and Ctrl+V (pasteRequested). Runs under QT_QPA_PLATFORM=offscreen so no
// display is needed.
//
// Scope note: the Model's paste parser (loadFromText — prefixes, separators,
// sign, width masking) is already exercised by test_model.cpp. Here we assert
// only the View wiring:  keyboard → clipboard  (copy) and  clipboard → signal
// (paste, passed through verbatim).
//
// Keystrokes are delivered with sendEvent rather than QTest::keyClick so the
// harness does not depend on window exposure / focus dispatch under the
// offscreen platform — they go straight into CalculatorView::keyPressEvent,
// the real entry point for both shortcuts.
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QObject>
#include <QSignalSpy>
#include <QTest>

#include "calculatorview.h"

class TestClipboard : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        view.reset(new CalculatorView);
        QVERIFY(view != nullptr);
        QVERIFY(QGuiApplication::clipboard() != nullptr);   // offscreen has an in-memory clipboard
    }
    void init()   // clean slate before every test
    {
        QGuiApplication::clipboard()->clear();
    }

    // ── Ctrl+C: copy the active-base value into the clipboard ────────────────
    void copyDec()
    {
        view->setActiveBase("DEC");
        view->setDecimalText("255");
        QCOMPARE(triggerCopy(), QStringLiteral("255"));
    }
    void copyDecStripsSeparators()
    {
        view->setActiveBase("DEC");
        view->setDecimalText("1 000");   // grouped display form
        QCOMPARE(triggerCopy(), QStringLiteral("1000"));   // remove(' ')
    }
    void copyHex()
    {
        view->setActiveBase("HEX");
        view->setHexText("DEAD BEEF");
        QCOMPARE(triggerCopy(), QStringLiteral("DEADBEEF"));
    }
    void copyOct()
    {
        view->setActiveBase("OCT");
        view->setOctalText("777");
        QCOMPARE(triggerCopy(), QStringLiteral("777"));
    }
    void copyBin()   // BIN has no text field — value comes from the stored binary string
    {
        view->setActiveBase("BIN");
        view->setBinaryText("1010 1010");
        QCOMPARE(triggerCopy(), QStringLiteral("10101010"));
    }

    // ── leading zeros: HEX/BIN/OCT copy the bare value, not the padded form ──
    void copyHexStripsLeadingZeros()
    {
        view->setActiveBase("HEX");
        view->setHexText("0000 0000 0000 1234");   // width-padded form the Model emits
        QCOMPARE(triggerCopy(), QStringLiteral("1234"));
    }
    void copyHexZeroKeepsOneDigit()
    {
        view->setActiveBase("HEX");
        view->setHexText("0000 0000 0000 0000");
        QCOMPARE(triggerCopy(), QStringLiteral("0"));
    }
    void copyBinStripsLeadingZeros()
    {
        view->setActiveBase("BIN");
        view->setBinaryText("00000000 00000000 00001010");
        QCOMPARE(triggerCopy(), QStringLiteral("1010"));
    }
    void copyBinZeroKeepsOneDigit()
    {
        view->setActiveBase("BIN");
        view->setBinaryText("00000000 00000000");
        QCOMPARE(triggerCopy(), QStringLiteral("0"));
    }
    void copyOctAlsoStripsLeadingZeros()   // OCT has none from the Model; this pins the rule
    {
        view->setActiveBase("OCT");
        view->setOctalText("0017");
        QCOMPARE(triggerCopy(), QStringLiteral("17"));
    }
    void copyDecKeepsLeadingZeros()   // DEC is excluded — never stripped (could carry '-')
    {
        view->setActiveBase("DEC");
        view->setDecimalText("0042");
        QCOMPARE(triggerCopy(), QStringLiteral("0042"));
    }

    // ── Ctrl+V: clipboard text → pasteRequested, passed through verbatim ─────
    void pasteHexPrefix()
    {
        QCOMPARE(triggerPaste("0xFF"), QStringLiteral("0xFF"));
    }
    void pasteDecimal()
    {
        QCOMPARE(triggerPaste("255"), QStringLiteral("255"));
    }
    void pasteKeepsGrouping()   // View never parses; the Model does
    {
        QCOMPARE(triggerPaste("DEAD BEEF"), QStringLiteral("DEAD BEEF"));
    }
    void pasteEmptyIsIgnored()   // !clip.isEmpty() guard in keyPressEvent
    {
        QGuiApplication::clipboard()->setText("");
        QSignalSpy spy(view.data(), &CalculatorView::pasteRequested);
        sendCtrlKey(Qt::Key_V);
        QCOMPARE(spy.count(), 0);
    }

    // ── Real focus path: keystrokes land on a read-only display field ────────
    // These regress the "0xffff won't paste" bug — a read-only QLineEdit used
    // to swallow Ctrl+V (and Ctrl+C copied its raw grouped text). The sendEvent
    // cases above bypass focus and so never caught it.
    void pasteWorksWhenFieldFocused()
    {
        QLineEdit *fld = firstLineEdit();
        QVERIFY(fld != nullptr);
        view->setActiveBase("HEX");
        QGuiApplication::clipboard()->setText("0xffff");
        QSignalSpy spy(view.data(), &CalculatorView::pasteRequested);
        sendCtrlKeyTo(fld, Qt::Key_V);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("0xffff"));
    }
    void copyWorksWhenFieldFocused()
    {
        view->setActiveBase("HEX");
        view->setHexText("DEAD BEEF");
        sendCtrlKeyTo(firstLineEdit(), Qt::Key_C);
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("DEADBEEF"));
    }

private:
    QScopedPointer<CalculatorView> view;

    void sendCtrlKey(int key) const
    {
        QKeyEvent press(QEvent::KeyPress, key, Qt::ControlModifier);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::ControlModifier);
        QCoreApplication::sendEvent(view.data(), &press);
        QCoreApplication::sendEvent(view.data(), &release);
    }
    // Deliver to a specific child the way the real focus path would (so the
    // QLineEdit's eventFilter — not the View's keyPressEvent — is exercised).
    void sendCtrlKeyTo(QWidget *target, int key) const
    {
        QKeyEvent press(QEvent::KeyPress, key, Qt::ControlModifier);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::ControlModifier);
        qApp->notify(target, &press);
        qApp->notify(target, &release);
    }
    QLineEdit *firstLineEdit() const
    {
        const auto list = view->findChildren<QLineEdit *>();
        return list.isEmpty() ? nullptr : list.first();
    }
    // Drive Ctrl+C; copyActiveValue() is private, so we hit it through the
    // public keyPressEvent path and read back what landed on the clipboard.
    QString triggerCopy()
    {
        sendCtrlKey(Qt::Key_C);
        return QGuiApplication::clipboard()->text();
    }
    // Drive Ctrl+V and return the text carried by the emitted pasteRequested.
    QString triggerPaste(const QString &clip)
    {
        QGuiApplication::clipboard()->setText(clip);
        QSignalSpy spy(view.data(), &CalculatorView::pasteRequested);
        sendCtrlKey(Qt::Key_V);
        if (spy.count() != 1)
            return QStringLiteral("<no signal; count=%1>").arg(spy.count());
        return spy.takeFirst().at(0).toString();
    }
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");   // must precede QApplication
    QApplication app(argc, argv);
    TestClipboard t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_clipboard.moc"
