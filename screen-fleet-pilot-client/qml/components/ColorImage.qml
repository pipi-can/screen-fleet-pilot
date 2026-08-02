import QtQuick
import Qt5Compat.GraphicalEffects

/*
 * @brief: 图标着色组件——将 source 指定的图片以 sourceColor 颜色渲染。
 *         使用 Qt5Compat 的 ColorOverlay 实现。
 */
Item {
    id: root

    property url source: ""
    property color sourceColor: "#ffffff"
    property int fillMode: Image.PreserveAspectFit

    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        fillMode: root.fillMode
        visible: false
    }

    ColorOverlay {
        anchors.fill: image
        source: image
        color: root.sourceColor
    }
}
