# DocViewer

A lightweight, read-only document viewer built with **Qt 6 / QML 6** and **MuPDF**.

## Supported formats

PDF · XPS · OpenXPS · EPUB · MOBI · FB2 · CBZ · SVG · PNG · JPEG · GIF · BMP · TIFF · PNM · PBM · PGM · PPM · PAM · PostScript · EPS

---

## Dependencies

| Library | Version  | Purpose            |
|---------|----------|--------------------|
| Qt 6    | ≥ 6.4    | GUI, QML, Quick    |
| MuPDF   | ≥ 1.20   | Document rendering |
| CMake   | ≥ 3.16   | Build system       |

### Install dependencies

**Ubuntu / Debian**
```bash
sudo apt install \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  libmupdf-dev \
  cmake build-essential
```

**Arch Linux**
```bash
sudo pacman -S qt6-base qt6-declarative mupdf cmake
```

**macOS (Homebrew)**
```bash
brew install qt@6 mupdf cmake
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
```

**Windows (vcpkg)**
```powershell
vcpkg install qt6 mupdf
```

---

## Build

```bash
mkdir build && cd build
cmake ..                     # auto-detects Qt6 and MuPDF via pkg-config
cmake --build . -j$(nproc)
```

If MuPDF is installed in a non-standard location:
```bash
cmake -DMUPDF_ROOT=/path/to/mupdf ..
```

---

## Usage

```
./DocViewer [file]
```

### Keyboard shortcuts

| Key              | Action               |
|------------------|----------------------|
| `Ctrl+O`         | Open file            |
| `←` / `→`        | Previous / next page |
| `Page Up/Down`   | Previous / next page |
| `Home` / `End`   | First / last page    |
| `Ctrl+=` / `Ctrl+-` | Zoom in / out     |
| `Ctrl+0`         | Fit to window        |
| `Ctrl+W`         | Fit to width         |

Drag-and-drop a file onto the window to open it.

---

## Project structure

```
DocViewer/
├── CMakeLists.txt        # Build configuration
├── main.cpp              # Entry point, QApplication setup, CLI args
├── mainwindow.hpp/.cpp   # MainWindow, PageImageProvider, embedded QML
└── file_format.hpp/.cpp  # DocumentHandler (MuPDF wrapper, Q_OBJECT)
```

### Architecture

```
main.cpp
  └─ MainWindow
       ├─ DocumentHandler  (MuPDF C API wrapper, exposed to QML as "documentHandler")
       ├─ PageImageProvider ("image://pages/<page>/<zoom>/<ver>")
       └─ QQmlApplicationEngine  (loads QML embedded as raw string literal)
```

- **DocumentHandler** is registered as a QML context property.  
  QML binds directly to its `pageCount`, `currentPage`, `zoomLevel`, `isLoaded` properties.
- **PageImageProvider** renders pages off-thread via `DocumentHandler::renderPage()`,  
  protected by a `QMutex`.
- The QML source is embedded as a C++ raw string literal (`R"QML(...)QML"`).  
  No `.qrc` file or external resources are required.
