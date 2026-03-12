#include "mainwindow.hpp"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════
//  PageImageProvider
// ═══════════════════════════════════════════════════════════════

PageImageProvider::PageImageProvider(DocumentHandler *handler)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_handler(handler)
{}

// id format:  "<pageNum>/<zoom>/<rotation>/<version>"
QImage PageImageProvider::requestImage(const QString &id,
                                       QSize         *size,
                                       const QSize   & /*requestedSize*/)
{
    const QStringList parts = id.split('/');
    if (parts.size() < 2) return {};

    bool  ok1 = false, ok2 = false;
    const int   pageNum  = parts[0].toInt(&ok1);
    const qreal zoom     = parts[1].toDouble(&ok2);
    const int   rotation = (parts.size() >= 3) ? parts[2].toInt() : 0;

    if (!ok1 || !ok2) return {};

    QImage img = m_handler->renderPage(pageNum, zoom, rotation);
    if (size) *size = img.size();
    return img;
}

// ═══════════════════════════════════════════════════════════════
//  RecentFilesModel
// ═══════════════════════════════════════════════════════════════

RecentFilesModel::RecentFilesModel(QObject *parent)
    : QObject(parent)
    , m_settings("DocViewer", "DocViewer")
{
    load();
}

QStringList RecentFilesModel::paths() const { return m_paths; }

void RecentFilesModel::add(const QString &path)
{
    if (path.isEmpty()) return;
    m_paths.removeAll(path);
    m_paths.prepend(path);
    while (m_paths.size() > kMaxRecent)
        m_paths.removeLast();
    save();
    emit pathsChanged();
}

void RecentFilesModel::remove(const QString &path)
{
    if (m_paths.removeAll(path)) {
        save();
        emit pathsChanged();
    }
}

void RecentFilesModel::clear()
{
    if (m_paths.isEmpty()) return;
    m_paths.clear();
    save();
    emit pathsChanged();
}

void RecentFilesModel::load()
{
    m_paths = m_settings.value("recentFiles").toStringList();
    m_paths.erase(
        std::remove_if(m_paths.begin(), m_paths.end(),
            [](const QString &p){ return !QFileInfo::exists(p); }),
        m_paths.end());
}

void RecentFilesModel::save() const
{
    const_cast<QSettings&>(m_settings).setValue("recentFiles", m_paths);
}

// ═══════════════════════════════════════════════════════════════
//  MainWindow
// ═══════════════════════════════════════════════════════════════

MainWindow::MainWindow(QObject *parent)
    : QObject(parent)
{}

void MainWindow::show()
{
    QQmlContext *ctx = m_engine.rootContext();
    ctx->setContextProperty("docHandler",  &m_handler);
    ctx->setContextProperty("recentFiles", &m_recent);

    m_engine.addImageProvider("pages", new PageImageProvider(&m_handler));

    QObject::connect(&m_handler, &DocumentHandler::documentLoaded,
                     this, [this](const QString &, int) {
        m_recent.add(m_handler.filePath());
    });

    m_engine.loadData(QByteArray(qmlSource()));
}

void MainWindow::openFile(const QString &path)
{
    m_handler.openDocument(path);
}

// ═══════════════════════════════════════════════════════════════
//  QML source — full application UI
// ═══════════════════════════════════════════════════════════════

