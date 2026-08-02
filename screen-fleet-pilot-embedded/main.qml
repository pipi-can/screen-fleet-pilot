import QtQuick 2.12
import QtQuick.Window 2.12

Window {
    id: root
    visible: true
    width: 800
    height: 480
    title: qsTr("Screen Fleet Pilot")
    color: "black"

    Image {
        id: imagePlayer
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: player.currentImage
        visible: player.currentImage !== ""

        Behavior on source {
            OpacityAnimator {
                target: imagePlayer
                from: 0
                to: 1
                duration: 500
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: qsTr("等待内容...")
        font.pixelSize: 28
        color: "#666666"
        visible: player.currentImage === ""
    }

    Rectangle {
        anchors {
            right: parent.right
            top: parent.top
            margins: 12
        }
        width: statusColumn.width + 16
        height: statusColumn.height + 16
        radius: 8
        color: "#CC000000"

        Column {
            id: statusColumn
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: player.statusInfo
                font.pixelSize: 12
                color: "#AAAAAA"
            }
            Text {
                text: player.imageCount > 0
                      ? (player.currentIndex + 1) + " / " + player.imageCount
                      : "0 / 0"
                font.pixelSize: 12
                color: "white"
            }
        }
    }
}
