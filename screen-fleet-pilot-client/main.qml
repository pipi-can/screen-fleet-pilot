import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "qml/components"
import "qml/pages"

Window {
    id: mainWindow
    width: 900
    height: 680
    minimumWidth: 900
    minimumHeight: 680
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    x: Screen.width / 2 - width / 2
    y: Screen.height / 2 - height / 2

    // 手动最大化状态（避免无边框窗口 showMaximized 尺寸异常）
    readonly property bool isMaximized: customMaximized
    property bool customMaximized: false
    property int normalX: 0
    property int normalY: 0
    property int normalWidth: 0
    property int normalHeight: 0

    property bool showScreenshot: false
    property string screenshotPreviewDev: ""
    property string screenshotPreviewImg: ""

    function toggleMaximize() {
        if (customMaximized) {
            // 还原
            x = normalX
            y = normalY
            width = normalWidth
            height = normalHeight
            customMaximized = false
        } else {
            // 保存当前几何
            normalX = x
            normalY = y
            normalWidth = width
            normalHeight = height
            // 最大化到当前屏幕可用区域
            var scr = screen
            x = scr.virtualX
            y = scr.virtualY
            width = scr.width
            height = scr.height
            customMaximized = true
        }
    }

    // 圆角矩形背景
    Rectangle {
        id: background
        anchors.fill: parent
        //anchors.margins: mainWindow.isMaximized ? 0 : 10
        radius: mainWindow.isMaximized ? 0 : 12
        color: "#1a1a2e"
        clip: true

        // ── 标题栏 ──
        TitleBar {
            id: titleBar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            targetWindow: mainWindow
            title: "商业显示设备远程管控平台"
        }

        // ── 内容区域 ──
        Rectangle {
            id: contentArea
            anchors {
                left: parent.left; right: parent.right
                top: titleBar.bottom
                bottom: parent.bottom
            }
            color: "#1a1a2e"

            // ═══════════════════════════════════════
            // 统计卡片行
            // ═══════════════════════════════════════
            Row {
                id: statsRow
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors { leftMargin: 20; rightMargin: 20; topMargin: 16 }
                spacing: 12

                StatCard {
                    width: (contentArea.width - 40 - 3 * 12) / 4
                    accentColor: "#22c55e"
                    iconText: "🟢"
                    count: NetworkManager.onlineCount
                    label: "在线设备"
                }
                StatCard {
                    width: (contentArea.width - 40 - 3 * 12) / 4
                    accentColor: "#eab308"
                    iconText: "🟡"
                    count: NetworkManager.warningCount
                    label: "告警设备"
                }
                StatCard {
                    width: (contentArea.width - 40 - 3 * 12) / 4
                    accentColor: "#ef4444"
                    iconText: "🔴"
                    count: NetworkManager.offlineCount
                    label: "离线设备"
                }
                StatCard {
                    width: (contentArea.width - 40 - 3 * 12) / 4
                    accentColor: "#3b82f6"
                    iconText: "📦"
                    count: NetworkManager.totalCount
                    label: "设备总数"
                }
            }

            // ═══════════════════════════════════════
            // 功能标签栏
            // ═══════════════════════════════════════
            FuncTabWidget {
                id: funcTabWidget
                anchors {
                    left: parent.left
                    right: parent.right
                    top: statsRow.bottom
                    topMargin: 8
                }
                onTabClicked: pageKey => switchPage(pageKey)
            }

            // ═══════════════════════════════════════
            // 页面容器 — StackView
            // ═══════════════════════════════════════
            StackView {
                id: contentStack
                anchors {
                    left: parent.left
                    right: parent.right
                    top: funcTabWidget.bottom
                    bottom: parent.bottom
                }
            }
        } //
    }

    // 📸 实时截图弹窗（Window 顶层，避免 StackView 内页面裁剪）
    Rectangle {
        id: screenshotOverlay
        anchors.fill: background
        color: "#80000000"
        visible: showScreenshot
        z: 10000

        MouseArea { anchors.fill: parent }

        Rectangle {
            width: Math.min(parent.width * 0.75, 860)
            height: Math.min(parent.height * 0.75, 640)
            anchors.centerIn: parent
            radius: 12
            color: "#1f2937"
            border { width: 1; color: "#334155" }

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Row {
                    width: parent.width
                    spacing: 12

                    Column {
                        width: parent.width - 72
                        spacing: 4

                        Text {
                            text: "📸 实时截图 — " + screenshotPreviewDev
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: "#e2e8f0"
                        }

                        Text {
                            width: parent.width
                            text: "以下为设备当前画面"
                            font.pixelSize: 12
                            color: "#64748b"
                            wrapMode: Text.WordWrap
                        }
                    }

                    Rectangle {
                        width: 64
                        height: 28
                        radius: 7
                        color: "#3b82f6"
                        Text { anchors.centerIn: parent; text: "关闭"; font.pixelSize: 12; color: "#fff" }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                showScreenshot = false
                                screenshotPreviewImg = ""
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: parent.height - 72
                    radius: 8
                    color: "#111827"
                    border { width: 1; color: "#334155" }
                    clip: true

                    Image {
                        id: screenshotImage
                        anchors.fill: parent
                        anchors.margins: 8
                        source: screenshotPreviewImg
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "图片加载中..."
                        font.pixelSize: 14
                        color: "#64748b"
                        visible: screenshotImage.status === Image.Loading
                    }

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        text: "图片加载失败\n" + screenshotPreviewImg
                        font.pixelSize: 12
                        color: "#ef4444"
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: screenshotImage.status === Image.Error
                    }
                }
            }
        }
    }

    // ── 预实例化的页面（不被 StackView 管理的独立生命周期） ──
    property var dashboardPage: null
    property var pushPage: null
    property var otaPage: null

    function switchPage(pageKey) {
        var target = null
        switch (pageKey) {
            case "dashboard": target = dashboardPage; break
            case "push":      target = pushPage;      break
            case "ota":       target = otaPage;       break
        }
        if (target && contentStack.currentItem !== target) {
            contentStack.replace(null, target)
        }
        if (pageKey === "push") {
            NetworkManager.requestServerFileList()
        }
        if (pageKey === "ota") {
            NetworkManager.requestServerFirmwareList()
        }
    }

    Component.onCompleted:  {
        // 预实例化三个页面
        dashboardPage = dashboardComp.createObject()
        pushPage      = pushComp.createObject()
        otaPage       = otaComp.createObject()

        // 初始显示仪表盘
        contentStack.push(dashboardPage)

        NetworkManager.connectToServer("8.136.113.168", 8000)
    }

    // 推送按钮跳转
    property string pendingPushDevice: ""

    Connections {
        target: NetworkManager
        function onPushToDeviceRequested(deviceName) {
            pendingPushDevice = deviceName
            funcTabWidget.currentPage = "push"
            switchPage("push")
        }
        function onScreenshotUrlReceived(deviceName, imageUrl) {
            screenshotPreviewDev = deviceName
            screenshotPreviewImg = imageUrl
            showScreenshot = true
        }
    }

    // ── 页面组件定义（只用于 createObject，不直接渲染） ──
    Component { id: dashboardComp;  DashboardPage  {} }
    Component { id: pushComp;       ContentPushPage {} }
    Component { id: otaComp;        OTAPage        {} }

    // ── 边缘 resize（最大化时禁用） ──
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { left: parent.left; top: parent.top; topMargin: 10; bottom: parent.bottom; bottomMargin: 10 }
        width: 6
        cursorShape: Qt.SizeHorCursor
        onPressed: mainWindow.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { right: parent.right; top: parent.top; topMargin: 10; bottom: parent.bottom; bottomMargin: 10 }
        width: 6
        cursorShape: Qt.SizeHorCursor
        onPressed: mainWindow.startSystemResize(Qt.RightEdge)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { top: parent.top; left: parent.left; leftMargin: 10; right: parent.right; rightMargin: 10 }
        height: 6
        cursorShape: Qt.SizeVerCursor
        onPressed: mainWindow.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { bottom: parent.bottom; left: parent.left; leftMargin: 10; right: parent.right; rightMargin: 10 }
        height: 6
        cursorShape: Qt.SizeVerCursor
        onPressed: mainWindow.startSystemResize(Qt.BottomEdge)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { left: parent.left; top: parent.top }
        width: 10; height: 10
        cursorShape: Qt.SizeFDiagCursor
        onPressed: mainWindow.startSystemResize(Qt.TopLeftCorner)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { right: parent.right; top: parent.top }
        width: 10; height: 10
        cursorShape: Qt.SizeBDiagCursor
        onPressed: mainWindow.startSystemResize(Qt.TopRightCorner)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { left: parent.left; bottom: parent.bottom }
        width: 10; height: 10
        cursorShape: Qt.SizeBDiagCursor
        onPressed: mainWindow.startSystemResize(Qt.BottomLeftCorner)
    }
    MouseArea {
        enabled: !mainWindow.isMaximized
        anchors { right: parent.right; bottom: parent.bottom }
        width: 10; height: 10
        cursorShape: Qt.SizeFDiagCursor
        onPressed: mainWindow.startSystemResize(Qt.BottomRightCorner)
    }
}
