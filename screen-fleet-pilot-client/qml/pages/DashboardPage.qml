import QtQuick 2.15
import QtQuick.Controls 2.15

// 页面 1：设备仪表盘
Rectangle {
    id: root
    color: "#1a1a2e"

    property var deviceModel: NetworkManager.deviceModel
    property string activeGroup: "" // "" = 全部

    // ═══════════════════════════════════════
    // 设备数据变化 → 如果当前分组不存在则回退"全部"
    // ═══════════════════════════════════════
    Connections {
        target: deviceModel
        function onGroupsChanged() {
            // 无条件重建分组列表（数据来了、分组变了都要刷新侧边栏）
            groupList.model = buildGroupModel()

            // 如果当前筛选的分组已不存在，回退到"全部"
            if (activeGroup !== "") {
                var names = deviceModel.groupNames
                var found = false
                for (var i = 0; i < names.length; i++) {
                    if (names[i] === activeGroup) { found = true; break }
                }
                if (!found) {
                    activeGroup = ""
                    deviceModel.filterGroup = ""
                    groupList.model = buildGroupModel()
                }
            }
        }
    }

    Connections {
        target: NetworkManager
        function onUpdateEmbeddedMessageFinished() {
            toastMsg = "✅ 设备信息修改成功"
            showToast = true
            toastTimer.restart()
        }
    }

    property string toastMsg: ""
    property bool showToast: false

    // 右键菜单上下文
    property string contextMenuDeviceName: ""
    property string contextMenuDeviceUId: ""
    property int contextMenuDeviceId: 0

    Menu {
        id: deviceContextMenu
        padding: 4

        background: Rectangle {
            implicitWidth: 140
            color: "#1f2937"
            radius: 8
            border { width: 1; color: "#334155" }
        }

        MenuItem {
            text: "删除设备"
            enabled: contextMenuDeviceUId !== ""
            contentItem: Text {
                text: parent.text
                font.pixelSize: 13
                color: parent.enabled ? "#ef4444" : "#64748b"
                leftPadding: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitHeight: 32
                radius: 4
                color: parent.highlighted ? Qt.rgba(0.937, 0.267, 0.267, 0.12) : "transparent"
            }
            onTriggered: NetworkManager.requestMaskDevice(contextMenuDeviceUId)
        }
    }

    Timer {
        id: toastTimer
        interval: 2000
        onTriggered: showToast = false
    }

    // ═══════════════════════════════════════
    // 主布局：左侧边栏 + 右侧内容
    // ═══════════════════════════════════════
    Item {
        anchors.fill: parent

        // ─────────────────────────────────────
        // 左侧：分组树
        // ─────────────────────────────────────
        Rectangle {
            id: sidebar
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: 190
            color: "#16213e"
            border { width: 1; color: "#334155" }

            // 分组标题
            Text {
                text: "设备分组"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: "#64748b"
                anchors { left: parent.left; leftMargin: 16; top: parent.top; topMargin: 14 }
            }

            // 分组列表
            ListView {
                id: groupList
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top; topMargin: 38
                    bottom: parent.bottom; bottomMargin: 8
                }
                clip: true
                spacing: 2
                model: buildGroupModel()

                delegate: Rectangle {
                    width: groupList.width
                    height: 36
                    color: {
                        if (modelData.isActive)
                            return Qt.rgba(0.024, 0.714, 0.831, 0.08)
                        if (grpMouse.containsMouse)
                            return Qt.rgba(1, 1, 1, 0.03)
                        return "transparent"
                    }

                    // 激活指示条
                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                        width: 2
                        color: modelData.isActive ? "#06b6d4" : "transparent"
                    }

                    // 图标 + 文字
                    Row {
                        anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                        spacing: 8

                        Text {
                            text: "📁"
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: modelData.label
                            font.pixelSize: 13
                            color: modelData.isActive ? "#06b6d4" : "#94a3b8"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // 计数徽章
                    Rectangle {
                        anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
                        width: badgeText.implicitWidth + 16
                        height: 20
                        radius: 10
                        color: modelData.isActive
                            ? Qt.rgba(0.024, 0.714, 0.831, 0.2)
                            : "#1f2937"

                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: modelData.count
                            font.pixelSize: 11
                            color: modelData.isActive ? "#06b6d4" : "#64748b"
                        }
                    }

                    MouseArea {
                        id: grpMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            activeGroup = modelData.groupKey
                            deviceModel.filterGroup = modelData.groupKey
                            groupList.model = buildGroupModel()
                        }
                    }
                }
            }
        }

        // ─────────────────────────────────────
        // 右侧：工具栏 + 设备列表
        // ─────────────────────────────────────
        Rectangle {
            id: contentArea
            anchors {
                left: sidebar.right; right: parent.right
                top: parent.top; bottom: parent.bottom
            }
            color: "#1a1a2e"

            // ── 列宽：按可用宽度等比缩放，最大化不空 ──
            readonly property real colS: (width - 12) / 640.0
            readonly property real cwStatus:  Math.max(38,  colS * 50)
            readonly property real cwName:    Math.max(80,  colS * 130)
            readonly property real cwGroup:   Math.max(44,  colS * 70)
            readonly property real cwTemp:    Math.max(44,  colS * 65)
            readonly property real cwMem:     Math.max(36,  colS * 55)
            readonly property real cwDisk:    Math.max(56,  colS * 85)
            readonly property real cwVersion: Math.max(44,  colS * 65)
            readonly property real cwActions: Math.max(90,  colS * 120)

            // ── 工具栏 ──
            Rectangle {
                id: toolbar
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 50
                color: "transparent"

                Row {
                    anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                    spacing: 8

                    // 搜索框
                    Rectangle {
                        width: 220; height: 32
                        radius: 8
                        color: "#111827"
                        border { width: 1; color: "#334155" }

                        Row {
                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                            spacing: 8
                            Text {
                                text: "🔍"
                                font.pixelSize: 13
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            TextField {
                                id: searchInput
                                width: 170
                                font.pixelSize: 13
                                color: "#e2e8f0"
                                placeholderText: "搜索设备名称..."
                                placeholderTextColor: "#64748b"
                                anchors.verticalCenter: parent.verticalCenter

                                background: Item {}  // 透明背景，用外层 Rectangle

                                onTextChanged: {
                                    if (deviceModel)
                                        deviceModel.searchText = text
                                }
                            }
                        }
                    }
                }

                Row {
                    anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                    spacing: 8

                    // 刷新按钮
                    Rectangle {
                        width: 72; height: 32
                        radius: 7
                        color: "transparent"
                        border { width: 1; color: "#334155" }

                        Text {
                            anchors.centerIn: parent
                            text: "🔄 刷新"
                            font.pixelSize: 12
                            color: "#94a3b8"
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: NetworkManager.fetchEmbeddedDevices()
                        }
                    }

                    // 添加设备按钮
                    Rectangle {
                        width: 96; height: 32
                        radius: 7
                        color: "#3b82f6"

                        Text {
                            anchors.centerIn: parent
                            text: "+ 添加设备"
                            font.pixelSize: 12
                            color: "#ffffff"
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }
            }

            // ── 设备表格 ──
            ListView {
                id: deviceList
                anchors {
                    left: parent.left; right: parent.right
                    top: toolbar.bottom; bottom: parent.bottom
                }
                clip: true
                model: deviceModel
                spacing: 0

                // 表头
                header: Rectangle {
                    width: deviceList.width
                    height: 38
                    color: "#1a1a2e"
                    z: 2

                    // 底部分隔线
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 1
                        color: "#334155"
                    }

                    Row {
                        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                        spacing: 0

                        HeaderCell { label: "状态";       cellWidth: contentArea.cwStatus }
                        HeaderCell { label: "设备名称";   cellWidth: contentArea.cwName }
                        HeaderCell { label: "分组";       cellWidth: contentArea.cwGroup }
                        HeaderCell { label: "CPU温度";    cellWidth: contentArea.cwTemp }
                        HeaderCell { label: "内存";       cellWidth: contentArea.cwMem }
                        HeaderCell { label: "磁盘剩余";   cellWidth: contentArea.cwDisk }
                        HeaderCell { label: "版本";       cellWidth: contentArea.cwVersion }
                        HeaderCell { label: "操作";       cellWidth: contentArea.cwActions }
                    }
                }

                // 行代理
                delegate: Rectangle {
                    id: rowRect
                    width: deviceList.width
                    height: 44
                    color: rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.02) : "transparent"

                    // 行底部分隔线
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 1
                        color: Qt.rgba(0.2, 0.255, 0.333, 0.4)
                    }

                    // 行 hover / 右键菜单
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                contextMenuDeviceName = deviceName
                                contextMenuDeviceUId = deviceUId
                                contextMenuDeviceId = deviceId
                                deviceContextMenu.popup(rowMouse, mouse.x, mouse.y)
                            } else {
                                mouse.accepted = false
                            }
                        }
                    }

                    Row {
                        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                        spacing: 0

                        // ── 状态列 ──
                        Row {
                            width: contentArea.cwStatus
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 5
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: statusColor
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: statusText
                                font.pixelSize: 12
                                color: statusColor
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // ── 设备名称 ──
                        Text {
                            width: contentArea.cwName
                            text: deviceName
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: status === "warning" ? "#eab308" : "#e2e8f0"
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                        }

                        // ── 分组 ──
                        Text {
                            width: contentArea.cwGroup
                            text: deviceGroup
                            font.pixelSize: 13
                            color: "#94a3b8"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // ── CPU 温度 ──
                        Text {
                            width: contentArea.cwTemp
                            text: temperature + "°C"
                            font.pixelSize: 13
                            color: status === "warning" ? "#eab308" : "#94a3b8"
                            font.weight: status === "warning" ? Font.DemiBold : Font.Normal
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // ── 内存 ──
                        Text {
                            width: contentArea.cwMem
                            text: memUsage + "%"
                            font.pixelSize: 13
                            color: status === "warning" ? "#eab308" : "#94a3b8"
                            font.weight: status === "warning" ? Font.DemiBold : Font.Normal
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // ── 磁盘剩余 ──
                        Text {
                            width: contentArea.cwDisk
                            text: formatDisk(diskFree)
                            font.pixelSize: 13
                            color: "#94a3b8"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // ── 版本 ──
                        Text {
                            width: contentArea.cwVersion
                            text: deviceVersion
                            font.pixelSize: 13
                            color: "#94a3b8"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // ── 操作按钮 ──
                        Row {
                            width: contentArea.cwActions
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            // 📸 截屏
                            ActionBtn {
                                icon: "📸"; tooltip: "截屏"
                                onClicked: NetworkManager.requestScreenshot(deviceUId)
                            }
                            // 🎯 推送
                            ActionBtn {
                                icon: "🎯"; tooltip: "推送"
                                onClicked: NetworkManager.requestPushToDevice(deviceName)
                            }
                            // ✏️ 编辑
                            ActionBtn {
                                icon: "✏️"; tooltip: "编辑"
                                onClicked: {
                                    editDeviceName = deviceName
                                    editDeviceGroup = deviceGroup
                                    editDeviceId = deviceId
                                    editDeviceUId = deviceUId
                                    showEditDialog = true
                                }
                            }
                        }
                    }
                }

                // 空状态
                footer: Item {
                    width: deviceList.width
                    height: deviceList.count === 0 ? 200 : 0
                    visible: deviceList.count === 0

                    Text {
                        anchors.centerIn: parent
                        text: "暂无设备数据\n请确认已连接服务器"
                        font.pixelSize: 16
                        color: "#64748b"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════
    // 辅助函数：构建分组模型
    // ═══════════════════════════════════════
    function buildGroupModel() {
        var list = []

        list.push({
            groupKey: "",
            label: "全部设备",
            count: deviceModel ? deviceModel.groupDeviceCount("") : 0,
            isActive: activeGroup === ""
        })

        if (deviceModel) {
            var names = deviceModel.groupNames  // Q_PROPERTY access
            if (names && names.length > 0) {
                for (var i = 0; i < names.length; i++) {
                    var g = names[i]
                    list.push({
                        groupKey: g,
                        label: g,
                        count: deviceModel.groupDeviceCount(g),
                        isActive: activeGroup === g
                    })
                }
            }
        }
        return list
    }

    // ═══════════════════════════════════════
    // 辅助函数：格式化磁盘大小
    // ═══════════════════════════════════════
    function formatDisk(mb) {
        if (mb >= 1024)
            return (mb / 1024).toFixed(1) + " GB"
        return mb + " MB"
    }

    // ═══════════════════════════════════════
    // 子组件：表头单元格
    // ═══════════════════════════════════════
    component HeaderCell: Item {
        property alias label: hdrText.text
        property real cellWidth: 70
        width: cellWidth
        height: 38

        Text {
            id: hdrText
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            text: ""
            font.pixelSize: 11
            font.weight: Font.Medium
            color: "#64748b"
        }
    }

    // ═══════════════════════════════════════
    // 子组件：操作按钮
    // ═══════════════════════════════════════
    component ActionBtn: Rectangle {
        property string icon: ""
        property string tooltip: ""
        signal clicked()
        width: 28; height: 28
        radius: 6
        color: ma.containsMouse ? "#1f2937" : "transparent"

        Text {
            anchors.centerIn: parent
            text: icon
            font.pixelSize: 14
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    // ═══════════════════════════════════════
    // 编辑弹窗需要的临时属性
    // ═══════════════════════════════════════
    property int    editDeviceId: 0
    property string editDeviceUId: ""
    property string editDeviceName: ""
    property string editDeviceGroup: ""

    property bool showEditDialog: false

    // ═══════════════════════════════════════
    // ✏️ 编辑设备遮罩
    // ═══════════════════════════════════════
    Rectangle {
        id: editOverlay
        anchors.fill: parent
        color: "#80000000"
        visible: showEditDialog
        z: 100

        MouseArea { anchors.fill: parent } // 阻止点击穿透

        Rectangle {
            width: 400; height: 200
            anchors.centerIn: parent
            radius: 12; color: "#1f2937"
            border { width: 1; color: "#334155" }

            Column {
                anchors { fill: parent; margins: 20 }
                spacing: 14

                Text {
                    text: "✏️ 编辑设备 — " + editDeviceName
                    font.pixelSize: 16; font.weight: Font.DemiBold; color: "#e2e8f0"
                }

                Row {
                    spacing: 12
                    Text { text: "设备名称"; font.pixelSize: 13; color: "#94a3b8"
                        width: 60; anchors.verticalCenter: parent.verticalCenter }
                    Rectangle {
                        width: 260; height: 34; radius: 6
                        color: "#111827"; border { width: 1; color: "#334155" }
                        TextInput {
                            id: editNameInput
                            anchors { fill: parent; margins: 8 }
                            font.pixelSize: 13; color: "#e2e8f0"
                            text: editDeviceName
                        }
                    }
                }

                Row {
                    spacing: 12
                    Text { text: "所属分组"; font.pixelSize: 13; color: "#94a3b8"
                        width: 60; anchors.verticalCenter: parent.verticalCenter }
                    Rectangle {
                        width: 260; height: 34; radius: 6
                        color: "#111827"; border { width: 1; color: "#334155" }
                        TextInput {
                            id: editGroupInput
                            anchors { fill: parent; margins: 8 }
                            font.pixelSize: 13; color: "#e2e8f0"
                            text: editDeviceGroup
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    spacing: 8
                    Rectangle {
                        width: 72; height: 32; radius: 7
                        color: "transparent"; border { width: 1; color: "#334155" }
                        Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: 12; color: "#94a3b8" }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: showEditDialog = false
                        }
                    }
                    Rectangle {
                        width: 72; height: 32; radius: 7; color: "#3b82f6"
                        Text { anchors.centerIn: parent; text: "保存"; font.pixelSize: 12; color: "#fff" }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                // TODO: 发送编辑请求到服务器
                                showEditDialog = false

                                NetworkManager.requestEditEmbeddedMessage(editDeviceUId, editGroupInput.text, editNameInput.text)
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════
    // Toast 提示（2s 自动消失）
    // ═══════════════════════════════════════
    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 30 }
        width: toastText.implicitWidth + 40; height: 40
        radius: 8; color: "#22c55e"
        visible: showToast
        z: 200
        opacity: showToast ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 300 } }

        Text {
            id: toastText
            anchors.centerIn: parent
            text: toastMsg
            font.pixelSize: 14; font.weight: Font.DemiBold; color: "#ffffff"
        }
    }
}
