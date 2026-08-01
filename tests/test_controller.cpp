// End-to-end MVC wiring: a real keystroke through CalculatorView's
// keyPressEvent fires its intent signal, the Controller routes it to the
// Model, and the Model's displayChanged refreshes the View. We assert at BOTH
// ends — the Model register (held directly) and the View display field (via
// the read-only getters) — so a broken connect, a misrouted slot, or a stale
// refreshView is caught. Headless under offscreen.
//
// Controller slots are private, so we never call them directly: every case
// drives the public keyPressEvent path the way a user would.
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

#include "calculatorcontroller.h"
#include "calculatormodel.h"
#include "calculatorview.h"

class TestController : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        model = new CalculatorModel;
        view = new CalculatorView;
        controller = new CalculatorController(*model, *view);   // connects all signals
        QVERIFY(QGuiApplication::clipboard() != nullptr);
    }
    void cleanupTestCase() { delete controller; delete view; delete model; }
    void init()   // deterministic start for every case (don't rely on QSettings residue)
    {
        model->clearAll();
        model->setBitWidth(64);
        model->setBase(CalculatorModel::Base::Decimal);
        model->setSignedMode(false);
        view->setActiveBase("DEC");
    }

    // ── paste: clipboard → pasteRequested → onPaste → loadFromText → X ────────
    void pasteLoadsXEndToEnd()
    {
        QGuiApplication::clipboard()->setText("0xff");
        sendCtrlKey(Qt::Key_V);
        QCOMPARE((quint64)model->xRegister(), 0xffull);
        QVERIFY(view->hexText().contains(QLatin1String("FF")));   // refreshView wired HEX field
    }

    // ── digit → onDigit → inputDigit; View decimal field mirrors X ────────────
    void digitEntryEndToEnd()
    {
        sendChar('5');
        QCOMPARE((qint64)model->xRegister(), 5);
        QCOMPARE(view->decimalText(), QStringLiteral("5"));
    }

    // ── 1 ENTER 2 + → onBinaryOp(ADD) → 3 ─────────────────────────────────────
    void arithmeticEndToEnd()
    {
        sendChar('1'); sendKey(Qt::Key_Return);
        sendChar('2');
        sendKey(Qt::Key_Plus);
        QCOMPARE((qint64)model->xRegister(), 3);
    }

    // ── '~' → commandPressed(NOT) → applyNot ──────────────────────────────────
    void notCommandEndToEnd()
    {
        sendChar('1');
        sendChar('~');
        QCOMPARE((quint64)model->xRegister(), (quint64)~1ull);   // ~1 within 64 bits
    }

    // ── Space → bitToggled(focusBit) → toggleBit ──────────────────────────────
    void bitToggleEndToEnd()
    {
        sendKey(Qt::Key_Space);   // focus bit 0
        QCOMPARE((qint64)model->xRegister(), 1);
    }

    // ── Delete → CLX; Esc → CLR ───────────────────────────────────────────────
    void clxClearsXEndToEnd()
    {
        sendKey(Qt::Key_5);
        sendKey(Qt::Key_Delete);
        QCOMPARE((qint64)model->xRegister(), 0);
    }
    void clrClearsAllEndToEnd()
    {
        sendKey(Qt::Key_5);
        sendKey(Qt::Key_Escape);   // no active selection → CLR
        QCOMPARE((qint64)model->xRegister(), 0);
    }

    // ── Model change → displayChanged → refreshView re-renders the View ───────
    void signedDisplayRefreshesEndToEnd()
    {
        // 8-bit 0xFF signed renders as -1 in the decimal field. Width has no key
        // binding, so drive the Model directly and assert the Controller's
        // refreshView propagated it to the View.
        model->setBitWidth(8);
        model->loadFromText("0xFF");
        model->setSignedMode(true);
        QCOMPARE(view->decimalText(), QStringLiteral("-1"));
    }
    void displayChangedEmittedOnEveryChange()
    {
        QSignalSpy spy(model, &CalculatorModel::displayChanged);
        sendChar('7');
        QVERIFY(spy.count() >= 1);
    }

    // ── SIZE: X as a byte count → human-readable binary (IEC) units ────────────
    void sizeDisplayEndToEnd()
    {
        model->loadFromText("0x0");
        QCOMPARE(view->sizeText(), QStringLiteral("0 B"));
        model->loadFromText("0x400");          // 1024
        QCOMPARE(view->sizeText(), QStringLiteral("1 KiB"));
        model->loadFromText("0x4000");         // 16384
        QCOMPARE(view->sizeText(), QStringLiteral("16 KiB"));
        model->loadFromText("0x4001");         // 16385 — clean unit + 1 byte
        QCOMPARE(view->sizeText(), QStringLiteral("16 KiB + 1 B"));
        model->loadFromText("0x20000");        // 131072
        QCOMPARE(view->sizeText(), QStringLiteral("128 KiB"));
        model->loadFromText("0x100000");       // 1 MiB
        QCOMPARE(view->sizeText(), QStringLiteral("1 MiB"));
        model->loadFromText("0x120001");       // 1 MiB + 128 KiB + 1 B
        QCOMPARE(view->sizeText(), QStringLiteral("1 MiB + 128 KiB + 1 B"));
        // "all-ones tail" masks → round − 1 B shorthand (trailing hex F-run ≥ 3)
        model->loadFromText("0x3FF");          // F-run 2 → ordinary form
        QCOMPARE(view->sizeText(), QStringLiteral("1023 B"));
        model->loadFromText("0xFFF");          // 4 KiB − 1 B
        QCOMPARE(view->sizeText(), QStringLiteral("4 KiB - 1 B"));
        model->loadFromText("0xEEFFFFFFF");    // 59 GiB + 768 MiB − 1 B
        QCOMPARE(view->sizeText(), QStringLiteral("59 GiB + 768 MiB - 1 B"));
        model->loadFromText("0xFFFFFFFF");     // 4 GiB − 1 B
        QCOMPARE(view->sizeText(), QStringLiteral("4 GiB - 1 B"));
        model->loadFromText("0xFFFFFFFFFFFFFFFF");  // 16 EiB − 1 B (round value overflows)
        QCOMPARE(view->sizeText(), QStringLiteral("16 EiB - 1 B"));
    }

    // ── width persistence: only the standard 8/16/32/64 widths reach QSettings ─
    void standardWidthIsPersisted()
    {
        model->setBitWidth(8);
        QCOMPARE(QSettings().value("calc/bitWidth", -1).toInt(), 8);
    }
    void sliceWidthIsNotPersisted()
    {
        // Regression: a non-standard width (slice/concat/replicate) must not be
        // saved — setBitWidth() refuses it on restore, but the View would still
        // grey out cells, desyncing the grid from the Model's real width.
        model->loadFromText("0xFF");
        model->applySlice(0, 3);          // X width → 4
        QCOMPARE(model->bitWidth(), 4);
        QCOMPARE(QSettings().value("calc/bitWidth", -1).toInt(), 64);
    }

