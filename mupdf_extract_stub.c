/*
 * mupdf_extract_stub.c
 *
 * Stub implementations of the extract_* symbols that libmupdf.a
 * (output-docx.o) needs but that are not available on this system
 * because libmupdf-extract / libextract is not packaged separately.
 *
 * These stubs make the linker happy.  The DOCX export feature of
 * MuPDF will not work at runtime (it will silently fail or return
 * an error), but all other MuPDF functionality (PDF/EPUB/image
 * rendering) works perfectly.
 *
 * Usage: add this file to the qt_add_executable() source list in
 * CMakeLists.txt only when LIB_EXTRACT is not found.
 */

#include <stddef.h>
#include <stdlib.h>

/* Minimal opaque types */
typedef struct { int dummy; } extract_alloc_t;
typedef struct { int dummy; } extract_t;
typedef struct { int dummy; } extract_buffer_t;

/* --- allocation --- */
int  extract_alloc_create(extract_alloc_t **alloc)       { if(alloc) *alloc = NULL; return -1; }
void extract_alloc_destroy(extract_alloc_t **alloc)      { (void)alloc; }

/* --- document lifecycle --- */
int  extract_begin(extract_t **extract, int doctype,
                   extract_alloc_t *alloc, int layout)
                                                          { if(extract) *extract=NULL; (void)doctype;(void)alloc;(void)layout; return -1; }
int  extract_end(extract_t **extract, int err)            { (void)extract; return err; }
int  extract_process(extract_t *extract, int autosplit,
                     int spacing, int rotation)           { (void)extract;(void)autosplit;(void)spacing;(void)rotation; return -1; }
int  extract_write(extract_t *extract,
                   extract_buffer_t *buffer)              { (void)extract;(void)buffer; return -1; }

/* --- page lifecycle --- */
int  extract_page_begin(extract_t *extract)               { (void)extract; return -1; }
int  extract_page_end(extract_t *extract)                 { (void)extract; return -1; }

/* --- spans / chars --- */
int  extract_span_begin(extract_t *e, const char *font,
                        int bold, int italic, double size,
                        int wmode, double tm[6])
                                                          { (void)e;(void)font;(void)bold;(void)italic;(void)size;(void)wmode;(void)tm; return -1; }
int  extract_span_end(extract_t *e)                       { (void)e; return -1; }
int  extract_add_char(extract_t *e, double x, double y,
                      unsigned ucs, double adv)           { (void)e;(void)x;(void)y;(void)ucs;(void)adv; return -1; }

/* --- paths --- */
int  extract_fill_begin(extract_t *e, double *ctm,
                        double color, double alpha)       { (void)e;(void)ctm;(void)color;(void)alpha; return -1; }
int  extract_fill_end(extract_t *e)                       { (void)e; return -1; }
int  extract_stroke_begin(extract_t *e, double *ctm,
                          double width, double color,
                          double alpha)                   { (void)e;(void)ctm;(void)width;(void)color;(void)alpha; return -1; }
int  extract_stroke_end(extract_t *e)                     { (void)e; return -1; }
int  extract_moveto(extract_t *e, double x, double y)    { (void)e;(void)x;(void)y; return -1; }
int  extract_lineto(extract_t *e, double x, double y)    { (void)e;(void)x;(void)y; return -1; }
int  extract_closepath(extract_t *e)                      { (void)e; return -1; }

/* --- images --- */
int  extract_add_image(extract_t *e, const char *type,
                       double x, double y,
                       double w, double h,
                       const char *data, size_t len)
                                                          { (void)e;(void)type;(void)x;(void)y;(void)w;(void)h;(void)data;(void)len; return -1; }

/* --- structure --- */
int  extract_begin_struct(extract_t *e, int type,
                          const char *id, const char *raw){ (void)e;(void)type;(void)id;(void)raw; return -1; }
int  extract_end_struct(extract_t *e)                    { (void)e; return -1; }

/* --- buffers --- */
int  extract_buffer_open(extract_buffer_t **buf,
                         void *opaque, void *write_fn,
                         void *close_fn)                  { if(buf) *buf=NULL; (void)opaque;(void)write_fn;(void)close_fn; return -1; }
int  extract_buffer_close(extract_buffer_t **buf)         { (void)buf; return -1; }

/* --- misc --- */
int  extract_set_layout_analysis(extract_t *e, int on)   { (void)e;(void)on; return -1; }
int  extract_tables_csv_format(extract_t *e, int fmt)    { (void)e;(void)fmt; return -1; }
