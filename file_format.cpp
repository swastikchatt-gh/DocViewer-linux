#include "file_format.hpp"

#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QMutexLocker>
#include <cstring>
#include <algorithm>

// ─────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────

static constexpr qreal kZoomMin = 0.10;
static constexpr qreal kZoomMax = 8.00;

// ─────────────────────────────────────────────────────────────
//  Construction / destruction
// ─────────────────────────────────────────────────────────────

DocumentHandler::DocumentHandler(QObject *parent)
    : QObject(parent)
{
    m_ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!m_ctx) {
        qCritical() << "[DocViewer] Fatal: failed to create MuPDF context";
        return;
    }
    fz_register_document_handlers(m_ctx);
    qDebug() << "[DocViewer] MuPDF context ready";
}

DocumentHandler::~DocumentHandler()
{
    cleanup();
    if (m_ctx) {
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────

void DocumentHandler::cleanup()
{
    QMutexLocker lock(&m_mutex);
    if (m_doc && m_ctx) {
        fz_drop_document(m_ctx, m_doc);
        m_doc = nullptr;
    }
    m_pageCount   = 0;
    m_currentPage = 0;
    m_rotation    = 0;
    m_title.clear();
    m_filePath.clear();
}

void DocumentHandler::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

// ─────────────────────────────────────────────────────────────
//  Property accessors
// ─────────────────────────────────────────────────────────────

int     DocumentHandler::pageCount()     const { return m_pageCount;      }
int     DocumentHandler::currentPage()   const { return m_currentPage;    }
bool    DocumentHandler::isLoaded()      const { return m_doc != nullptr; }
QString DocumentHandler::documentTitle() const { return m_title;          }
QString DocumentHandler::filePath()      const { return m_filePath;       }
qreal   DocumentHandler::zoomLevel()     const { return m_zoomLevel;      }
bool    DocumentHandler::loading()       const { return m_loading;        }
int     DocumentHandler::rotation()      const { return m_rotation;       }

void DocumentHandler::setCurrentPage(int page)
{
    if (!m_doc || page == m_currentPage) return;
    page = std::clamp(page, 0, m_pageCount - 1);
    m_currentPage = page;
    emit currentPageChanged();
}

void DocumentHandler::setZoomLevel(qreal zoom)
{
    zoom = std::clamp(zoom, kZoomMin, kZoomMax);
    if (qFuzzyCompare(zoom, m_zoomLevel)) return;
    m_zoomLevel = zoom;
    emit zoomLevelChanged();
}

// ─────────────────────────────────────────────────────────────
//  Rotation
// ─────────────────────────────────────────────────────────────

void DocumentHandler::rotate(int degrees)
{
    m_rotation = ((m_rotation + degrees) % 360 + 360) % 360;
    emit documentChanged();
}

// ─────────────────────────────────────────────────────────────
//  Document open / close
// ─────────────────────────────────────────────────────────────

bool DocumentHandler::openDocument(const QString &urlOrPath)
{
    if (!m_ctx) {
        emit errorOccurred("MuPDF context is not initialised");
        return false;
    }

    // Accept both file:// URLs and plain paths
    QString localPath;
    {
        QUrl url(urlOrPath);
        localPath = url.isLocalFile() ? url.toLocalFile() : urlOrPath;
    }
    localPath = localPath.trimmed();

    if (localPath.isEmpty()) {
        emit errorOccurred("Invalid file path: " + urlOrPath);
        return false;
    }
    if (!QFileInfo::exists(localPath)) {
        emit errorOccurred("File not found: " + localPath);
        return false;
    }

    setLoading(true);
    cleanup();   // drop any previously open document

    // ── volatile: prevents -Wclobbered across MuPDF's setjmp boundary ──
    volatile bool success = false;
    // Collect any error message outside the MuPDF exception scope so we
    // can emit signals after the mutex is released.
    QString pendingError;

    {
        QMutexLocker lock(&m_mutex);

        fz_document *newDoc = nullptr;
        fz_var(newDoc);

        fz_try(m_ctx)
        {
            newDoc = fz_open_document(m_ctx, localPath.toLocal8Bit().constData());

            // ── Password check ───────────────────────────────
            if (fz_needs_password(m_ctx, newDoc)) {
                // Try the empty password first; real password UI could go here.
                if (!fz_authenticate_password(m_ctx, newDoc, "")) {
                    pendingError = "Document is password-protected: "
                                   + QFileInfo(localPath).fileName();
                    // newDoc will be released in fz_always below
                    fz_throw(m_ctx, FZ_ERROR_GENERIC, "password required");
                }
            }

            // ── Page count ───────────────────────────────────
            const int pages = fz_count_pages(m_ctx, newDoc);
            if (pages <= 0) {
                pendingError = "Document contains no pages: "
                               + QFileInfo(localPath).fileName();
                fz_throw(m_ctx, FZ_ERROR_GENERIC, "no pages");
            }

            // ── Metadata ─────────────────────────────────────
            char metaBuf[512] = {};
            fz_lookup_metadata(m_ctx, newDoc,
                               "info:Title", metaBuf, sizeof(metaBuf));
            if (metaBuf[0] != '\0')
                m_title = QString::fromUtf8(metaBuf).trimmed();
            if (m_title.isEmpty())
                m_title = QFileInfo(localPath).completeBaseName();

            m_pageCount   = pages;
            m_filePath    = localPath;
            m_currentPage = 0;
            m_doc         = newDoc;
            newDoc        = nullptr;   // ownership transferred
            success       = true;
        }
        fz_always(m_ctx)
        {
            // Runs whether or not an exception was thrown
            if (newDoc) {
                fz_drop_document(m_ctx, newDoc);
                newDoc = nullptr;
            }
        }
        fz_catch(m_ctx)
        {
            // Only fill pendingError if it wasn't already set above
            if (pendingError.isEmpty()) {
                pendingError =
                    QString(R"(Cannot open "%1": %2)")
                        .arg(QFileInfo(localPath).fileName(),
                             QString::fromUtf8(fz_caught_message(m_ctx)));
            }
            m_pageCount = 0;
        }
    } // ── mutex released ──

    // Emit signals outside the lock so QML slots can call back safely
    setLoading(false);

    if (!pendingError.isEmpty())
        emit errorOccurred(pendingError);

    emit documentChanged();
    emit currentPageChanged();

    if (success)
        emit documentLoaded(m_title, m_pageCount);

    return static_cast<bool>(success);
}

void DocumentHandler::closeDocument()
{
    cleanup();
    emit documentChanged();
    emit currentPageChanged();
}

// ─────────────────────────────────────────────────────────────
//  Navigation
// ─────────────────────────────────────────────────────────────

void DocumentHandler::nextPage()
{
    if (m_doc && m_currentPage < m_pageCount - 1)
        setCurrentPage(m_currentPage + 1);
}

void DocumentHandler::previousPage()
{
    if (m_doc && m_currentPage > 0)
        setCurrentPage(m_currentPage - 1);
}

void DocumentHandler::goToPage(int page)
{
    setCurrentPage(page);
}

// ─────────────────────────────────────────────────────────────
//  Page geometry
// ─────────────────────────────────────────────────────────────

QSizeF DocumentHandler::pageSize(int pageNum) const
{
    if (!m_ctx || !m_doc || pageNum < 0 || pageNum >= m_pageCount)
        return {};

    QMutexLocker lock(&m_mutex);
    QSizeF result;

    fz_try(m_ctx) {
        fz_page *page = fz_load_page(m_ctx, m_doc, pageNum);
        fz_rect  rect = fz_bound_page(m_ctx, page);
        fz_drop_page(m_ctx, page);
        result = { double(rect.x1 - rect.x0), double(rect.y1 - rect.y0) };
    }
    fz_catch(m_ctx) {
        qWarning() << "[DocViewer] pageSize error:" << fz_caught_message(m_ctx);
    }

    return result;
}

// ─────────────────────────────────────────────────────────────
//  Rendering
// ─────────────────────────────────────────────────────────────

QImage DocumentHandler::renderPage(int pageNum, qreal scale, int rotationDeg) const
{
    if (!m_ctx || !m_doc || pageNum < 0 || pageNum >= m_pageCount)
        return {};

    const float s = float(std::clamp(scale, kZoomMin, kZoomMax));

    QMutexLocker lock(&m_mutex);

    fz_page   *page   = nullptr;
    fz_pixmap *pixmap = nullptr;
    fz_device *dev    = nullptr;
    QImage     result;

    fz_var(page);
    fz_var(pixmap);
    fz_var(dev);

    fz_try(m_ctx) {
        page = fz_load_page(m_ctx, m_doc, pageNum);

        fz_matrix matrix = fz_scale(s, s);
        if (rotationDeg != 0)
            matrix = fz_concat(matrix, fz_rotate(float(rotationDeg)));

        const fz_rect  bounds = fz_bound_page(m_ctx, page);
        const fz_irect bbox   = fz_round_rect(fz_transform_rect(bounds, matrix));

        pixmap = fz_new_pixmap_with_bbox(m_ctx,
                                         fz_device_rgb(m_ctx),
                                         bbox,
                                         nullptr,
                                         1);       // alpha channel
        fz_clear_pixmap_with_value(m_ctx, pixmap, 0xFF);  // white background

        dev = fz_new_draw_device(m_ctx, matrix, pixmap);
        fz_run_page(m_ctx, page, dev, fz_identity, nullptr);
        fz_close_device(m_ctx, dev);
        fz_drop_device(m_ctx, dev);
        dev = nullptr;

        const int    w       = fz_pixmap_width (m_ctx, pixmap);
        const int    h       = fz_pixmap_height(m_ctx, pixmap);
        const int    stride  = fz_pixmap_stride(m_ctx, pixmap);
        const uchar *samples = fz_pixmap_samples(m_ctx, pixmap);

        // MuPDF RGBA → Qt RGBA8888 (identical byte order on all platforms)
        result = QImage(w, h, QImage::Format_RGBA8888);
        if (!result.isNull()) {
            for (int y = 0; y < h; ++y) {
                std::memcpy(result.scanLine(y),
                            samples + y * stride,
                            static_cast<std::size_t>(w) * 4);
            }
        }
    }
    fz_always(m_ctx) {
        if (dev)    fz_drop_device(m_ctx, dev);
        if (pixmap) fz_drop_pixmap(m_ctx, pixmap);
        if (page)   fz_drop_page  (m_ctx, page);
    }
    fz_catch(m_ctx) {
        qWarning() << "[DocViewer] renderPage error (page" << pageNum << "):"
                   << fz_caught_message(m_ctx);
        return {};
    }

    return result;
}

// ─────────────────────────────────────────────────────────────
//  Static helpers
// ─────────────────────────────────────────────────────────────

static const QStringList &supportedSuffixes()
{
    static const QStringList list {
        "pdf",  "xps",  "oxps", "cbz",  "cbr",
        "epub", "mobi", "fb2",
        "svg",
        "png",  "jpg",  "jpeg", "jfif", "gif",
        "bmp",  "tif",  "tiff",
        "pnm",  "pbm",  "pgm",  "ppm",  "pam",
        "ps",   "eps"
    };
    return list;
}

bool DocumentHandler::isSupportedFile(const QString &path)
{
    return supportedSuffixes().contains(QFileInfo(path).suffix().toLower());
}

QString DocumentHandler::supportedExtensions()
{
    QStringList patterns;
    for (const QString &s : supportedSuffixes())
        patterns << "*." + s;

    return "All Supported Documents (" + patterns.join(' ') + ");;"
           "PDF Files (*.pdf);;"
           "E-Books (*.epub *.mobi *.fb2);;"
           "Comic Books (*.cbz *.cbr);;"
           "Images (*.png *.jpg *.jpeg *.gif *.bmp *.tif *.tiff *.svg);;"
           "PostScript (*.ps *.eps);;"
           "All Files (*)";
}
