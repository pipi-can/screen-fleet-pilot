import QtQuick 2.15

// 统计卡片 — 在线 / 告警 / 离线 / 总数
Rectangle {
    id: root

    property alias count: countText.text
    property alias label: labelText.text
    property color accentColor: "#22c55e"
    property string iconText: ""

    implicitWidth: 180
    implicitHeight: 76
    radius: 10
    color: "#1f2937"
    border { width: 1; color: "#334155" }

    // hover 效果
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
    }

    Row {
        anchors { left: parent.left; leftMargin: 18; verticalCenter: parent.verticalCenter }
        spacing: 12

        // 图标容器
        Rectangle {
            width: 44; height: 44
            radius: 10
            color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: root.iconText
                font.pixelSize: 20
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                id: countText
                text: "0"
                font.pixelSize: 28
                font.weight: Font.Bold
                color: "#e2e8f0"
            }

            Text {
                id: labelText
                text: ""
                font.pixelSize: 12
                color: "#64748b"
            }
        }
    }
}
