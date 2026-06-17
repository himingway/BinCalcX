#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QSize>
#include <QStyleHints>

#include "calculatorcontroller.h"
#include "calculatormodel.h"
#include "calculatorview.h"

int main(int argc, char *argv[])
{
    // Honour fractional scale factors (1.25/1.5/1.75) exactly. With Qt6's
    // logical-pixel coordinate system, the rest of the UI needs no manual DPI
    // math — every fixed size is logical px and scales automatically.
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setApplicationName("BinCalc");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("BinCalc");

    QApplication app(argc, argv);

    // Force a monospace typeface everywhere — alignment is the core of the
    // geek-tool look. First installed family wins, else Qt's monospace fallback.
    QFont monoFont;
    monoFont.setFamilies({QStringLiteral("JetBrains Mono"),
                          QStringLiteral("Fira Code"),
                          QStringLiteral("Cascadia Code"),
                          QStringLiteral("Consolas"),
                          QStringLiteral("DejaVu Sans Mono"),
                          QStringLiteral("Menlo"),
                          QStringLiteral("Courier New")});
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(9);
    app.setFont(monoFont);

    // App icon — a multi-resolution bit-grid "X" (see assets/app.svg), embedded
    // via assets/app.qrc. Set application-wide so the title bar, taskbar and
    // alt-tab all carry it. On Windows the .exe icon (explorer / start menu) is
    // baked in at build time via assets/app.rc; this is the runtime icon.
    QIcon appIcon;
    for (const int s : {16, 32, 48, 64, 128, 256})
        appIcon.addFile(QStringLiteral(":/icons/app-%1.png").arg(s), QSize(s, s));
    QApplication::setWindowIcon(appIcon);

    CalculatorModel model;
    CalculatorView view;
    CalculatorController controller(model, view);

    // Lock to the exact content size *before* showing, so the window is born
    // non-resizable — the WM never sees a resizable frame. This is a fixed
    // layout tool; dragging to resize is intentionally disabled.
    view.setFixedSize(view.sizeHint());
    view.show();
    return app.exec();
}
