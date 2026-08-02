import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import "../components"

// 页面 2：内容推送 — 服务器资源库 + 路径分发到嵌入式设备
Rectangle {
    id: root
    color: "#1a1a2e"

    // ═══════════════════════════════════════════════════════
    // 数据
    // ═══════════════════════════════════════════════════════
    ListModel {
        id: resourceModel
        dynamicRoles: true
    }

    property var selectedList: []    // 每次变更必须整体赋值

    property var filterList: [
        { key: "all",   label: "全部", count: 0 },
        { key: "image", label: "🖼 图片", count: 0 },
        { key: "video", label: "🎬 视频", count: 0 },
        { key: "text",  label: "📝 文字", count: 0 },
        { key: "web",   label: "🌐 网页", count: 0 }
    ]
    property string currentFilter: "all"

    property var activeGroups: []
    property string targetMode: "group"
    property var checkedDeviceList: []   // deviceUId 列表
    property var deviceModel: NetworkManager.deviceModel
    property var pushGroupList: []
    property var pushOnlineDeviceList: []

    property var pushResults: []
    property bool hasPushed: false
    property string resultTime: "--"

    property string playMode: "loop"   // loop | schedule
    property string scheduleDate: Qt.formatDateTime(new Date(), "yyyy-MM-dd")
    property string scheduleTime: "08:00"
    property int scheduleDurationSec: 60

    // ═══════════════════════════════════════════════════════
    // 操作函数
    // ═══════════════════════════════════════════════════════
    function isSelected(id) {
        return selectedList.indexOf(id) >= 0
    }

    function toggleSelect(id) {
        var arr = selectedList.slice()
        var idx = arr.indexOf(id)
        if (idx >= 0) { arr.splice(idx, 1) }
        else          { arr.push(id)     }
        selectedList = arr
    }

    function removeSelected(id) {
        var arr = selectedList.slice()
        var idx = arr.indexOf(id)
        if (idx >= 0) { arr.splice(idx, 1) }
        selectedList = arr
    }

    function refreshFilter() {
        var counts = { all: resourceModel.count, image: 0, video: 0, text: 0, web: 0 }
        for (var i = 0; i < resourceModel.count; i++) {
            var t = resourceModel.get(i).type
            if (counts[t] !== undefined) { counts[t]++ }
        }
        filterList = [
            { key: "all",   label: "全部",    count: counts.all   },
            { key: "image", label: "🖼 图片", count: counts.image },
            { key: "video", label: "🎬 视频", count: counts.video },
            { key: "text",  label: "📝 文字", count: counts.text  },
            { key: "web",   label: "🌐 网页", count: counts.web   }
        ]
    }

    function matchesFilter(type) {
        return currentFilter === "all" || type === currentFilter
    }

    function filteredCount() {
        if (currentFilter === "all")
            return resourceModel.count
        var n = 0
        for (var i = 0; i < resourceModel.count; i++) {
            if (resourceModel.get(i).type === currentFilter)
                n++
        }
        return n
    }

    function selectedResources() {
        var list = []
        for (var i = 0; i < resourceModel.count; i++) {
            var r = resourceModel.get(i)
            if (isSelected(r.id)) { list.push(r) }
        }
        return list
    }

    function targetCount() {
        if (!deviceModel)
            return 0
        if (targetMode === "all")
            return deviceModel.onlineDeviceCount()
        if (targetMode === "group") {
            var n = 0
            for (var i = 0; i < activeGroups.length; i++)
                n += deviceModel.onlineCountInGroup(activeGroups[i])
            return n
        }
        return checkedDeviceList.length
    }

    function refreshPushTargets() {
        if (!deviceModel)
            return

        var names = deviceModel.groupNames
        var groups = []
        for (var i = 0; i < names.length; i++) {
            groups.push({
                name: names[i],
                cnt: deviceModel.onlineCountInGroup(names[i])
            })
        }
        pushGroupList = groups
        pushOnlineDeviceList = deviceModel.onlineDeviceList()

        // 去掉已离线设备的勾选
        var onlineUids = []
        for (var j = 0; j < pushOnlineDeviceList.length; j++)
            onlineUids.push(pushOnlineDeviceList[j].deviceUId)
        var checked = checkedDeviceList.slice()
        for (var k = checked.length - 1; k >= 0; k--) {
            if (onlineUids.indexOf(checked[k]) < 0)
                checked.splice(k, 1)
        }
        checkedDeviceList = checked
    }

    function canPush() {
        return selectedList.length > 0 && targetCount() > 0
    }

    function toggleGroup(name) {
        var arr = activeGroups.slice()
        var idx = arr.indexOf(name)
        if (idx >= 0) { arr.splice(idx, 1) } else { arr.push(name) }
        activeGroups = arr
    }

    function toggleDevice(uid) {
        var arr = checkedDeviceList.slice()
        var idx = arr.indexOf(uid)
        if (idx >= 0) { arr.splice(idx, 1) } else { arr.push(uid) }
        checkedDeviceList = arr
    }

    function isDeviceChecked(uid) {
        return checkedDeviceList.indexOf(uid) >= 0
    }

    /** 按推送目标模式返回在线设备的 deviceUId 列表 */
    function selectedDeviceUids() {
        if (!deviceModel)
            return []

        if (targetMode === "device")
            return checkedDeviceList.slice()

        var onlineList = pushOnlineDeviceList.length > 0
                ? pushOnlineDeviceList
                : deviceModel.onlineDeviceList()

        if (targetMode === "all") {
            var all = []
            for (var i = 0; i < onlineList.length; i++)
                all.push(onlineList[i].deviceUId)
            return all
        }

        // group：选中分组内的在线设备
        var uids = []
        for (var j = 0; j < onlineList.length; j++) {
            if (activeGroups.indexOf(onlineList[j].deviceGroup) >= 0)
                uids.push(onlineList[j].deviceUId)
        }
        return uids
    }

    function schedulePushParams() {
        var duration = parseInt(scheduleDurationInput.text)
        if (isNaN(duration) || duration < 1)
            duration = scheduleDurationSec
        return {
            date: scheduleDateInput.text,
            time: scheduleTimeInput.text,
            durationSec: duration
        }
    }

    function doPush() {
        if (!canPush())
            return

        var deviceUids = selectedDeviceUids()
        var resourcePathList = []
        var resources = selectedResources()
        for (var i = 0; i < resources.length; i++)
            resourcePathList.push(resources[i].path)

        if (playMode === "loop") {
            NetworkManager.requestPushContentsToEmbedded(deviceUids, resourcePathList)
        } else if (playMode === "schedule") {
            var schedule = schedulePushParams()
            scheduleDate = schedule.date
            scheduleTime = schedule.time
            scheduleDurationSec = schedule.durationSec
            console.log("[push] schedule push", schedule.date, schedule.time, schedule.durationSec)
            NetworkManager.requestSchedulePushToEmbedded(
                        deviceUids,
                        resourcePathList,
                        schedule.date,
                        schedule.time,
                        schedule.durationSec)
        }
    }

    function storageMB() {
        var total = 0.0
        for (var i = 0; i < resourceModel.count; i++) {
            var s = resourceModel.get(i).size
            var val = parseFloat(s)
            if (s.indexOf("KB") >= 0)      { total += val / 1000.0   }
            else if (s.indexOf("GB") >= 0) { total += val * 1000.0   }
            else                           { total += val            }
        }
        return total
    }

    function guessType(name) {
        var ext = name.split(".").pop().toLowerCase()
        if (["jpg", "jpeg", "png", "gif"].indexOf(ext) >= 0) return "image"
        if (ext === "mp4") return "video"
        if (ext === "txt") return "text"
        if (["html", "htm"].indexOf(ext) >= 0) return "web"
        return "image"
    }

    function guessIcon(name) {
        var t = guessType(name)
        if (t === "image") return "🖼️"
        if (t === "video") return "🎬"
        if (t === "text")  return "📝"
        if (t === "web")   return "🌐"
        return "📄"
    }

    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1024 / 1024).toFixed(1) + " MB"
    }

    function loadResourceList(files) {
        resourceModel.clear()
        selectedList = []
        for (var i = 0; i < files.length; i++) {
            var f = files[i]
            var name = f.name || ""
            // 有原片时隐藏 15s 预览副本，避免列表重复
            if (/_15s\.mp4$/i.test(name)) {
                var originalName = name.replace(/_15s\.mp4$/i, ".mp4")
                var hasOriginal = false
                for (var j = 0; j < files.length; j++) {
                    if ((files[j].name || "") === originalName) {
                        hasOriginal = true
                        break
                    }
                }
                if (hasOriginal)
                    continue
            }
            resourceModel.append({
                id: i + 1,
                type: guessType(name),
                name: name,
                path: f.path || "",
                size: formatSize(f.size || 0),
                date: "",
                icon: guessIcon(name)
            })
        }
        refreshFilter()
    }

    // ═══════════════════════════════════════════════════════
    // 初始示例数据（展示滚动效果）
    // ═══════════════════════════════════════════════════════
    // Component.onCompleted: {
    //     var samples = [
    //         { type:"image", name:"促销海报_v2.jpg",      path:"/static/uploads/promo_v2.jpg",      size:"3.2 MB", date:"2026-06-15", icon:"🖼️" },
    //         { type:"video", name:"午市菜单.mp4",           path:"/static/uploads/lunch_menu.mp4",    size:"18.5 MB",date:"2026-06-18", icon:"🎬" },
    //         { type:"image", name:"新品上市.png",           path:"/static/uploads/new_arrival.png",   size:"1.1 MB", date:"2026-06-20", icon:"🖼️" },
    //         { type:"text",  name:"欢迎词滚动.txt",         path:"/static/uploads/welcome_scroll.txt",size:"0.5 KB", date:"2026-06-21", icon:"📝" },
    //         { type:"web",   name:"限时活动页.html",        path:"/static/uploads/promo_page.html",   size:"4.8 KB", date:"2026-06-22", icon:"🌐" },
    //         { type:"image", name:"品牌LOGO_高清.png",      path:"/static/uploads/logo_hd.png",       size:"5.6 MB", date:"2026-06-10", icon:"🖼️" },
    //         { type:"video", name:"门店宣传片.mp4",         path:"/static/uploads/store_ad.mp4",      size:"22.1 MB",date:"2026-06-12", icon:"🎬" },
    //         { type:"image", name:"端午活动_banner.jpg",    path:"/static/uploads/dragon_boat.jpg",   size:"2.8 MB", date:"2026-06-19", icon:"🖼️" },
    //         { type:"text",  name:"安全提醒_多语言.txt",    path:"/static/uploads/safety_notice.txt", size:"1.2 KB", date:"2026-06-16", icon:"📝" },
    //         { type:"image", name:"电梯广告_竖版.jpg",      path:"/static/uploads/elevator_ad.jpg",   size:"1.8 MB", date:"2026-06-17", icon:"🖼️" },
    //         { type:"video", name:"周末特卖_15s.mp4",       path:"/static/uploads/weekend_sale.mp4",  size:"8.3 MB", date:"2026-06-14", icon:"🎬" },
    //         { type:"web",   name:"会员权益页.html",        path:"/static/uploads/vip_benefits.html", size:"6.1 KB", date:"2026-06-22", icon:"🌐" },
    //         { type:"image", name:"停车场指引.jpg",         path:"/static/uploads/parking_guide.jpg", size:"0.9 MB", date:"2026-06-08", icon:"🖼️" },
    //     ]
    //     var maxId = 0
    //     for (var i = 0; i < samples.length; i++) {
    //         maxId++
    //         samples[i].id = maxId
    //         resourceModel.append(samples[i])
    //     }
    //     refreshFilter()
    // }

    // ═══════════════════════════════════════════════════════
    // 布局：左面板（资源库） + 右面板（推送配置）
    // ═══════════════════════════════════════════════════════
    Item {
        anchors.fill: parent

        // ──────────────────────────────────────────────
        // 左面板：服务器资源库
        // ──────────────────────────────────────────────
        Item {
            id: leftPanel
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                right: rightPanel.left
            }

            // ---- 标题 ----
            Row {
                id: leftTitle
                anchors {
                    left: parent.left
                    leftMargin: 20
                    top: parent.top
                    topMargin: 14
                }
                spacing: 8

                Text {
                    text: "📦"
                    font.pixelSize: 16
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "服务器资源库"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: "#e2e8f0"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: rcText.implicitWidth + 16
                    height: 20
                    radius: 10
                    color: "#1f2937"
                    Text {
                        id: rcText
                        anchors.centerIn: parent
                        text: "共 " + resourceModel.count + " 个资源"
                        font.pixelSize: 11
                        color: "#64748b"
                    }
                }
            }

            // ---- 上传区 ----
            Rectangle {
                id: uploadZone
                anchors {
                    left: parent.left
                    leftMargin: 20
                    right: parent.right
                    rightMargin: 20
                    top: leftTitle.bottom
                    topMargin: 12
                }
                height: 160
                radius: 10
                color: uploadMA.containsMouse ? "#243447" : "#1f2937"
                border {
                    width: 2
                    color: uploadMA.containsMouse ? "#3b82f6" : "#334155"
                }

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                MouseArea {
                    id: uploadMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: uploadDialog.open()
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "☁️"
                        font.pixelSize: 32
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "拖拽或选择文件上传至服务器"
                        font.pixelSize: 13
                        color: "#94a3b8"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "支持 JPG / PNG / MP4 / HTML · 单文件最大 50MB"
                        font.pixelSize: 11
                        color: "#64748b"
                    }
                }
            }

            // ---- 文件选择对话框 ----
            FileDialog {
                id: uploadDialog
                title: "选择上传文件"
                fileMode: FileDialog.OpenFiles
                nameFilters: [
                    "图片文件 (*.jpg *.jpeg *.png)",
                    "视频文件 (*.mp4)",
                    "网页文件 (*.html *.htm)",
                    "所有支持的文件 (*.jpg *.jpeg *.png *.mp4 *.html *.htm)"
                ]
                onAccepted: {
                    var paths = []
                    for (var i = 0; i < selectedFiles.length; i++) {
                        console.log("Selected:", selectedFiles[i])
                        paths.push(selectedFiles[i])
                    }
                    FileUploader.addTasks(paths)
                }
            }

            // ---- 筛选标签 ----
            Row {
                id: filterRow
                anchors {
                    left: parent.left
                    leftMargin: 20
                    top: uploadZone.bottom
                    topMargin: 10
                }
                spacing: 4

                Repeater {
                    model: filterList

                    delegate: Rectangle {
                        width: flText.implicitWidth + 24
                        height: 28
                        radius: 6
                        color: {
                            if (currentFilter === modelData.key)
                                return Qt.rgba(0.024, 0.714, 0.831, 0.1)
                            if (flMa.containsMouse)
                                return "#1f2937"
                            return "transparent"
                        }
                        border {
                            width: currentFilter === modelData.key ? 1 : 0
                            color: Qt.rgba(0.024, 0.714, 0.831, 0.3)
                        }

                        Text {
                            id: flText
                            anchors.centerIn: parent
                            text: modelData.label + " " + modelData.count
                            font.pixelSize: 12
                            color: currentFilter === modelData.key ? "#06b6d4" : "#64748b"
                        }

                        MouseArea {
                            id: flMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { currentFilter = modelData.key }
                        }
                    }
                }
            }

            // ---- 可滚动：卡片网格 + 存储用量 ----
            Flickable {
                id: leftFlick
                anchors {
                    left: parent.left
                    leftMargin: 20
                    right: parent.right
                    rightMargin: 20
                    top: filterRow.bottom
                    topMargin: 10
                    bottom: parent.bottom
                    bottomMargin: 12
                }
                contentWidth: width
                contentHeight: gridSection.height + 12
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                boundsMovement: Flickable.StopAtBounds

                // 滚动条
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    width: parent.width
                    spacing: 0

                    // 空状态 / 卡片网格
                    Item {
                        id: gridSection
                        width: parent.width
                        height: resourceModel.count === 0 ? 220
                              : (filteredCount() === 0 ? 220 : cardFlow.implicitHeight)

                        // 空状态：资源库为空
                        Item {
                            anchors.centerIn: parent
                            visible: resourceModel.count === 0
                            width: 300
                            height: 200

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "📭"
                                    font.pixelSize: 56
                                    opacity: 0.5
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "服务器资源库为空"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    color: "#94a3b8"
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "请上传图片、视频、文字或网页资源到服务器"
                                    font.pixelSize: 13
                                    color: "#64748b"
                                }
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: esTag.implicitWidth + 32
                                    height: 28
                                    radius: 14
                                    color: "#1f2937"
                                    border { width: 1; color: "#334155" }
                                    Text {
                                        id: esTag
                                        anchors.centerIn: parent
                                        text: "支持 JPG · PNG · MP4 · GIF · HTML · TXT"
                                        font.pixelSize: 11
                                        color: "#64748b"
                                    }
                                }
                            }
                        }

                        // 空状态：当前分类无资源
                        Item {
                            anchors.centerIn: parent
                            visible: resourceModel.count > 0 && filteredCount() === 0
                            width: 300
                            height: 160

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "📂"
                                    font.pixelSize: 48
                                    opacity: 0.5
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "该分类下暂无资源"
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: "#94a3b8"
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "切换到其他分类查看，或上传新资源"
                                    font.pixelSize: 12
                                    color: "#64748b"
                                }
                            }
                        }

                        // 卡片网格
                        Flow {
                            id: cardFlow
                            anchors {
                                left: parent.left
                                right: parent.right
                            }
                            spacing: 12
                            visible: filteredCount() > 0

                            Repeater {
                                model: resourceModel
                                delegate: ResourceCard {
                                    width: (cardFlow.width - 12 * 3) / 4
                                    visible: root.matchesFilter(model.type)
                                    resType: model.type
                                    resName: model.name
                                    resPath: model.path
                                    resSize: model.size
                                    resDate: model.date
                                    resIcon: model.icon
                                    selected: isSelected(model.id)
                                    onClicked: root.toggleSelect(model.id)
                                }
                            }
                        }
                    }

                    Item { width: 1; height: 12 }

                    // // 存储用量
                    // Rectangle {
                    //     id: storageSection
                    //     width: parent.width
                    //     height: 32
                    //     color: "transparent"

                    //     Column {
                    //         anchors.fill: parent
                    //         spacing: 4

                    //         Item {
                    //             width: parent.width
                    //             height: 14

                    //             Text {
                    //                 anchors.left: parent.left
                    //                 text: "服务器存储用量"
                    //                 font.pixelSize: 11
                    //                 color: "#64748b"
                    //             }
                    //             Text {
                    //                 anchors.right: parent.right
                    //                 text: storageMB().toFixed(1) + "MB / 500MB"
                    //                 font.pixelSize: 11
                    //                 color: "#64748b"
                    //             }
                    //         }

                    //         Rectangle {
                    //             width: parent.width
                    //             height: 4
                    //             radius: 2
                    //             color: "#111827"

                    //             Rectangle {
                    //                 anchors {
                    //                     left: parent.left
                    //                     top: parent.top
                    //                     bottom: parent.bottom
                    //                 }
                    //                 width: parent.width * Math.min(storageMB() / 500.0, 1.0)
                    //                 radius: 2
                    //                 gradient: Gradient {
                    //                     GradientStop { position: 0.0; color: "#06b6d4" }
                    //                     GradientStop { position: 1.0; color: "#3b82f6" }
                    //                 }
                    //             }
                    //         }
                    //     }
                    // }
                }
            }
        }

        // ──────────────────────────────────────────────
        // 右面板：推送配置
        // ──────────────────────────────────────────────
        Rectangle {
            id: rightPanel
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            width: 300
            color: "#16213e"

            // 左分隔线
            Rectangle {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 1
                color: "#334155"
            }

            Flickable {
                id: rightFlick
                anchors.fill: parent
                contentWidth: width
                contentHeight: pushColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                boundsMovement: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    id: pushColumn
                    width: parent.width
                    spacing: 0

                    // ---- 推送队列标题 ----
                    Rectangle {
                        width: parent.width
                        implicitHeight: 48
                        color: "transparent"

                        Row {
                            anchors {
                                left: parent.left
                                leftMargin: 24
                                verticalCenter: parent.verticalCenter
                            }
                            spacing: 8

                            Text {
                                text: "📋 推送队列"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: "#64748b"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: badgeText.implicitWidth + 10
                                height: 19
                                radius: 9
                                color: "#06b6d4"

                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: selectedList.length
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    color: "#0f172a"
                                }
                            }
                        }
                    }

                    // ---- 已选资源 ----
                    Rectangle {
                        width: parent.width - 48
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: selectedList.length > 0 ? selectedColumn.implicitHeight + 16 : 64
                        radius: 10
                        color: selectedList.length > 0 ? "transparent" : "#1f2937"
                        border {
                            width: selectedList.length > 0 ? 0 : 1
                            color: "#334155"
                        }

                        // 空提示
                        Text {
                            anchors.centerIn: parent
                            text: "👆 点击左侧资源卡片选择要推送的内容"
                            font.pixelSize: 13
                            color: "#64748b"
                            visible: selectedList.length === 0
                        }

                        // 已选列表
                        Column {
                            id: selectedColumn
                            anchors {
                                left: parent.left
                                right: parent.right
                                top: parent.top
                                topMargin: 8
                            }
                            spacing: 6
                            visible: selectedList.length > 0

                            Repeater {
                                model: selectedResources()

                                delegate: Rectangle {
                                    width: selectedColumn.width
                                    implicitHeight: 40
                                    radius: 8
                                    color: "#1f2937"
                                    border {
                                        width: 1
                                        color: "#334155"
                                    }

                                    Row {
                                        anchors {
                                            fill: parent
                                            leftMargin: 10
                                            rightMargin: 10
                                        }
                                        spacing: 8

                                        Text {
                                            text: modelData.icon
                                            font.pixelSize: 16
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - 70

                                            Text {
                                                text: modelData.name
                                                font.pixelSize: 12
                                                font.weight: Font.DemiBold
                                                color: "#e2e8f0"
                                                elide: Text.ElideRight
                                                width: parent.width
                                            }

                                            Text {
                                                text: modelData.path
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 10
                                                }
                                                color: "#64748b"
                                                elide: Text.ElideRight
                                                width: parent.width
                                            }
                                        }

                                        Item { width: 4; height: 1 }

                                        Rectangle {
                                            width: 24
                                            height: 24
                                            radius: 12
                                            color: rmMa.containsMouse
                                                ? Qt.rgba(0.94, 0.27, 0.27, 0.15)
                                                : "transparent"
                                            anchors.verticalCenter: parent.verticalCenter

                                            Text {
                                                anchors.centerIn: parent
                                                text: "✕"
                                                font.pixelSize: 13
                                                color: rmMa.containsMouse ? "#ef4444" : "#64748b"
                                            }

                                            MouseArea {
                                                id: rmMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.removeSelected(modelData.id)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item { width: 1; implicitHeight: 14 }

                    // ---- 推送目标 ----
                    Rectangle {
                        width: parent.width
                        implicitHeight: 40
                        color: "transparent"

                        Text {
                            anchors {
                                left: parent.left
                                leftMargin: 24
                                verticalCenter: parent.verticalCenter
                            }
                            text: "🎯 推送目标"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: "#64748b"
                        }
                    }

                    // 模式切换
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 8

                        property var modeOpts: [
                            { k: "group",  l: "按分组"   },
                            { k: "device", l: "指定设备" },
                            { k: "all",    l: "全部在线" }
                        ]

                        Repeater {
                            model: parent.modeOpts

                            delegate: Rectangle {
                                width: 90
                                implicitHeight: 32
                                radius: 7
                                color: targetMode === modelData.k
                                    ? Qt.rgba(0.024, 0.714, 0.831, 0.1)
                                    : "transparent"
                                border {
                                    width: 1
                                    color: targetMode === modelData.k ? "#06b6d4" : "#334155"
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.l
                                    font.pixelSize: 12
                                    color: targetMode === modelData.k ? "#06b6d4" : "#94a3b8"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { targetMode = modelData.k }
                                }
                            }
                        }
                    }

                    // 分组 chip
                    Flow {
                        id: groupTargetFlow
                        anchors {
                            left: parent.left
                            leftMargin: 24
                            right: parent.right
                            rightMargin: 24
                        }
                        spacing: 8
                        topPadding: 10
                        visible: targetMode === "group"

                        Repeater {
                            model: pushGroupList

                            delegate: Rectangle {
                                width: gcTxt.implicitWidth + 22
                                implicitHeight: 30
                                radius: 15
                                color: activeGroups.indexOf(modelData.name) >= 0
                                    ? Qt.rgba(0.024, 0.714, 0.831, 0.12)
                                    : "transparent"
                                border {
                                    width: 1
                                    color: activeGroups.indexOf(modelData.name) >= 0
                                        ? "#06b6d4"
                                        : "#334155"
                                }

                                Text {
                                    id: gcTxt
                                    anchors.centerIn: parent
                                    text: modelData.name + "  " + modelData.cnt + "台在线"
                                    font.pixelSize: 12
                                    color: activeGroups.indexOf(modelData.name) >= 0
                                        ? "#06b6d4"
                                        : "#94a3b8"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.toggleGroup(modelData.name)
                                }
                            }
                        }

                        Text {
                            visible: pushGroupList.length === 0
                            text: "暂无分组，请先在仪表盘添加设备"
                            font.pixelSize: 12
                            color: "#64748b"
                        }
                    }

                    // 指定设备（仅在线可选）
                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: 24
                            right: parent.right
                            rightMargin: 24
                        }
                        spacing: 8
                        topPadding: 10
                        visible: targetMode === "device"

                        Text {
                            text: "勾选要推送的在线设备（离线设备不可选）"
                            font.pixelSize: 12
                            color: "#64748b"
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            Repeater {
                                model: pushOnlineDeviceList

                                delegate: Rectangle {
                                    width: devChipTxt.implicitWidth + 22
                                    implicitHeight: 30
                                    radius: 15
                                    color: root.isDeviceChecked(modelData.deviceUId)
                                        ? Qt.rgba(0.024, 0.714, 0.831, 0.12)
                                        : "transparent"
                                    border {
                                        width: 1
                                        color: root.isDeviceChecked(modelData.deviceUId)
                                            ? "#06b6d4"
                                            : "#334155"
                                    }

                                    Text {
                                        id: devChipTxt
                                        anchors.centerIn: parent
                                        text: modelData.deviceName + " · " + modelData.deviceGroup
                                        font.pixelSize: 12
                                        color: root.isDeviceChecked(modelData.deviceUId)
                                            ? "#06b6d4"
                                            : "#94a3b8"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.toggleDevice(modelData.deviceUId)
                                    }
                                }
                            }
                        }

                        Text {
                            visible: pushOnlineDeviceList.length === 0
                            text: "暂无在线设备"
                            font.pixelSize: 12
                            color: "#64748b"
                        }
                    }

                    // 全部在线
                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: 24
                            right: parent.right
                            rightMargin: 24
                        }
                        spacing: 8
                        topPadding: 10
                        visible: targetMode === "all"

                        Text {
                            text: "将全部 " + (deviceModel ? deviceModel.onlineDeviceCount() : 0) + " 台在线设备作为推送目标"
                            font.pixelSize: 12
                            color: "#06b6d4"
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            Repeater {
                                model: pushOnlineDeviceList

                                delegate: Rectangle {
                                    width: allChipTxt.implicitWidth + 22
                                    implicitHeight: 30
                                    radius: 15
                                    color: Qt.rgba(0.024, 0.714, 0.831, 0.08)
                                    border { width: 1; color: "#334155" }

                                    Text {
                                        id: allChipTxt
                                        anchors.centerIn: parent
                                        text: modelData.deviceName + " · " + modelData.deviceGroup
                                        font.pixelSize: 12
                                        color: "#94a3b8"
                                    }
                                }
                            }
                        }

                        Text {
                            visible: pushOnlineDeviceList.length === 0
                            text: "暂无在线设备"
                            font.pixelSize: 12
                            color: "#64748b"
                        }
                    }

                    // 目标汇总
                    Text {
                        anchors {
                            left: parent.left
                            leftMargin: 24
                        }
                        text: "已选 " + targetCount() + " 台在线设备"
                        font.pixelSize: 12
                        color: "#64748b"
                        topPadding: 8
                    }

                    Item { width: 1; implicitHeight: 14 }

                    // ---- 播放设置 ----
                    Rectangle {
                        width: parent.width
                        implicitHeight: 38
                        color: "transparent"

                        Text {
                            anchors {
                                left: parent.left
                                leftMargin: 24
                                verticalCenter: parent.verticalCenter
                            }
                            text: "⚙ 播放设置"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: "#64748b"
                        }
                    }

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: 24
                            right: parent.right
                            rightMargin: 24
                        }
                        spacing: 8

                        Row {
                            width: parent.width
                            spacing: 8

                            Text {
                                text: "播放模式"
                                font.pixelSize: 13
                                color: "#94a3b8"
                                width: 80
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            ComboBox {
                                id: playModeCombo
                                width: 150
                                leftPadding: 10
                                rightPadding: 10
                                model: ["循环播放", "定时推送"]
                                currentIndex: playMode === "loop" ? 0 : 1
                                onActivated: function(index) {
                                    playMode = index === 0 ? "loop" : "schedule"
                                }

                                background: Rectangle {
                                    implicitWidth: playModeCombo.width
                                    implicitHeight: 32
                                    radius: 7
                                    color: "#111827"
                                    border { width: 1; color: playModeCombo.activeFocus ? "#06b6d4" : "#334155" }
                                }

                                contentItem: Text {
                                    leftPadding: playModeCombo.leftPadding
                                    rightPadding: playModeCombo.indicator.width + playModeCombo.spacing + playModeCombo.rightPadding
                                    text: playModeCombo.displayText
                                    font.pixelSize: 13
                                    color: "#e2e8f0"
                                    verticalAlignment: Text.AlignVCenter
                                }

                                delegate: ItemDelegate {
                                    width: playModeCombo.width
                                    height: 32
                                    text: modelData
                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: 13
                                        color: parent.highlighted ? "#06b6d4" : "#e2e8f0"
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    background: Rectangle {
                                        color: parent.highlighted ? "#243447" : "transparent"
                                    }
                                }

                                popup: Popup {
                                    y: playModeCombo.height + 2
                                    width: playModeCombo.width
                                    implicitHeight: Math.min(contentItem.implicitHeight, 120)
                                    padding: 4

                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: playModeCombo.popup.visible ? playModeCombo.delegateModel : null
                                        currentIndex: playModeCombo.highlightedIndex
                                        ScrollIndicator.vertical: ScrollIndicator { }
                                    }

                                    background: Rectangle {
                                        color: "#1f2937"
                                        radius: 7
                                        border { width: 1; color: "#334155" }
                                    }
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8
                            visible: playMode === "loop"

                            Text {
                                text: "切换间隔"
                                font.pixelSize: 13
                                color: "#94a3b8"
                                width: 80
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: 70
                                implicitHeight: 32
                                radius: 7
                                color: "#111827"
                                border { width: 1; color: "#334155" }

                                TextInput {
                                    anchors {
                                        fill: parent
                                        margins: 8
                                    }
                                    text: "30"
                                    font.pixelSize: 13
                                    color: "#e2e8f0"
                                    validator: IntValidator { bottom: 1; top: 3600 }
                                }
                            }

                            Text {
                                text: "秒"
                                font.pixelSize: 13
                                color: "#64748b"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8
                            visible: playMode === "schedule"

                            Text {
                                text: "推送时间"
                                font.pixelSize: 13
                                color: "#94a3b8"
                                width: 80
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: 108
                                implicitHeight: 32
                                radius: 7
                                color: "#111827"
                                border { width: 1; color: "#334155" }

                                TextInput {
                                    id: scheduleDateInput
                                    anchors { fill: parent; margins: 8 }
                                    text: scheduleDate
                                    font.pixelSize: 13
                                    color: "#e2e8f0"
                                    selectByMouse: true
                                }
                            }

                            Rectangle {
                                width: 64
                                implicitHeight: 32
                                radius: 7
                                color: "#111827"
                                border { width: 1; color: "#334155" }

                                TextInput {
                                    id: scheduleTimeInput
                                    anchors { fill: parent; margins: 8 }
                                    text: scheduleTime
                                    font.pixelSize: 13
                                    color: "#e2e8f0"
                                    selectByMouse: true
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8
                            visible: playMode === "schedule"

                            Text {
                                text: "持续时间"
                                font.pixelSize: 13
                                color: "#94a3b8"
                                width: 80
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: 70
                                implicitHeight: 32
                                radius: 7
                                color: "#111827"
                                border { width: 1; color: "#334155" }

                                TextInput {
                                    id: scheduleDurationInput
                                    anchors { fill: parent; margins: 8 }
                                    text: scheduleDurationSec.toString()
                                    font.pixelSize: 13
                                    color: "#e2e8f0"
                                    selectByMouse: true
                                    validator: IntValidator { bottom: 1; top: 86400 }
                                }
                            }

                            Text {
                                text: "秒"
                                font.pixelSize: 13
                                color: "#64748b"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Item { width: 1; implicitHeight: 18 }

                    // ---- 推送按钮 ----
                    Rectangle {
                        anchors {
                            left: parent.left
                            leftMargin: 24
                            right: parent.right
                            rightMargin: 24
                        }
                        implicitHeight: 50
                        radius: 12
                        color: canPush() ? "#06b6d4" : "#334155"
                        opacity: canPush() ? 1.0 : 0.5

                        Behavior on color { ColorAnimation { duration: 180 } }
                        Behavior on opacity { NumberAnimation { duration: 180 } }

                        Text {
                            anchors.centerIn: parent
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: "#ffffff"
                            text: canPush()
                                ? "📤 推送 " + selectedList.length + " 个资源到 " + targetCount() + " 台设备 →"
                                : "📤 选择资源和目标设备后推送 →"
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: canPush()
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: doPush()
                        }
                    }

                    Item { width: 1; implicitHeight: 16 }

                    // ---- 推送记录 ----
                    Rectangle {
                        anchors {
                            left: parent.left
                            leftMargin: 24
                            right: parent.right
                            rightMargin: 24
                        }
                        implicitHeight: hasPushed ? Math.max(200, logContent.implicitHeight + 40) : 100
                        radius: 10
                        color: "#111827"
                        border { width: 1; color: "#334155" }

                        Column {
                            anchors {
                                fill: parent
                                margins: 12
                            }
                            spacing: 4

                            // 标题行
                            Item {
                                width: parent.width
                                implicitHeight: 18

                                Text {
                                    anchors.left: parent.left
                                    text: "📋 推送记录"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: "#64748b"
                                }

                                Text {
                                    anchors.right: parent.right
                                    text: resultTime
                                    font {
                                        family: "Consolas, Courier New, monospace"
                                        pixelSize: 11
                                    }
                                    color: "#64748b"
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: "#334155"
                            }

                            // 日志内容
                            Column {
                                id: logContent
                                width: parent.width
                                spacing: 2
                                visible: hasPushed

                                Repeater {
                                    model: pushResults

                                    delegate: Item {
                                        width: logContent.width
                                        implicitHeight: modelData.t === "sep" ? 6 : 20

                                        // 分隔线
                                        Rectangle {
                                            anchors {
                                                left: parent.left
                                                right: parent.right
                                                verticalCenter: parent.verticalCenter
                                            }
                                            height: 1
                                            color: "#334155"
                                            visible: modelData.t === "sep"
                                        }

                                        // info / path 行
                                        Text {
                                            anchors.fill: parent
                                            text: modelData.txt || ""
                                            font {
                                                family: "Consolas, Courier New, monospace"
                                                pixelSize: 11
                                            }
                                            color: modelData.t === "hdr" ? "#06b6d4" : "#64748b"
                                            verticalAlignment: Text.AlignVCenter
                                            visible: modelData.t === "hdr" || modelData.t === "path"
                                        }

                                        // 成功/失败行
                                        Row {
                                            anchors.fill: parent
                                            spacing: 8
                                            visible: modelData.t === "ok" || modelData.t === "fail"

                                            Text {
                                                text: modelData.t === "ok" ? "✓" : "✗"
                                                width: 14
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: modelData.t === "ok" ? "#22c55e" : "#ef4444"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Text {
                                                text: modelData.dev || ""
                                                width: 70
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: "#94a3b8"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Text {
                                                text: modelData.msg || ""
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: "#64748b"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Item { width: 24; height: 1 }

                                            Text {
                                                text: modelData.time || ""
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: "#64748b"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        // 汇总行
                                        Row {
                                            anchors.fill: parent
                                            spacing: 14
                                            visible: modelData.t === "sum"

                                            Text {
                                                text: "✓ " + (modelData.ok || 0) + " 成功"
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: "#22c55e"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Text {
                                                text: "✗ " + (modelData.no || 0) + " 失败"
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: modelData.no > 0 ? "#ef4444" : "#64748b"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Text {
                                                text: "总耗时 " + (modelData.tt || "0") + "s"
                                                font {
                                                    family: "Consolas, Courier New, monospace"
                                                    pixelSize: 11
                                                }
                                                color: "#64748b"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }
                                    }
                                }
                            }

                            // 空日志
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                topPadding: 24
                                text: "尚未推送 · 选择资源后点击推送按钮"
                                font.pixelSize: 12
                                color: "#64748b"
                                visible: !hasPushed
                            }
                        }
                    }

                    // 底部留白
                    Item { width: 1; implicitHeight: 16 }
                }
            }
        }
    }

    Connections {
        target: deviceModel
        function onCountsChanged() { refreshPushTargets() }
        function onGroupsChanged() { refreshPushTargets() }
    }

    Connections {
        target: FileUploader
        function onTaskFailed(taskName, message) {
            console.log("[client]: upload failed", taskName, message)
        }

        function onTaskStarted(taskName, taskSize) {
            console.log("[client]: upload started", taskName, taskSize)
        }

        function onTaskProgress(taskName, sent, total) {
            console.log("[client]: upload progress", taskName, sent, total)
        }

        function onTaskFinished(taskName, taskServerPath, taskServerUrl) {
            console.log("[client]: upload finished", taskName, taskServerPath, taskServerUrl)
            NetworkManager.requestServerFileList()
        }
    }

    Connections {
        target: NetworkManager

        function onRegistered() {
            NetworkManager.requestServerFileList()
        }

        function onServerFileListReceived(files) {
            loadResourceList(files)
        }
    }

    Component.onCompleted: {
        refreshPushTargets()
        NetworkManager.requestServerFileList()
    }
}
