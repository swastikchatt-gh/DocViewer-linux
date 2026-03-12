#pragma once

#include <QObject>
#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QMutex>
#include <QCache>
#include <QByteArray>

// MuPDF C API
extern "C" {
#include <mupdf/fitz.h>
}

// ─────────────────────────────────────────────────────────────
//  RecentFile — lightweight record kept in the recent-files list
// ─────────────────────────────────────────────────────────────
struct RecentFile {
    QString path;
    QString title;
    int     lastPage  = 0;
    qreal   lastZoom  = 1.0;
};

// ─────────────────────────────────────────────────────────────
//  DocumentHandler
//  Wraps MuPDF: opens documents, renders pages to QImage.
//  Exposed to QML as a context property.
//
//  Thread-safety: renderPage() is safe to call from a worker
//  thread (QThreadPool / QtConcurrent).  All other public
//  methods must be called from the main (GUI) thread.
// ─────────────────────────────────────────────────────────────
class DocumentHandler : public QObject
{
    Q_OBJECT

    // ── QML-visible properties ──────────────────────────────
    Q_PROPERTY(int     pageCount      READ pageCount      NOTIFY documentChanged)
    Q_PROPERTY(int     currentPage    READ currentPage    WRITE  setCurrentPage  NOTIFY currentPageChanged)
    Q_PROPERTY(bool    isLoaded       READ isLoaded       NOTIFY documentChanged)
    Q_PROPERTY(QString documentTitle  READ documentTitle  NOTIFY documentChanged)
    Q_PROPERTY(QString filePath       READ filePath       NOTIFY documentChanged)
    Q_PROPERTY(qreal   zoomLevel      READ zoomLevel      WRITE  setZoomLevel    NOTIFY zoomLevelChanged)
    Q_PROPERTY(bool    loading        READ loading        NOTIFY loadingChanged)

public:
    explicit DocumentHandler(QObject *parent = nullptr);
    ~DocumentHandler() override;

    // ── Property accessors ──────────────────────────────────
    int     pageCount()     const;
    int     currentPage()   const;
    bool    isLoaded()      const;
    QString documentTitle() const;
    QString filePath()      const;
    qreal   zoomLevel()     const;
    bool    loading()       const;

    void setCurrentPage(int page);
    void setZoomLevel(qreal zoom);

    // ── QML-invokable methods ───────────────────────────────
    Q_INVOKABLE bool   openDocument (const QString &urlOrPath);
    Q_INVOKABLE void   closeDocument();
    Q_INVOKABLE void   nextPage();
    Q_INVOKABLE void   previousPage();
    Q_INVOKABLE void   goToPage(int page);

    /// Rotate the view by ±90° increments (0/90/180/270).
    Q_INVOKABLE void   rotate(int degrees);
    Q_INVOKABLE int    rotation() const;

    /// Page size in MuPDF points (1 pt = 1/72 in) at scale 1.0.
    Q_INVOKABLE QSizeF pageSize(int pageNum) const;

    /// Quick check: is the supplied path a file we can try to open?
    Q_INVOKABLE static bool isSupportedFile(const QString &path);

    /// Filter string for file-open dialogs.
    Q_INVOKABLE static QString supportedExtensions();

    // ── Called by PageImageProvider (may run on a render thread) ──
    QImage renderPage(int pageNum, qreal scale, int rotationDeg = 0) const;

signals:
    void documentChanged();
    void currentPageChanged();
    void zoomLevelChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);
    void documentLoaded(const QString &title, int pages);

private:
    void cleanup();
    void setLoading(bool v);

    fz_context  *m_ctx  = nullptr;
    fz_document *m_doc  = nullptr;

    int     m_pageCount   = 0;
    int     m_currentPage = 0;
    int     m_rotation    = 0;      // 0 | 90 | 180 | 270
    bool    m_loading     = false;
    QString m_title;
    QString m_filePath;
    qreal   m_zoomLevel = 1.0;

    // Protects MuPDF context / document during concurrent rendering
    mutable QMutex m_mutex;
};
