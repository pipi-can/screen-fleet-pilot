import QtQuick 2.15

// 资源卡片 — 服务器资源库中的单个资源
Rectangle {
    id: root
    width: 220; height: 200
    radius: 10; color: "#1f2937"
    border { width: selected ? 2 : 1; color: selected ? "#06b6d4" : "#334155" }

    property string resType: "image"    // image | video | text | web
    property string resName: ""
    property string resPath: ""
    property string resSize: ""
    property string resDate: ""
    property string resIcon: "🖼️"
    property bool selected: false

    readonly property string mediaUrl: {
        if (!resPath)
            return ""
        if (resPath.indexOf("http://") === 0 || resPath.indexOf("https://") === 0)
            return resPath
        return "http://8.136.113.168" + resPath
    }

    signal clicked()

    function loadTextPreview() {
        if (resType !== "text" || !mediaUrl) {
            textPreviewContent = ""
            return
        }
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                textPreviewContent = (xhr.status === 200) ? xhr.responseText : "无法加载"
            }
        }
        xhr.open("GET", mediaUrl)
        xhr.send()
    }

    property string textPreviewContent: ""

    onResPathChanged: loadTextPreview()
    onResTypeChanged: loadTextPreview()
    Component.onCompleted: loadTextPreview()

    // 缩略图
    Rectangle {
        id: graph
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 2 }
        height: 150
        color: "#0a0e1a"
        radius: parent.radius
        clip: true

        // 图片预览
        Image {
            id: thumbImage
            anchors.fill: parent
            visible: root.resType === "image"
            source: root.resType === "image" ? root.mediaUrl : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
        }

        // 文字预览
        Text {
            anchors.fill: parent
            anchors.margins: 8
            visible: root.resType === "text"
            text: root.textPreviewContent
            font.pixelSize: 10
            lineHeight: 1.25
            color: "#94a3b8"
            wrapMode: Text.WordWrap
            clip: true
            verticalAlignment: Text.AlignTop
        }

        // 占位图标（video / web / 加载中）
        Text {
            anchors.centerIn: parent
            text: root.resIcon
            font.pixelSize: 40
            opacity: 0.7
            visible: {
                if (root.resType === "video" || root.resType === "web")
                    return true
                if (root.resType === "image")
                    return thumbImage.status === Image.Error || thumbImage.status === Image.Null
                if (root.resType === "text")
                    return !root.textPreviewContent
                return false
            }
        }

        // 类型徽章
        Rectangle {
            anchors { left: parent.left; leftMargin: 8; top: parent.top; topMargin: 8 }
            width: badgeText.implicitWidth + 12; height: 18; radius: 4
            z: 2
            color: {
                switch (root.resType) {
                    case "image": return Qt.rgba(0.23,0.51,0.96,0.2);
                    case "video": return Qt.rgba(0.55,0.36,0.96,0.2);
                    case "text":  return Qt.rgba(0.92,0.7,0.03,0.2);
                    case "web":   return Qt.rgba(0.13,0.77,0.37,0.2);
                }
                return Qt.rgba(0.23,0.51,0.96,0.2);
            }
            Text {
                id: badgeText; anchors.centerIn: parent; text: root.resType
                font.pixelSize: 9; font.weight: Font.DemiBold
                color: {
                    switch (root.resType) {
                        case "image": return "#3b82f6"; case "video": return "#8b5cf6";
                        case "text":  return "#eab308"; case "web":   return "#22c55e";
                    }
                    return "#3b82f6";
                }
            }
        }

        // 选中角标
        Rectangle {
            anchors { right: parent.right; rightMargin: 8; top: parent.top; topMargin: 8 }
            width: 20; height: 20; radius: 10
            z: 2
            color: root.selected ? "#06b6d4" : "transparent"
            visible: root.selected
            Text { anchors.centerIn: parent; text: "✓"; font.pixelSize: 11; font.weight: Font.Bold; color: "#0f172a" }
        }
    }

    // 信息区
    Column {
        anchors { left: parent.left; leftMargin: 10; right: parent.right; rightMargin: 10; top: graph.bottom; topMargin: 10 }
        spacing: 4

        Text {
            width: parent.width; text: root.resName; font.pixelSize: 13; font.weight: Font.DemiBold
            color: "#e2e8f0"; elide: Text.ElideRight
        }

        Row {
            width: parent.width
            Text { text: root.resSize; font.pixelSize: 10; font.weight: Font.Medium; color: "#64748b" }
            Item { width: 8; height: 1 }
            Text { text: root.resDate; font.pixelSize: 10; color: "#64748b"; opacity: 0.7 }
        }
    }

    MouseArea {
        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
