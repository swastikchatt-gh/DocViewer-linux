#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QStringList>
#include <QSettings>
#include "file_format.hpp"

// ─────────────────────────────────────────────────────────────
//  PageImageProvider
//  Renders document pages on demand for QML Image elements.
//  URL scheme:  image://pages/<pageNum>/<zoom>/<rotation>/<version>
//
//  The <version> component is a monotonically increasing counter
//  bumped whenever the document changes — this forces Qt's image
//  cache to re-request the new render instead of serving a stale
//  cached copy.
// ─────────────────────────────────────────────────────────────
class PageImageProvider : public QQuickImageProvider
{
public:
    explicit PageImageProvider(DocumentHandler *handler);

    // Called by the QML engine (potentially on a render thread)
    QImage requestImage(const QString &id,
                        QSize         *size,
                        const QSize   &requestedSize) override;

private:
    DocumentHandler *m_handler;   // not owned; lifetime guaranteed by MainWindow
};


// ─────────────────────────────────────────────────────────────
//  RecentFilesModel
//  A tiny QObject that tracks recently-opened paths and exposes
//  them to QML as a simple string-list property.
// ─────────────────────────────────────────────────────────────
class RecentFilesModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList paths READ paths NOTIFY pathsChanged)

public:
    static constexpr int kMaxRecent = 10;

    explicit RecentFilesModel(QObject *parent = nullptr);

    QStringList paths() const;

    Q_INVOKABLE void  add   (const QString &path);
    Q_INVOKABLE void  remove(const QString &path);
    Q_INVOKABLE void  clear ();

signals:
    void pathsChanged();

private:
    void load();
    void save() const;

    QStringList m_paths;
    QSettings   m_settings;
};


// ─────────────────────────────────────────────────────────────
//  MainWindow
//  Owns the QML engine, DocumentHandler, PageImageProvider,
//  and the RecentFilesModel.  Loads all QML from an embedded
//  string and shows the application window.
// ─────────────────────────────────────────────────────────────
class MainWindow : public QObject
{
    Q_OBJECT
public:
    explicit MainWindow(QObject *parent = nullptr);
    ~MainWindow() override = default;

    /// Loads the embedded QML source and displays the window.
    void show();

    /// Open a file path programmatically (e.g. from argv or drag-drop).
    void openFile(const QString &path);

private:
    /// Returns the full QML source as a static string literal.
    static const char *qmlSource();

    // ── Destruction order is significant:
    //    m_engine holds a raw pointer to the image provider but does NOT
    //    own m_handler, so m_handler must outlive m_engine.
    //    Declare m_handler first → it is destroyed last.
    DocumentHandler        m_handler;
    RecentFilesModel       m_recent;
    QQmlApplicationEngine  m_engine;
};
