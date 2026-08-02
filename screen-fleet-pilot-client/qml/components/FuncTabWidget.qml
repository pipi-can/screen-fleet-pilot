import QtQuick 2.15

// 功能标签栏 — 设备仪表盘 / 内容推送 / OTA 升级
Rectangle {
    id: root

    // 当前激活的页面标识
    property string currentPage: "dashboard"
    readonly property var tabs: [
        { key: "dashboard",  icon: "📊", label: "设备仪表盘" },
        { key: "push",       icon: "📤", label: "内容推送"   },
        { key: "ota",        icon: "⬆",  label: "OTA 升级"  }
    ]

    signal tabClicked(string pageKey)

    implicitHeight: 42
    color: "#1a1a2e"

    // 底部分隔线
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: "#334155"
    }

    Row {
        anchors {
            left: parent.left
            leftMargin: 20
            verticalCenter: parent.verticalCenter
        }
        spacing: 0

        Repeater {
            model: root.tabs

            Item {
                width: tabBtn.implicitWidth + 40
                height: root.height

                // Tab 按钮
                Text {
                    id: tabBtn
                    anchors.centerIn: parent
                    text: modelData.icon + "  " + modelData.label
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: {
                        if (root.currentPage === modelData.key)
                            return "#06b6d4"       // cyan 激活色
                        else if (tabMouse.containsMouse)
                            return "#94a3b8"       // hover 色
                        else
                            return "#64748b"       // 默认灰
                    }
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                // 激活下划线
                Rectangle {
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 1
                    }
                    width: tabBtn.implicitWidth + 20
                    height: 2
                    radius: 1
                    color: root.currentPage === modelData.key ? "#06b6d4" : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentPage = modelData.key
                        root.tabClicked(modelData.key)
                    }
                }
            }
        }
    }
}
