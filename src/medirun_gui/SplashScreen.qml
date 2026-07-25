import QtQuick 6.5

Rectangle {
    id: root
    width: 900
    height: 900
    color: "#000000"
    clip: true

    signal exitAnimationFinished()

    property bool exitStarted: false
    property bool particleEnabled: true
    property bool introComplete: false

    function startExitAnimation() {
        if (exitStarted) {
            return
        }
        exitStarted = true
        particleEnabled = false
        outroAnimation.start()
    }

    Component.onCompleted: {
        console.log("SplashScreen loaded")
        introFadeIn.start()
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Item {
        id: scene
        anchors.fill: parent
        opacity: 1.0
        scale: 1.04

        Image {
            id: splashImage
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            source: "qrc:/assets/aurix_splash.png"
            sourceSize.width: 1800
            sourceSize.height: 1800
            smooth: true
            mipmap: true
            opacity: 0.93

            onStatusChanged: {
                if (status === Image.Ready) {
                    console.log("Splash image loaded:", source)
                } else if (status === Image.Error) {
                    console.log("Splash image failed to load:", source)
                }
            }
        }

        Rectangle {
            id: breatheGlow
            anchors.fill: parent
            color: "#00000000"
            opacity: 0.10

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00FFF1B0" }
                GradientStop { position: 0.45; color: "#18FFD76A" }
                GradientStop { position: 0.72; color: "#2EFFB300" }
                GradientStop { position: 1.0; color: "#00FFB300" }
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.10
        }

        SequentialAnimation {
            id: floatAnimation
            loops: Animation.Infinite
            running: root.introComplete && !root.exitStarted
            NumberAnimation { target: splashImage; property: "y"; from: 2; to: -3; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { target: splashImage; property: "y"; from: -3; to: 2; duration: 900; easing.type: Easing.InOutSine }
        }

        SequentialAnimation {
            id: imageScaleAnimation
            loops: Animation.Infinite
            running: root.introComplete && !root.exitStarted
            NumberAnimation { target: splashImage; property: "scale"; from: 1.0; to: 1.013; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { target: splashImage; property: "scale"; from: 1.013; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
        }

        SequentialAnimation {
            id: breatheGlowAnimation
            loops: Animation.Infinite
            running: root.introComplete && !root.exitStarted
            NumberAnimation { target: breatheGlow; property: "opacity"; from: 0.08; to: 0.19; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { target: breatheGlow; property: "opacity"; from: 0.19; to: 0.08; duration: 900; easing.type: Easing.InOutSine }
        }

        Canvas {
            id: ringCanvas
            anchors.fill: parent
            opacity: 0.78

            property real rotationA: 0
            property real rotationB: 0
            property real rotationC: 0
            property real pulse: 0
            property real fadeFactor: 1.0

            onRotationAChanged: requestPaint()
            onRotationBChanged: requestPaint()
            onRotationCChanged: requestPaint()
            onPulseChanged: requestPaint()
            onFadeFactorChanged: requestPaint()

            function strokeEllipse(ctx, cx, cy, rx, ry, lineWidth, color, alphaValue, rotationDeg, startDeg, endDeg) {
                ctx.save()
                ctx.translate(cx, cy)
                ctx.rotate(rotationDeg * Math.PI / 180)
                ctx.beginPath()
                ctx.ellipse(0, 0, rx, ry, 0, startDeg * Math.PI / 180, endDeg * Math.PI / 180)
                ctx.strokeStyle = color
                ctx.globalAlpha = alphaValue * fadeFactor
                ctx.lineWidth = lineWidth
                ctx.shadowBlur = 18
                ctx.shadowColor = color
                ctx.stroke()
                ctx.restore()
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                const cx = width * 0.5
                const cy = height * 0.81
                const wave = 1.0 + pulse * 0.12

                strokeEllipse(ctx, cx, cy, 138 * wave, 24 * wave, 2.0, "#FFD76A", 0.34, rotationA, 0, 360)
                strokeEllipse(ctx, cx, cy, 192 * (1.0 + pulse * 0.08), 34, 2.6, "#FFB300", 0.22, -rotationB, 24, 308)
                strokeEllipse(ctx, cx, cy, 248 * (1.0 + pulse * 0.04), 48, 1.7, "#FFF1B0", 0.18, rotationC, 118, 308)
                strokeEllipse(ctx, cx, cy, 286 * wave, 58 * wave, 1.4, "#FFB300", 0.14, -rotationA * 0.5, 202, 316)
            }

            NumberAnimation on rotationA {
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 6200
                running: !root.exitStarted
            }

            NumberAnimation on rotationB {
                loops: Animation.Infinite
                from: 360
                to: 0
                duration: 5200
                running: !root.exitStarted
            }

            NumberAnimation on rotationC {
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 8600
                running: !root.exitStarted
            }

            SequentialAnimation on pulse {
                loops: Animation.Infinite
                running: !root.exitStarted
                NumberAnimation { from: 0.0; to: 1.0; duration: 1400; easing.type: Easing.InOutSine }
                NumberAnimation { from: 1.0; to: 0.0; duration: 1400; easing.type: Easing.InOutSine }
            }
        }

        Rectangle {
            id: energyCore
            width: parent.width * 0.36
            height: width
            radius: width / 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: parent.height * 0.04
            color: "#00000000"
            opacity: 0.24
            scale: 0.86

            gradient: Gradient {
                GradientStop { position: 0.00; color: "#F8E7AB" }
                GradientStop { position: 0.15; color: "#FFD76A" }
                GradientStop { position: 0.45; color: "#FFB300" }
                GradientStop { position: 1.00; color: "#00FFB300" }
            }
        }

        Rectangle {
            id: energyBeam
            width: 18
            height: parent.height * 0.30
            radius: 9
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: parent.height * 0.16
            opacity: 0.12

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00FFF1B0" }
                GradientStop { position: 0.18; color: "#C8FFF1B0" }
                GradientStop { position: 0.68; color: "#60FFB300" }
                GradientStop { position: 1.0; color: "#00FFB300" }
            }
        }

        SequentialAnimation {
            id: energyBurstAnimation
            running: true
            loops: Animation.Infinite

            ParallelAnimation {
                NumberAnimation { target: energyCore; property: "opacity"; from: 0.20; to: 0.52; duration: 1000; easing.type: Easing.InOutSine }
                NumberAnimation { target: energyCore; property: "scale"; from: 0.82; to: 1.06; duration: 1000; easing.type: Easing.InOutSine }
                NumberAnimation { target: energyBeam; property: "opacity"; from: 0.12; to: 0.40; duration: 1000; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                NumberAnimation { target: energyCore; property: "opacity"; from: 0.52; to: 0.78; duration: 900; easing.type: Easing.InOutSine }
                NumberAnimation { target: energyCore; property: "scale"; from: 1.06; to: 1.14; duration: 900; easing.type: Easing.InOutSine }
                NumberAnimation { target: energyBeam; property: "opacity"; from: 0.40; to: 0.58; duration: 900; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                NumberAnimation { target: energyCore; property: "opacity"; from: 0.78; to: 0.26; duration: 1200; easing.type: Easing.InOutSine }
                NumberAnimation { target: energyCore; property: "scale"; from: 1.14; to: 0.90; duration: 1200; easing.type: Easing.InOutSine }
                NumberAnimation { target: energyBeam; property: "opacity"; from: 0.58; to: 0.18; duration: 1200; easing.type: Easing.InOutSine }
            }
        }

        Canvas {
            id: flareCanvas
            anchors.fill: parent
            opacity: 0.0
            property real sweepX: -width * 0.3
            onSweepXChanged: requestPaint()

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const centerY = height * 0.53
                const flareWidth = width * 0.32
                const gradient = ctx.createLinearGradient(sweepX, 0, sweepX + flareWidth, 0)
                gradient.addColorStop(0.0, "rgba(255,255,255,0)")
                gradient.addColorStop(0.36, "rgba(255,215,128,0.10)")
                gradient.addColorStop(0.50, "rgba(255,246,214,0.44)")
                gradient.addColorStop(0.64, "rgba(255,179,0,0.18)")
                gradient.addColorStop(1.0, "rgba(255,255,255,0)")

                ctx.save()
                ctx.translate(width * 0.5, centerY)
                ctx.rotate(-0.26)
                ctx.translate(-width * 0.5, -centerY)
                ctx.fillStyle = gradient
                ctx.fillRect(sweepX, centerY - 160, flareWidth, 320)
                ctx.restore()
            }
        }

        Repeater {
            id: particleRepeater
            model: 64

            Rectangle {
                property real startX: root.width * (0.08 + ((index * 37) % 84) / 100)
                property real liftDuration: 2200 + (index % 9) * 260
                width: 1.4 + (index % 4) * 0.8
                height: width
                radius: width / 2
                color: index % 5 === 0 ? "#FFF1B0" : (index % 2 === 0 ? "#FFD76A" : "#FFB300")
                opacity: 0.12
                visible: root.particleEnabled
                x: startX
                y: root.height * (0.78 + ((index * 29) % 18) / 100)

                SequentialAnimation on y {
                    loops: Animation.Infinite
                    running: root.particleEnabled
                    NumberAnimation {
                        from: root.height * (0.76 + ((index * 17) % 16) / 100)
                        to: root.height * (0.10 + ((index * 11) % 24) / 100)
                        duration: liftDuration
                        easing.type: Easing.Linear
                    }
                }

                SequentialAnimation on x {
                    loops: Animation.Infinite
                    running: root.particleEnabled
                    NumberAnimation {
                        from: startX
                        to: startX + ((index % 2 === 0) ? 12 : -12)
                        duration: 900 + (index % 5) * 180
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        from: startX + ((index % 2 === 0) ? 12 : -12)
                        to: startX
                        duration: 900 + (index % 5) * 180
                        easing.type: Easing.InOutSine
                    }
                }

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: root.particleEnabled
                    NumberAnimation { from: 0.10; to: 0.38; duration: 700 + (index % 6) * 120; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.38; to: 0.10; duration: 900 + (index % 4) * 160; easing.type: Easing.InOutSine }
                }
            }
        }

        Item {
            id: titleBlock
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: parent.height * 0.06
            width: parent.width * 0.45
            height: 84
            opacity: 0.0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "AURIX"
                color: "#F7D988"
                font.pixelSize: 30
                font.bold: true
                font.letterSpacing: 8
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 46
                text: "MEDIRUN DASHBOARD"
                color: "#D3FFF1B0"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 4
            }
        }
    }

    Rectangle {
        id: fadeOverlay
        anchors.fill: parent
        color: "#000000"
        opacity: 1.0
    }

    ParallelAnimation {
        id: introFadeIn
        running: false
        NumberAnimation { target: fadeOverlay; property: "opacity"; from: 1.0; to: 0.0; duration: 600; easing.type: Easing.InOutSine }
        NumberAnimation { target: scene; property: "scale"; from: 1.04; to: 1.0; duration: 600; easing.type: Easing.InOutSine }
        onFinished: root.introComplete = true
    }

    Timer {
        interval: 2000
        running: true
        repeat: false
        onTriggered: {
            flareCanvas.opacity = 1.0
            flareSweep.start()
            flareBoost.start()
        }
    }

    NumberAnimation {
        id: flareSweep
        target: flareCanvas
        property: "sweepX"
        from: -root.width * 0.30
        to: root.width * 1.02
        duration: 900
        easing.type: Easing.InOutSine
        onFinished: flareCanvas.opacity = 0.0
    }

    SequentialAnimation {
        id: flareBoost
        running: false
        NumberAnimation { target: splashImage; property: "opacity"; from: 0.94; to: 1.0; duration: 220; easing.type: Easing.InOutSine }
        NumberAnimation { target: splashImage; property: "opacity"; from: 1.0; to: 0.93; duration: 380; easing.type: Easing.InOutSine }
    }

    Timer {
        interval: 2800
        running: true
        repeat: false
        onTriggered: titleFadeIn.start()
    }

    NumberAnimation {
        id: titleFadeIn
        target: titleBlock
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: 420
        easing.type: Easing.InOutSine
    }

    SequentialAnimation {
        id: outroAnimation
        running: false

        ParallelAnimation {
            NumberAnimation { target: ringCanvas; property: "fadeFactor"; to: 0.18; duration: 500; easing.type: Easing.InOutSine }
            NumberAnimation { target: fadeOverlay; property: "opacity"; to: 1.0; duration: 500; easing.type: Easing.InOutSine }
            NumberAnimation { target: scene; property: "scale"; to: 0.985; duration: 500; easing.type: Easing.InOutSine }
            NumberAnimation { target: titleBlock; property: "opacity"; to: 0.0; duration: 320; easing.type: Easing.InOutSine }
        }

        ScriptAction {
            script: root.exitAnimationFinished()
        }
    }
}
