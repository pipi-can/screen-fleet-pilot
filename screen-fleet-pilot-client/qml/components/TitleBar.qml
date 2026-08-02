import QtQuick 2.15
import QtQuick.Window 2.15

// 自定义标题栏 — 鼠标拖拽移动窗口 + 窗口控制按钮
Rectangle {
    id: root

    required property Window targetWindow
    property string title: ""

    property color bgColor: "#16213e"
    property color textColor: "#e2e8f0"
    property color btnHoverColor: "#3a3a4e"
    property color closeHoverColor: "#ef4444"
    property color iconColor: "#94a3b8"

    implicitHeight: 44
    radius: root.targetWindow.isMaximized ? 0 : 12
    color: bgColor

    // ── 遮住底部圆角（最大化时不需要） ──
    Rectangle {
        visible: !root.targetWindow.isMaximized
        anchors { left: parent.left; bottom: parent.bottom }
        width: 12; height: 12
        color: root.bgColor
    }
    Rectangle {
        visible: !root.targetWindow.isMaximized
        anchors { right: parent.right; bottom: parent.bottom }
        width: 12; height: 12
        color: root.bgColor
    }

    // ── 鼠标拖拽：startSystemMove ──
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: root.targetWindow.startSystemMove()
        onDoubleClicked: root.targetWindow.toggleMaximize()
    }

    // ═══════════════════════════════════════
    // 左侧：图标 + 标题
    // ═══════════════════════════════════════
    Row {
        anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
        spacing: 8
        Image {
            source: "qrc:/resources/app-icon.svg"
            width: 28; height: 28
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.title
            color: root.textColor
            font.pixelSize: 13
            font.weight: Font.Bold
            font.letterSpacing: 1
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ═══════════════════════════════════════
    // 右侧：连接状态 + 窗口控制按钮
    // ═══════════════════════════════════════
    Row {
        anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
        spacing: 8

        // ── 连接状态指示器 ──
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            visible: typeof NetworkManager !== "undefined"

            // 状态圆点
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8; height: 8
                radius: 4
                color: {
                    var s = NetworkManager.connectionStatus
                    if (s === "已连接") return "#22c55e"
                    else if (s === "连接中..." || s === "重连中...") return "#f59e0b"
                    else return "#ef4444"
                }
            }

            // 状态文字
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: NetworkManager.connectionStatus
                color: {
                    var s = NetworkManager.connectionStatus
                    if (s === "已连接") return "#86efac"
                    else if (s === "连接中..." || s === "重连中...") return "#fde68a"
                    else return "#fca5a5"
                }
                font.pixelSize: 12
            }
        }

        // ── 最小化 ──
        Rectangle {
            width: 36; height: 28; radius: 6
            color: minimizeMouse.containsMouse ? root.btnHoverColor : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            ColorImage {
                anchors.centerIn: parent
                source: "qrc:/resources/TitleBar/minimize.svg"
                sourceColor: root.iconColor
                width: 14; height: 14
            }

            MouseArea {
                id: minimizeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.targetWindow.showMinimized()
            }
        }

        // ── 最大化 / 还原 ──
        Rectangle {
            width: 36; height: 28; radius: 6
            color: maximizeMouse.containsMouse ? root.btnHoverColor : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            ColorImage {
                anchors.centerIn: parent
                source: root.targetWindow.isMaximized
                    ? "qrc:/resources/TitleBar/exit-fullscreen.svg"
                    : "qrc:/resources/TitleBar/fullscreen.svg"
                sourceColor: root.iconColor
                width: 14; height: 14
            }

            MouseArea {
                id: maximizeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.targetWindow.toggleMaximize()
            }
        }

        // ── 关闭 ──
        Rectangle {
            width: 36; height: 28; radius: 6
            color: closeMouse.containsMouse ? root.closeHoverColor : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            ColorImage {
                anchors.centerIn: parent
                source: "qrc:/resources/TitleBar/close.svg"
                sourceColor: closeMouse.containsMouse ? "#ffffff" : root.iconColor
                width: 14; height: 14
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.targetWindow.close()
            }
        }
    }
}