const char *MainWindow::qmlSource()
{
    return R"QML(
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    title: docHandler.isLoaded
           ? docHandler.documentTitle + " — DocViewer"
           : "DocViewer"
    width:  1100
    height: 760
    minimumWidth:  640
    minimumHeight: 480
    visible: true

    Material.theme:     Material.Dark
    Material.accent:    "#6C9EFF"
    Material.primary:   "#1A1D23"
    Material.background:"#12141A"
    Material.foreground:"#E8EAF0"

    property int  renderVersion: 0
    property bool sidebarOpen:   true
    property bool fullscreen:    false

    function bumpVersion() { renderVersion++ }
    function pageImageUrl(n) {
        return "image://pages/%1/%2/%3/%4"
               .arg(n)
               .arg(docHandler.zoomLevel)
               .arg(docHandler.rotation)
               .arg(renderVersion)
    }
    function openFileDialog() { fileDialog.open() }
    function fitWidth() {
        if (!docHandler.isLoaded) return
        var ps = docHandler.pageSize(docHandler.currentPage)
        if (ps.width <= 0) return
        docHandler.setZoomLevel((pageView.width - 32) / ps.width)
        bumpVersion()
    }
    function fitPage() {
        if (!docHandler.isLoaded) return
        var ps = docHandler.pageSize(docHandler.currentPage)
        if (ps.width <= 0 || ps.height <= 0) return
        var zw = (pageView.width  - 32) / ps.width
        var zh = (pageView.height - 32) / ps.height
        docHandler.setZoomLevel(Math.min(zw, zh))
        bumpVersion()
    }

    Connections {
        target: docHandler
        function onDocumentChanged()    { bumpVersion() }
        function onCurrentPageChanged() { bumpVersion() }
        function onZoomLevelChanged()   { bumpVersion() }
        function onErrorOccurred(msg)   { errorBar.message = msg; errorBar.show() }
    }

    // ── Keyboard shortcuts ────────────────────────────────────
    Shortcut { sequence: "Ctrl+O";         onActivated: openFileDialog() }
    Shortcut { sequence: "Ctrl+W";         onActivated: docHandler.closeDocument() }
    Shortcut { sequence: "Right";          onActivated: docHandler.nextPage() }
    Shortcut { sequence: "Left";           onActivated: docHandler.previousPage() }
    Shortcut { sequence: "Down";           onActivated: docHandler.nextPage() }
    Shortcut { sequence: "Up";             onActivated: docHandler.previousPage() }
    Shortcut { sequence: "Ctrl+Home";      onActivated: docHandler.goToPage(0) }
    Shortcut { sequence: "Ctrl+End";       onActivated: docHandler.goToPage(docHandler.pageCount - 1) }
    Shortcut { sequence: "Ctrl+Plus";      onActivated: { docHandler.setZoomLevel(docHandler.zoomLevel * 1.2); bumpVersion() } }
    Shortcut { sequence: "Ctrl+Minus";     onActivated: { docHandler.setZoomLevel(docHandler.zoomLevel / 1.2); bumpVersion() } }
    Shortcut { sequence: "Ctrl+0";         onActivated: { docHandler.setZoomLevel(1.0); bumpVersion() } }
    Shortcut { sequence: "Ctrl+1";         onActivated: fitWidth() }
    Shortcut { sequence: "Ctrl+2";         onActivated: fitPage() }
    Shortcut { sequence: "Ctrl+B";         onActivated: sidebarOpen = !sidebarOpen }
    Shortcut { sequence: "F11";            onActivated: { fullscreen = !fullscreen; fullscreen ? showFullScreen() : showNormal() } }
    Shortcut { sequence: "Ctrl+R";         onActivated: { docHandler.rotate(90);  bumpVersion() } }
    Shortcut { sequence: "Ctrl+Shift+R";   onActivated: { docHandler.rotate(-90); bumpVersion() } }

    // ── Drag-and-drop ─────────────────────────────────────────
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0)
                docHandler.openDocument(drop.urls[0])
        }
    }

    // ── Dialogs ───────────────────────────────────────────────
    FileDialog {
        id: fileDialog
        title: "Open Document"
        nameFilters: [ docHandler.supportedExtensions() ]
        onAccepted: docHandler.openDocument(selectedFile)
    }

    // ── Layout ────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Toolbar ───────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 50
            color: "#1A1D23"

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: "#2A2D36"
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                spacing: 2

                ToolButton {
                    text: "☰"
                    font.pixelSize: 18
                    ToolTip.text: "Toggle Sidebar  (Ctrl+B)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: sidebarOpen = !sidebarOpen
                }
                ToolButton {
                    text: "📂"
                    font.pixelSize: 16
                    ToolTip.text: "Open File  (Ctrl+O)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: openFileDialog()
                }
                ToolButton {
                    text: "✕"
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Close  (Ctrl+W)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: docHandler.closeDocument()
                }

                Rectangle { width: 1; height: 28; color: "#2A2D36"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

                ToolButton {
                    text: "‹"
                    font.pixelSize: 20; font.bold: true
                    enabled: docHandler.isLoaded && docHandler.currentPage > 0; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Previous Page  (←)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: docHandler.previousPage()
                }

                // Inline page number editor
                TextField {
                    id: pageField
                    width: 46; height: 28
                    horizontalAlignment: TextInput.AlignHCenter
                    text: docHandler.isLoaded ? (docHandler.currentPage + 1).toString() : "—"
                    enabled: docHandler.isLoaded
                    font.pixelSize: 13
                    background: Rectangle {
                        color: pageField.activeFocus ? "#2A2D36" : "transparent"
                        border.color: pageField.activeFocus ? Material.accent : "#3A3D46"
                        border.width: 1; radius: 4
                    }
                    onAccepted: {
                        var n = parseInt(text, 10)
                        if (!isNaN(n) && n >= 1 && n <= docHandler.pageCount)
                            docHandler.goToPage(n - 1)
                        else
                            text = (docHandler.currentPage + 1).toString()
                    }
                    Connections {
                        target: docHandler
                        function onCurrentPageChanged() {
                            if (!pageField.activeFocus)
                                pageField.text = (docHandler.currentPage + 1).toString()
                        }
                    }
                }
                Label {
                    text: docHandler.isLoaded ? ("/ " + docHandler.pageCount) : ""
                    font.pixelSize: 12; color: "#8890A0"
                }

                ToolButton {
                    text: "›"
                    font.pixelSize: 20; font.bold: true
                    enabled: docHandler.isLoaded && docHandler.currentPage < docHandler.pageCount - 1
                    opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Next Page  (→)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: docHandler.nextPage()
                }

                Rectangle { width: 1; height: 28; color: "#2A2D36"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

                ToolButton {
                    text: "−"
                    font.pixelSize: 18; font.bold: true
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Zoom Out  (Ctrl+−)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: { docHandler.setZoomLevel(docHandler.zoomLevel / 1.2); bumpVersion() }
                }
                ComboBox {
                    id: zoomCombo
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    width: 88; editable: true
                    model: ["50%","75%","100%","125%","150%","200%","300%","400%"]
                    displayText: Math.round(docHandler.zoomLevel * 100) + "%"
                    onActivated: {
                        var v = parseFloat(currentText) / 100.0
                        if (!isNaN(v) && v > 0) { docHandler.setZoomLevel(v); bumpVersion() }
                    }
                    contentItem: TextField {
                        text: zoomCombo.displayText
                        horizontalAlignment: TextInput.AlignHCenter
                        font.pixelSize: 12
                        background: Item {}
                        onAccepted: {
                            var v = parseFloat(text.replace("%","")) / 100.0
                            if (!isNaN(v) && v > 0) { docHandler.setZoomLevel(v); bumpVersion() }
                        }
                    }
                }
                ToolButton {
                    text: "+"
                    font.pixelSize: 18; font.bold: true
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Zoom In  (Ctrl++)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: { docHandler.setZoomLevel(docHandler.zoomLevel * 1.2); bumpVersion() }
                }
                ToolButton {
                    text: "W"; font.bold: true; font.pixelSize: 13
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Fit Width  (Ctrl+1)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: fitWidth()
                }
                ToolButton {
                    text: "P"; font.bold: true; font.pixelSize: 13
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Fit Page  (Ctrl+2)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: fitPage()
                }

                Rectangle { width: 1; height: 28; color: "#2A2D36"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

                ToolButton {
                    text: "↺"; font.pixelSize: 18
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Rotate CCW  (Ctrl+Shift+R)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: { docHandler.rotate(-90); bumpVersion() }
                }
                ToolButton {
                    text: "↻"; font.pixelSize: 18
                    enabled: docHandler.isLoaded; opacity: enabled ? 1 : 0.35
                    ToolTip.text: "Rotate CW  (Ctrl+R)"; ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: { docHandler.rotate(90); bumpVersion() }
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    text: root.fullscreen ? "⊡" : "⊞"; font.pixelSize: 18
                    ToolTip.text: root.fullscreen ? "Exit Fullscreen (F11)" : "Fullscreen (F11)"
                    ToolTip.visible: hovered; ToolTip.delay: 600
                    onClicked: { root.fullscreen = !root.fullscreen; root.fullscreen ? root.showFullScreen() : root.showNormal() }
                }
            }
        } // toolbar

        // ── Body ──────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Sidebar ───────────────────────────────────────
            Rectangle {
                id: sidebar
                visible: sidebarOpen
                width: visible ? 210 : 0
                Layout.fillHeight: true
                color: "#14161D"

                property int currentTab: 0

                Rectangle {
                    anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                    width: 1; color: "#2A2D36"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Tab bar
                    Rectangle {
                        Layout.fillWidth: true; height: 38; color: "#1A1D23"
                        Rectangle {
                            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                            height: 1; color: "#2A2D36"
                        }
                        RowLayout {
                            anchors.fill: parent; spacing: 0
                            Repeater {
                                model: ["Thumbnails", "Recent"]
                                Rectangle {
                                    Layout.fillWidth: true; height: parent.height
                                    color: sidebar.currentTab === index ? "#12141A" : "transparent"
                                    property int index: model.index
                                    Rectangle {
                                        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                                        height: 2
                                        color: sidebar.currentTab === parent.index ? Material.accent : "transparent"
                                    }
                                    Label {
                                        anchors.centerIn: parent; text: modelData
                                        font.pixelSize: 11
                                        color: sidebar.currentTab === parent.index ? "#E8EAF0" : "#6870A0"
                                    }
                                    MouseArea { anchors.fill: parent; onClicked: sidebar.currentTab = parent.index }
                                }
                            }
                        }
                    }

                    // Thumbnails tab
                    ListView {
                        id: thumbList
                        visible: sidebar.currentTab === 0
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        model: docHandler.isLoaded ? docHandler.pageCount : 0
                        currentIndex: docHandler.currentPage
                        highlightMoveDuration: 150
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            width: thumbList.width; height: 138
                            Rectangle {
                                id: thumbCell
                                anchors { top: parent.top; topMargin: 8; horizontalCenter: parent.horizontalCenter }
                                width: 150; height: 112
                                color: "#1E2130"
                                border.color: index === docHandler.currentPage ? Material.accent : "#2A2D36"
                                border.width: index === docHandler.currentPage ? 2 : 1
                                radius: 3
                                Image {
                                    anchors { fill: parent; margins: 2 }
                                    source: docHandler.isLoaded
                                            ? "image://pages/%1/0.5/%2/%3".arg(index).arg(docHandler.rotation).arg(renderVersion)
                                            : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true; cache: false
                                    opacity: status === Image.Ready ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                }
                                BusyIndicator {
                                    anchors.centerIn: parent
                                    running: parent.children[0].status === Image.Loading
                                    width: 24; height: 24
                                }
                            }
                            Label {
                                anchors { top: thumbCell.bottom; topMargin: 3; horizontalCenter: parent.horizontalCenter }
                                text: (index + 1).toString(); font.pixelSize: 10
                                color: index === docHandler.currentPage ? Material.accent : "#6870A0"
                            }
                            MouseArea { anchors.fill: parent; onClicked: docHandler.goToPage(index) }
                        }

                        Connections {
                            target: docHandler
                            function onCurrentPageChanged() {
                                thumbList.positionViewAtIndex(docHandler.currentPage, ListView.Contain)
                            }
                        }
                    }

                    // Recent files tab
                    ColumnLayout {
                        visible: sidebar.currentTab === 1
                        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

                        ListView {
                            id: recentList
                            Layout.fillWidth: true; Layout.fillHeight: true
                            clip: true; model: recentFiles.paths
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            delegate: ItemDelegate {
                                width: recentList.width
                                text: modelData.split("/").pop()
                                font.pixelSize: 12
                                ToolTip.text: modelData; ToolTip.visible: hovered; ToolTip.delay: 600
                                onClicked: docHandler.openDocument(modelData)
                            }
                            Label {
                                visible: recentList.count === 0
                                anchors.centerIn: parent
                                text: "No recent files"; color: "#4A5070"; font.pixelSize: 12
                            }
                        }
                        Button {
                            Layout.fillWidth: true; Layout.margins: 8
                            text: "Clear Recent Files"; flat: true
                            font.pixelSize: 11
                            enabled: recentFiles.paths.length > 0
                            onClicked: recentFiles.clear()
                        }
                    }
                }
            } // sidebar

            // ── Page viewer ───────────────────────────────────
            Rectangle {
                id: pageView
                Layout.fillWidth: true; Layout.fillHeight: true
                color: "#0D0F14"

                // Empty state
                Column {
                    anchors.centerIn: parent
                    visible: !docHandler.isLoaded && !docHandler.loading
                    spacing: 16
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "📄"; font.pixelSize: 72; color: "#2A2D36"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Open a document to get started"
                        font.pixelSize: 18; color: "#3A4060"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Drag & drop a file here, or press Ctrl+O"
                        font.pixelSize: 13; color: "#2A3050"
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Open File…"; highlighted: true
                        onClicked: openFileDialog()
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: docHandler.loading
                    width: 56; height: 56
                }

                ScrollView {
                    anchors.fill: parent
                    visible: docHandler.isLoaded
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                    ScrollBar.vertical.policy:   ScrollBar.AsNeeded

                    Item {
                        width:  Math.max(pageView.width,  pageImage.width  + 32)
                        height: Math.max(pageView.height, pageImage.height + 32)

                        Image {
                            id: pageImage
                            anchors.centerIn: parent
                            source: docHandler.isLoaded ? pageImageUrl(docHandler.currentPage) : ""
                            fillMode: Image.Pad
                            smooth: true; asynchronous: true; cache: false
                            BusyIndicator {
                                anchors.centerIn: parent
                                running: pageImage.status === Image.Loading
                                width: 48; height: 48
                            }
                        }

                        WheelHandler {
                            acceptedModifiers: Qt.ControlModifier
                            onWheel: (event) => {
                                var f = event.angleDelta.y > 0 ? 1.1 : 1.0/1.1
                                docHandler.setZoomLevel(docHandler.zoomLevel * f)
                                bumpVersion()
                            }
                        }
                        WheelHandler {
                            acceptedModifiers: Qt.NoModifier
                            property real acc: 0
                            onWheel: (event) => {
                                acc += event.angleDelta.y
                                if (acc >  120) { acc = 0; docHandler.previousPage() }
                                if (acc < -120) { acc = 0; docHandler.nextPage()     }
                            }
                        }
                    }
                }

                // Click-zone: left 10% = previous page
                MouseArea {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: parent.width * 0.10
                    visible: docHandler.isLoaded
                    cursorShape: Qt.PointingHandCursor
                    onClicked: docHandler.previousPage()
                }
                // Click-zone: right 10% = next page
                MouseArea {
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    width: parent.width * 0.10
                    visible: docHandler.isLoaded
                    cursorShape: Qt.PointingHandCursor
                    onClicked: docHandler.nextPage()
                }
            } // pageView
        } // body RowLayout

        // ── Status bar ────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; height: 26; color: "#1A1D23"
            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 1; color: "#2A2D36"
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 16
                Label {
                    text: docHandler.isLoaded ? docHandler.filePath : "No document open"
                    font.pixelSize: 11; color: "#5A6280"
                    elide: Text.ElideMiddle; Layout.fillWidth: true
                }
                Label {
                    visible: docHandler.isLoaded
                    text: {
                        var ps = docHandler.isLoaded
                                 ? docHandler.pageSize(docHandler.currentPage) : null
                        return (ps && ps.width > 0)
                               ? "%1 × %2 pt".arg(Math.round(ps.width)).arg(Math.round(ps.height))
                               : ""
                    }
                    font.pixelSize: 11; color: "#5A6280"
                }
                Label {
                    visible: docHandler.isLoaded
                    text: Math.round(docHandler.zoomLevel * 100) + "%"
                    font.pixelSize: 11; color: "#5A6280"
                }
                Label {
                    visible: docHandler.isLoaded
                    text: "Page %1 of %2".arg(docHandler.currentPage + 1).arg(docHandler.pageCount)
                    font.pixelSize: 11; color: "#5A6280"
                }
            }
        }
    } // outer ColumnLayout

    // ── Error bar ─────────────────────────────────────────────
    Rectangle {
        id: errorBar
        property string message: ""
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right; bottomMargin: 28 }
        height: 44; color: "#8B2020"; visible: false; z: 100
        function show() { visible = true; hideTimer.restart() }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
            Label {
                Layout.fillWidth: true; text: errorBar.message
                color: "#FFCCCC"; font.pixelSize: 13; elide: Text.ElideRight
            }
            ToolButton { text: "✕"; onClicked: errorBar.visible = false }
        }
        Timer { id: hideTimer; interval: 6000; onTriggered: errorBar.visible = false }
    }
}
)QML";
}