private:
    CalculatorModel *model = nullptr;
    CalculatorView *view = nullptr;
    CalculatorController *controller = nullptr;

    void sendKey(int qtKey, Qt::KeyboardModifiers mods = Qt::NoModifier) const
    {
        QKeyEvent p(QEvent::KeyPress, qtKey, mods);
        QKeyEvent r(QEvent::KeyRelease, qtKey, mods);
        QCoreApplication::sendEvent(view, &p);
        QCoreApplication::sendEvent(view, &r);
    }
    void sendChar(QChar ch) const
    {
        const int key = ch.toUpper().toLatin1();
        QKeyEvent p(QEvent::KeyPress, key, Qt::NoModifier, ch);
        QKeyEvent r(QEvent::KeyRelease, key, Qt::NoModifier, ch);
        QCoreApplication::sendEvent(view, &p);
        QCoreApplication::sendEvent(view, &r);
    }
    void sendCtrlKey(int qtKey) const { sendKey(qtKey, Qt::ControlModifier); }
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // Isolate QSettings from the real app: the Controller reads prefs in
    // restorePreferences() and writes them in onBitWidthChanged(), and ctest
    // must not clobber the developer's actual BinCalc config. Point the store
    // at a scratch dir and clear it so every run starts deterministic.
    QCoreApplication::setOrganizationName(QStringLiteral("BinCalc"));
    QCoreApplication::setApplicationName(QStringLiteral("BinCalc"));
    const QString settingsDir = QDir::tempPath() + QStringLiteral("/bincalc-test-settings");
    QDir(settingsDir).removeRecursively();
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    TestController t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_controller.moc"
