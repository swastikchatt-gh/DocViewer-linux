#include <QApplication>
#include <QSurfaceFormat>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include "mainwindow.hpp"

int main(int argc, char *argv[])
{
    // ── High-DPI policy ──────────────────────────────────────
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // ── OpenGL surface (4× MSAA for crisp text rendering) ───
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    // ── Qt application ───────────────────────────────────────
    QApplication app(argc, argv);
    app.setApplicationName   ("DocViewer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName  ("DocViewer");
    app.setOrganizationDomain("docviewer.local");

    // ── CLI argument parsing ─────────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Lightweight document viewer powered by MuPDF and Qt 6 QML.\n"
        "Supports PDF, XPS, EPUB, MOBI, FB2, CBZ, SVG, PNG, JPEG, TIFF, PS, and more.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        "file",
        "Document to open on startup.",
        "[file]");
    parser.process(app);

    // ── Create and show the main window ─────────────────────
    MainWindow mainWindow;
    mainWindow.show();

    // ── Open file from command line (if provided) ────────────
    // Use a short delay so the QML engine has finished initialising
    // and the DocumentHandler's signals are fully connected to QML.
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        const QString absPath = QFileInfo(args.first()).absoluteFilePath();
        QTimer::singleShot(100, [&mainWindow, absPath]() {
            mainWindow.openFile(absPath);
        });
    }

    return app.exec();
}
