import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs

// 页面 4：OTA 升级
Rectangle {
    id: root
    color: "#1a1a2e"

    property var deviceModel: NetworkManager.deviceModel
    property string availableVersion: "暂无"
    property string packageSize: "--"
    property string firmwareChangelog: ""

    property string otaStrategy: "manual"   // full | manual
    property var checkedDeviceList: []
    property var otaOnlineDeviceList: []
    property string selectedFirmwarePath: ""
    property string selectedFirmwareName: ""
    property string selectedFirmwareMd5: ""
    property bool firmwareMd5Computing: false
    property bool hasSelectedFirmware: selectedFirmwareName !== ""
    property bool firmwareUploading: false

    property string leftPanelMode: "list"   // list | firmware | device
    property var selectedServerFirmware: null
    property bool hasSelectedServerFirmware: selectedServerFirmware !== null
    property var selectedDetailDevice: null
    property var upgradeProgressByUid: ({})

    property string uiFont: "Microsoft YaHei"

    property string toastMsg: ""
    property bool showToast: false
    property bool toastOk: true

    Timer {
        id: toastTimer
        interval: 2000
        onTriggered: showToast = false
    }

    ListModel {
        id: firmwareModel
        dynamicRoles: true
    }

    function refreshOtaTargets() {
        if (!deviceModel)
            return
        otaOnlineDeviceList = deviceModel.onlineDeviceList()

        var onlineUids = []
        for (var i = 0; i < otaOnlineDeviceList.length; i++)
            onlineUids.push(otaOnlineDeviceList[i].deviceUId)

        var checked = checkedDeviceList.slice()
        for (var k = checked.length - 1; k >= 0; k--) {
            if (onlineUids.indexOf(checked[k]) < 0)
                checked.splice(k, 1)
        }
        checkedDeviceList = checked

        if (selectedDetailDevice) {
            var found = false
            for (var j = 0; j < otaOnlineDeviceList.length; j++) {
                if (otaOnlineDeviceList[j].deviceUId === selectedDetailDevice.deviceUId) {
                    selectedDetailDevice = otaOnlineDeviceList[j]
                    found = true
                    break
                }
            }
            if (!found) {
                selectedDetailDevice = null
                if (leftPanelMode === "device")
                    leftPanelMode = hasSelectedServerFirmware ? "firmware" : "list"
            }
        }
    }

    function formatSize(bytes) {
        if (bytes < 1024)
            return bytes + " B"
        if (bytes < 1024 * 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1024 / 1024).toFixed(1) + " MB"
    }

    function formatMd5Short(md5) {
        if (!md5)
            return "—"
        if (md5.length <= 16)
            return md5
        return md5.substring(0, 8) + "…" + md5.substring(md5.length - 8)
    }

    function executablesText(list) {
        if (!list || list.length === 0)
            return "—"
        var names = []
        for (var i = 0; i < list.length; i++)
            names.push(list[i])
        return names.join("、")
    }

    function filesDetailText(files) {
        if (!files || files.length === 0)
            return "—"
        var lines = []
        for (var j = 0; j < files.length; j++)
            lines.push(files[j].name + "  " + formatSize(files[j].size || 0))
        return lines.join("\n")
    }

    function firmwareItemFromRaw(f) {
        return {
            name: f.name || "",
            path: f.path || "",
            size: formatSize(f.size || 0),
            sizeBytes: f.size || 0,
            version: f.version || "",
            packTime: f.pack_time || "",
            changelog: f.changelog || "",
            executables: f.executables || [],
            files: f.files || []
        }
    }

    function applyFirmwareDetail(item) {
        selectedServerFirmware = item
        availableVersion = formatVersion(item.version)
        packageSize = item.size
        firmwareChangelog = item.changelog || ""
    }

    function firmwareDetailRows() {
        if (!selectedServerFirmware)
            return []
        return [
            { label: "固件文件",   value: selectedServerFirmware.name, highlight: true },
            { label: "存储路径",   value: selectedServerFirmware.path, highlight: false },
            { label: "目标版本",   value: availableVersion, highlight: availableVersion !== "暂无" },
            { label: "升级包大小", value: packageSize, highlight: false },
            { label: "打包时间",   value: selectedServerFirmware.packTime || "—", highlight: false },
            { label: "包含程序",   value: executablesText(selectedServerFirmware.executables), highlight: false },
            { label: "程序大小",   value: filesDetailText(selectedServerFirmware.files), highlight: false }
        ]
    }

    function loadFirmwareList(firmwares) {
        firmwareModel.clear()
        var selectedPath = selectedServerFirmware ? selectedServerFirmware.path : ""
        var stillExists = false

        for (var i = 0; i < firmwares.length; i++) {
            var item = firmwareItemFromRaw(firmwares[i])
            item.id = i + 1
            firmwareModel.append(item)
            if (item.path && item.path === selectedPath) {
                stillExists = true
                applyFirmwareDetail(item)
            }
        }

        if (selectedServerFirmware && !stillExists) {
            selectedServerFirmware = null
            firmwareChangelog = ""
            availableVersion = "暂无"
            packageSize = "--"
            if (leftPanelMode === "firmware")
                leftPanelMode = "list"
        }
    }

    function formatVersion(ver) {
        if (!ver)
            return "暂无"
        return ver.indexOf("v") === 0 ? ver : ("v" + ver)
    }

    function selectServerFirmwareByIndex(index) {
        applyFirmwareDetail({
            name: firmwareModel.get(index).name,
            path: firmwareModel.get(index).path,
            size: firmwareModel.get(index).size,
            sizeBytes: firmwareModel.get(index).sizeBytes,
            version: firmwareModel.get(index).version,
            packTime: firmwareModel.get(index).packTime,
            changelog: firmwareModel.get(index).changelog,
            executables: firmwareModel.get(index).executables,
            files: firmwareModel.get(index).files
        })
        leftPanelMode = "firmware"
    }

    function isServerFirmwareSelected(path) {
        return hasSelectedServerFirmware && selectedServerFirmware.path === path
    }

    function isDeviceChecked(uid) {
        return checkedDeviceList.indexOf(uid) >= 0
    }

    function toggleDevice(uid) {
        var arr = checkedDeviceList.slice()
        var idx = arr.indexOf(uid)
        if (idx >= 0)
            arr.splice(idx, 1)
        else
            arr.push(uid)
        checkedDeviceList = arr
    }

    function deviceProgress(uid) {
        var p = upgradeProgressByUid[uid]
        return (p === undefined || p === null) ? -1 : p
    }

    function progressLabel(uid) {
        var p = deviceProgress(uid)
        return p < 0 ? "—" : (p + "%")
    }

    function selectDeviceDetail(item) {
        selectedDetailDevice = item
        leftPanelMode = "device"
    }

    function statusText(status) {
        if (status === "warning")
            return "告警"
        if (status === "offline")
            return "离线"
        return "在线"
    }

    function targetCount() {
        if (!deviceModel)
            return 0
        if (otaStrategy === "full")
            return deviceModel.onlineDeviceCount()
        return checkedDeviceList.length
    }

    function canStartUpgrade() {
        return hasSelectedServerFirmware && targetCount() > 0
    }

    function selectedOtaDeviceUids() {
        if (!deviceModel)
            return []

        if (otaStrategy === "full") {
            var onlineList = deviceModel.onlineDeviceList()
            var uids = []
            for (var i = 0; i < onlineList.length; i++)
                uids.push(onlineList[i].deviceUId)
            return uids
        }

        return checkedDeviceList.slice()
    }

    function startOtaUpgrade() {
        if (!canStartUpgrade() || !selectedServerFirmware)
            return

        var deviceUids = selectedOtaDeviceUids()
        if (deviceUids.length === 0)
            return

        var otaPath = selectedServerFirmware.path || ""
        if (otaPath !== "" && otaPath.indexOf("/") !== 0)
            otaPath = "/" + otaPath

        console.log("[OTA] start upgrade", otaStrategy, deviceUids.length, otaPath)
        NetworkManager.requestOTAUpdateToEmbedded(deviceUids, otaPath)
    }

    function strategyButtonLabel() {
        if (otaStrategy === "full")
            return "⬆ 开始全量升级 (" + targetCount() + " 台)"
        return "⬆ 开始升级 (" + targetCount() + " 台)"
    }

    Component.onCompleted: {
        refreshOtaTargets()
        NetworkManager.requestServerFirmwareList()
    }

    Connections {
        target: deviceModel
        function onCountsChanged() { refreshOtaTargets() }
    }

    Connections {
        target: NetworkManager
        function onServerFirmwareListReceived(firmwares) {
            loadFirmwareList(firmwares)
        }
        function onCheckFirmwareAck(result) {
            toastOk = (result === 1)
            toastMsg = toastOk ? "固件校验通过" : "固件校验失败"
            showToast = true
            toastTimer.restart()
        }
        function onOtaUpdateAck(result) {
            toastOk = (result === 1)
            toastMsg = toastOk ? "固件升级成功" : "固件升级失败"
            showToast = true
            toastTimer.restart()
        }
    }

    Connections {
        target: FileUploader
        function onTaskStarted(taskName, taskSize) {
            if (!firmwareUploading)
                return
            console.log("[OTA] upload started", taskName, taskSize)
        }
        function onTaskProgress(taskName, sent, total) {
            if (!firmwareUploading)
                return
            console.log("[OTA] upload progress", taskName, sent, total)
        }
        function onTaskFinished(taskName, taskServerPath, taskServerUrl) {
            if (!firmwareUploading)
                return
            firmwareUploading = false
            console.log("[OTA] upload finished", taskName, taskServerPath, taskServerUrl)
            if (selectedFirmwareMd5 !== "")
                NetworkManager.requestCheckFirmware(taskServerPath, selectedFirmwareMd5)
            NetworkManager.requestServerFirmwareList()
        }
        function onTaskFailed(taskName, message) {
            if (!firmwareUploading)
                return
            firmwareUploading = false
            console.log("[OTA] upload failed", taskName, message)
        }
        function onFileMd5Computed(filePath, md5) {
            if (filePath !== selectedFirmwarePath)
                return
            firmwareMd5Computing = false
            selectedFirmwareMd5 = md5
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: otaLayout.implicitHeight + 40
        clip: true

        Row {
            id: otaLayout
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 20
            }
            spacing: 20

            // ── 左侧：固件信息 / 设备详情 ──
            Column {
                width: (parent.width - 20) * 0.58
                spacing: 16

                Item {
                    width: parent.width
                    implicitHeight: 26

                    Text {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                        text: leftPanelMode === "device" ? "设备详情"
                              : (leftPanelMode === "firmware" ? "固件版本信息" : "服务器固件库")
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "#e2e8f0"
                    }

                    Rectangle {
                        visible: leftPanelMode === "list"
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        width: refreshLbl.implicitWidth + 16
                        implicitHeight: 26
                        radius: 6
                        color: refreshMa.containsMouse ? "#334155" : "transparent"
                        border { width: 1; color: "#334155" }

                        Text {
                            id: refreshLbl
                            anchors.centerIn: parent
                            text: "🔄 刷新"
                            font.pixelSize: 11
                            color: "#94a3b8"
                        }

                        MouseArea {
                            id: refreshMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: NetworkManager.requestServerFirmwareList()
                        }
                    }

                    Rectangle {
                        visible: leftPanelMode === "firmware" || leftPanelMode === "device"
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        width: backLbl.implicitWidth + 16
                        implicitHeight: 26
                        radius: 6
                        color: backMa.containsMouse ? "#334155" : "transparent"
                        border { width: 1; color: "#334155" }

                        Text {
                            id: backLbl
                            anchors.centerIn: parent
                            text: leftPanelMode === "device" ? "← 返回" : "← 固件列表"
                            font.pixelSize: 11
                            color: "#94a3b8"
                        }

                        MouseArea {
                            id: backMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (leftPanelMode === "device")
                                    leftPanelMode = hasSelectedServerFirmware ? "firmware" : "list"
                                else
                                    leftPanelMode = "list"
                            }
                        }
                    }
                }

                Text {
                    visible: leftPanelMode === "list"
                    text: "共 " + firmwareModel.count + " 个固件"
                    font.pixelSize: 11
                    color: "#64748b"
                }

                // 服务器固件列表
                Rectangle {
                    width: parent.width
                    visible: leftPanelMode === "list"
                    implicitHeight: Math.max(280, firmwareListArea.implicitHeight + 16)
                    radius: 10
                    color: "#1f2937"
                    border { width: 1; color: "#334155" }
                    clip: true

                    Item {
                        id: firmwareListArea
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 12
                        }
                        implicitHeight: firmwareModel.count === 0 ? 256 : fwFlow.implicitHeight

                        // 空状态
                        Column {
                            anchors.centerIn: parent
                            spacing: 10
                            visible: firmwareModel.count === 0
                            width: parent.width - 24

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "📭"
                                font.pixelSize: 56
                                opacity: 0.5
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "服务器固件库为空"
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: "#94a3b8"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: "请通过右侧上传固件到服务器 /firmwares 目录"
                                font.pixelSize: 13
                                color: "#64748b"
                            }
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: fwEmptyTag.implicitWidth + 32
                                height: 28
                                radius: 14
                                color: "#111827"
                                border { width: 1; color: "#334155" }
                                Text {
                                    id: fwEmptyTag
                                    anchors.centerIn: parent
                                    text: "支持 .tar.gz · .tgz · .zip · .bin"
                                    font.pixelSize: 11
                                    color: "#64748b"
                                }
                            }
                        }

                        Flow {
                            id: fwFlow
                            width: parent.width
                            spacing: 10
                            visible: firmwareModel.count > 0

                            Repeater {
                                model: firmwareModel

                                delegate: Rectangle {
                                    width: (fwFlow.width - 10) / 2
                                    implicitHeight: 90
                                    radius: 10
                                    color: "#111827"
                                    border {
                                        width: root.isServerFirmwareSelected(model.path) ? 2 : 1
                                        color: root.isServerFirmwareSelected(model.path) ? "#06b6d4" : "#334155"
                                    }

                                    Column {
                                        anchors {
                                            fill: parent
                                            margins: 12
                                        }
                                        spacing: 6

                                        Row {
                                            width: parent.width
                                            spacing: 8
                                            Text { text: "📦"; font.pixelSize: 18 }
                                            Text {
                                                width: parent.width - 26
                                                text: model.name
                                                font.bold: true
                                                font.pixelSize: 13
                                                font.family: root.uiFont
                                                font.weight: Font.DemiBold
                                                color: "#e2e8f0"
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        Row {
                                            width: parent.width
                                            spacing: 10

                                            Text {
                                                text: model.version ? formatVersion(model.version) : "—"
                                                font.pixelSize: 11
                                                font.family: root.uiFont
                                                font.weight: Font.Bold
                                                color: "#06b6d4"
                                            }

                                            Text {
                                                text: model.size
                                                font.pixelSize: 11
                                                font.family: root.uiFont
                                                color: "#94a3b8"
                                            }

                                        }

                                        Text {
                                            visible: model.packTime !== ""
                                            text: model.packTime
                                            font.pixelSize: 10
                                            font.family: root.uiFont
                                            color: "#64748b"
                                            elide: Text.ElideRight
                                            width: parent.width
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.selectServerFirmwareByIndex(index)
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: leftPanelMode === "list"
                    width: parent.width
                    text: "提示：点击固件卡片查看版本详情"
                    font.pixelSize: 11
                    color: "#475569"
                }

                // 固件版本信息
                Rectangle {
                    width: parent.width
                    visible: leftPanelMode === "firmware" && hasSelectedServerFirmware
                    implicitHeight: firmwareCol.implicitHeight + 36
                    radius: 10
                    color: "#1f2937"
                    border { width: 1; color: "#334155" }

                    Column {
                        id: firmwareCol
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 18
                        }
                        spacing: 0

                        Repeater {
                            model: firmwareDetailRows()

                            delegate: Column {
                                width: firmwareCol.width
                                spacing: 0

                                Item {
                                    width: parent.width
                                    implicitHeight: 40

                                    Text {
                                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                                        text: modelData.label
                                        font.pixelSize: 12
                                        font.family: root.uiFont
                                        color: "#64748b"
                                    }

                                    Text {
                                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                        width: parent.width * 0.62
                                        horizontalAlignment: Text.AlignRight
                                        text: modelData.value
                                        font.pixelSize: 13
                                        font.family: root.uiFont
                                        font.weight: modelData.highlight ? Font.Bold : Font.Medium
                                        color: modelData.highlight ? "#06b6d4" : "#e2e8f0"
                                        elide: Text.ElideMiddle
                                    }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    visible: index < firmwareDetailRows().length - 1
                                    color: Qt.rgba(51/255, 65/255, 85/255, 0.3)
                                }
                            }
                        }
                    }
                }

                // 设备详情
                Rectangle {
                    width: parent.width
                    visible: leftPanelMode === "device" && selectedDetailDevice !== null
                    implicitHeight: deviceDetailCol.implicitHeight + 36
                    radius: 10
                    color: "#1f2937"
                    border { width: 1; color: "#06b6d4" }

                    Column {
                        id: deviceDetailCol
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 18
                        }
                        spacing: 0

                        Repeater {
                            model: selectedDetailDevice ? [
                                { label: "设备名称", value: selectedDetailDevice.deviceName, highlight: true },
                                { label: "所属分组", value: selectedDetailDevice.deviceGroup || "—", highlight: false },
                                { label: "当前版本", value: selectedDetailDevice.deviceVersion || "—", highlight: true },
                                { label: "运行状态", value: statusText(selectedDetailDevice.status), highlight: false },
                                { label: "升级进度", value: progressLabel(selectedDetailDevice.deviceUId), highlight: deviceProgress(selectedDetailDevice.deviceUId) >= 0 },
                                { label: "目标固件", value: hasSelectedServerFirmware ? formatVersion(selectedServerFirmware.version) + " · " + selectedServerFirmware.name : "未选择", highlight: hasSelectedServerFirmware }
                            ] : []

                            delegate: Column {
                                width: deviceDetailCol.width
                                spacing: 0

                                Item {
                                    width: parent.width
                                    implicitHeight: 40

                                    Text {
                                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                                        text: modelData.label
                                        font.pixelSize: 12
                                        color: "#64748b"
                                    }

                                    Text {
                                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                        width: parent.width * 0.62
                                        horizontalAlignment: Text.AlignRight
                                        text: modelData.value
                                        font.pixelSize: 13
                                        font.weight: modelData.highlight ? Font.Bold : Font.Medium
                                        color: modelData.highlight ? "#06b6d4" : "#e2e8f0"
                                        elide: Text.ElideMiddle
                                    }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    visible: index < 5
                                    color: Qt.rgba(51/255, 65/255, 85/255, 0.3)
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: leftPanelMode === "firmware"
                    text: "更新日志"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: "#e2e8f0"
                }

                Rectangle {
                    width: parent.width
                    visible: leftPanelMode === "firmware"
                    implicitHeight: Math.max(120, changelogText.implicitHeight + 32)
                    radius: 10
                    color: "#1f2937"
                    border { width: 1; color: "#334155" }

                    Text {
                        id: changelogText
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 16
                        }
                        width: parent.width - 32
                        wrapMode: Text.WordWrap
                        text: firmwareChangelog !== ""
                              ? firmwareChangelog
                              : "暂无更新日志"
                        font.pixelSize: 12
                        font.family: root.uiFont
                        color: "#94a3b8"
                        lineHeight: 1.6
                    }
                }

                Text {
                    visible: leftPanelMode === "firmware"
                    width: parent.width
                    text: "提示：双击右侧在线设备可查看详情"
                    font.pixelSize: 11
                    color: "#475569"
                }
            }

            // ── 右侧：策略 + 设备列表 + 选固件 ──
            Column {
                width: (parent.width - 20) * 0.42
                spacing: 16

                Text {
                    text: "升级策略"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: "#64748b"
                }

                Row {
                    spacing: 8

                    Repeater {
                        model: [
                            { k: "full",   l: "全量升级" },
                            { k: "manual", l: "手选设备" }
                        ]

                        delegate: Rectangle {
                            width: strategyTxt.implicitWidth + 24
                            implicitHeight: 32
                            radius: 7
                            color: otaStrategy === modelData.k ? "#3b82f6" : "transparent"
                            border {
                                width: 1
                                color: otaStrategy === modelData.k ? "#3b82f6" : "#334155"
                            }

                            Text {
                                id: strategyTxt
                                anchors.centerIn: parent
                                text: modelData.l
                                font.pixelSize: 11
                                font.weight: otaStrategy === modelData.k ? Font.Bold : Font.Normal
                                color: otaStrategy === modelData.k ? "#ffffff" : "#94a3b8"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: otaStrategy = modelData.k
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    font.pixelSize: 11
                    color: "#64748b"
                    wrapMode: Text.WordWrap
                    text: otaStrategy === "full"
                          ? "全量：将对全部 " + (deviceModel ? deviceModel.onlineDeviceCount() : 0) + " 台在线设备升级"
                          : "单击勾选升级目标 · 双击查看左侧详情"
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: 48
                    radius: 10
                    opacity: canStartUpgrade() ? 1.0 : 0.45

                    gradient: Gradient {
                        GradientStop { position: 0; color: "#8b5cf6" }
                        GradientStop { position: 1; color: "#3b82f6" }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: !hasSelectedServerFirmware
                              ? "⬆ 请先选择服务器固件"
                              : strategyButtonLabel()
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "#ffffff"
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: canStartUpgrade()
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: root.startOtaUpgrade()
                    }
                }

                Text {
                    text: "在线设备"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: "#64748b"
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: Math.max(200, deviceTable.implicitHeight + 8)
                    radius: 8
                    color: "#111827"
                    border { width: 1; color: "#334155" }
                    clip: true

                    Column {
                        id: deviceTable
                        width: parent.width

                        Rectangle {
                            width: parent.width
                            height: 34
                            color: "#16213e"

                            Row {
                                anchors {
                                    fill: parent
                                    leftMargin: 10
                                    rightMargin: 10
                                }
                                spacing: 6

                                Text {
                                    width: otaStrategy === "full" ? 0 : 22
                                    visible: otaStrategy !== "full"
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "✓"
                                    font.pixelSize: 10
                                    color: "#64748b"
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    width: (deviceTable.width - (otaStrategy === "full" ? 20 : 42)) * 0.38
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "设备"
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: "#64748b"
                                }

                                Text {
                                    width: (deviceTable.width - (otaStrategy === "full" ? 20 : 42)) * 0.24
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "当前版本"
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: "#64748b"
                                }

                                Text {
                                    width: (deviceTable.width - (otaStrategy === "full" ? 20 : 42)) * 0.38
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "升级进度"
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: "#64748b"
                                }
                            }
                        }

                        Repeater {
                            model: otaOnlineDeviceList

                            delegate: Rectangle {
                                width: deviceTable.width
                                implicitHeight: 40
                                color: rowMa.containsMouse
                                       ? Qt.rgba(255, 255, 255, 0.03)
                                       : (selectedDetailDevice
                                          && selectedDetailDevice.deviceUId === modelData.deviceUId
                                          ? Qt.rgba(6/255, 182/255, 212/255, 0.08)
                                          : "transparent")

                                property real rowProgress: root.deviceProgress(modelData.deviceUId)
                                property real contentW: deviceTable.width - (otaStrategy === "full" ? 20 : 42)

                                Row {
                                    anchors {
                                        fill: parent
                                        leftMargin: 10
                                        rightMargin: 10
                                    }
                                    spacing: 6

                                    Rectangle {
                                        width: 16
                                        height: 16
                                        visible: otaStrategy !== "full"
                                        radius: 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: root.isDeviceChecked(modelData.deviceUId) ? "#06b6d4" : "#111827"
                                        border {
                                            width: 1.5
                                            color: root.isDeviceChecked(modelData.deviceUId) ? "#06b6d4" : "#334155"
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: root.isDeviceChecked(modelData.deviceUId) ? "✓" : ""
                                            font.pixelSize: 9
                                            font.weight: Font.Bold
                                            color: "#0f172a"
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.toggleDevice(modelData.deviceUId)
                                        }
                                    }

                                    Text {
                                        width: contentW * 0.38
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.deviceName
                                        font.pixelSize: 12
                                        color: "#e2e8f0"
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: contentW * 0.24
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.deviceVersion || "—"
                                        font.pixelSize: 12
                                        color: "#94a3b8"
                                        elide: Text.ElideRight
                                    }

                                    Item {
                                        width: contentW * 0.38
                                        height: parent.height

                                        Row {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width
                                            spacing: 6
                                            visible: rowProgress >= 0

                                            Rectangle {
                                                width: parent.width - 36
                                                height: 6
                                                radius: 3
                                                color: "#0f172a"
                                                anchors.verticalCenter: parent.verticalCenter

                                                Rectangle {
                                                    width: parent.width * Math.min(rowProgress / 100, 1)
                                                    height: parent.height
                                                    radius: 3
                                                    color: rowProgress >= 100 ? "#22c55e" : "#3b82f6"
                                                }
                                            }

                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: rowProgress + "%"
                                                font.pixelSize: 11
                                                font.weight: Font.Bold
                                                color: rowProgress >= 100 ? "#22c55e" : "#3b82f6"
                                            }
                                        }

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: rowProgress < 0
                                            text: "—"
                                            font.pixelSize: 12
                                            color: "#475569"
                                        }
                                    }
                                }

                                MouseArea {
                                    id: rowMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton
                                    onDoubleClicked: root.selectDeviceDetail(modelData)
                                }

                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    anchors.bottom: parent.bottom
                                    color: Qt.rgba(51/255, 65/255, 85/255, 0.3)
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            visible: otaOnlineDeviceList.length === 0
                            horizontalAlignment: Text.AlignHCenter
                            padding: 24
                            text: "暂无在线设备"
                            font.pixelSize: 12
                            color: "#64748b"
                        }
                    }
                }

                Item {
                    width: parent.width
                    implicitHeight: 26

                    Text {
                        id: firmwareSectionTitle
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                        text: hasSelectedFirmware ? "已选固件" : "选择固件"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: "#64748b"
                    }

                    Row {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        spacing: 6
                        visible: hasSelectedFirmware

                        Rectangle {
                            width: uploadBtnLbl.implicitWidth + 14
                            implicitHeight: 22
                            radius: 5
                            color: uploadBtnMa.containsMouse ? "#3b82f6" : "transparent"
                            border { width: 1; color: "#3b82f6" }

                            Text {
                                id: uploadBtnLbl
                                anchors.centerIn: parent
                                text: firmwareUploading ? "上传中..." : "上传"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                color: uploadBtnMa.containsMouse && !firmwareUploading ? "#ffffff" : "#3b82f6"
                            }

                            MouseArea {
                                id: uploadBtnMa
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: !firmwareUploading
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: root.uploadFirmware()
                            }
                        }

                        Rectangle {
                            width: reselectBtnLbl.implicitWidth + 14
                            implicitHeight: 22
                            radius: 5
                            color: reselectBtnMa.containsMouse ? "#334155" : "transparent"
                            border { width: 1; color: "#475569" }

                            Text {
                                id: reselectBtnLbl
                                anchors.centerIn: parent
                                text: "重新选择"
                                font.pixelSize: 10
                                color: reselectBtnMa.containsMouse ? "#e2e8f0" : "#94a3b8"
                            }

                            MouseArea {
                                id: reselectBtnMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.reselectFirmware()
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: 88
                    radius: 10
                    color: "#1f2937"
                    visible: !hasSelectedFirmware
                    border {
                        width: 2
                        color: uploadMa.containsMouse || uploadMa.drag.active ? "#475569" : "#334155"
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📦 拖拽固件包到此处 或 "
                                  + "<font color='#06b6d4'>浏览文件</font>"
                            font.pixelSize: 13
                            color: "#94a3b8"
                            textFormat: Text.RichText
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "支持 .tar.gz / .tgz / .zip / .bin"
                            font.pixelSize: 11
                            color: "#64748b"
                        }
                    }

                    MouseArea {
                        id: uploadMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: firmwareDialog.open()
                    }

                    DropArea {
                        anchors.fill: parent
                        onDropped: {
                            if (drop.hasUrls && drop.urls.length > 0)
                                root.onFirmwareSelected(drop.urls[0])
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: selectedFirmwareCol.implicitHeight + 28
                    radius: 10
                    color: "#1f2937"
                    visible: hasSelectedFirmware
                    border { width: 1; color: "#06b6d4" }

                    Row {
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 14
                        }
                        spacing: 12

                        Text {
                            text: "📦"
                            font.pixelSize: 28
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            id: selectedFirmwareCol
                            width: parent.width - 52
                            spacing: 4

                            Text {
                                text: "固件文件"
                                font.pixelSize: 11
                                font.family: root.uiFont
                                color: "#64748b"
                            }

                            Text {
                                width: parent.width
                                text: selectedFirmwareName
                                font.pixelSize: 13
                                font.family: root.uiFont
                                font.weight: Font.Bold
                                color: "#06b6d4"
                                elide: Text.ElideMiddle
                            }

                            Text {
                                width: parent.width
                                visible: firmwareMd5Computing || selectedFirmwareMd5 !== ""
                                text: firmwareMd5Computing
                                      ? "MD5校验：计算中…"
                                      : ("MD5校验：" + formatMd5Short(selectedFirmwareMd5))
                                font.pixelSize: 10
                                font.family: root.uiFont
                                color: "#64748b"
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: firmwareDialog
        title: "选择固件包"
        fileMode: FileDialog.OpenFile
        nameFilters: ["固件包 (*.tar.gz *.tgz *.zip *.bin)", "所有文件 (*)"]
        onAccepted: {
            if (selectedFiles.length > 0)
                root.onFirmwareSelected(selectedFiles[0])
        }
    }

    function clearSelectedFirmware() {
        selectedFirmwarePath = ""
        selectedFirmwareName = ""
        selectedFirmwareMd5 = ""
        firmwareMd5Computing = false
    }

    function uploadFirmware() {
        if (!hasSelectedFirmware || firmwareUploading)
            return
        firmwareUploading = true
        FileUploader.addFirmwareTask(selectedFirmwarePath)
    }

    function reselectFirmware() {
        clearSelectedFirmware()
        firmwareDialog.open()
    }

    function parseLocalPath(fileUrl) {
        var s = (fileUrl === undefined || fileUrl === null) ? "" : String(fileUrl)
        if (s.indexOf("file:///") === 0)
            s = s.substring(8)
        else if (s.indexOf("file://") === 0)
            s = s.substring(7)
        try {
            s = decodeURIComponent(s)
        } catch (e) { }
        return s
    }

    function onFirmwareSelected(fileUrl) {
        var path = parseLocalPath(fileUrl)
        if (!path)
            return

        selectedFirmwarePath = path
        var slash = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"))
        selectedFirmwareName = slash >= 0 ? path.substring(slash + 1) : path
        selectedFirmwareMd5 = ""
        firmwareMd5Computing = true
        FileUploader.requestFileMd5(path)

        console.log("[OTA] firmware selected:", selectedFirmwareName, selectedFirmwarePath)
    }

    Rectangle {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 30
        }
        width: toastText.implicitWidth + 40
        height: 40
        radius: 8
        color: toastOk ? "#22c55e" : "#ef4444"
        visible: showToast
        z: 200
        opacity: showToast ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }

        Text {
            id: toastText
            anchors.centerIn: parent
            text: toastMsg
            font.pixelSize: 14
            font.weight: Font.DemiBold
            font.family: root.uiFont
            color: "#ffffff"
        }
    }
}
