#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <QImage>
#include <QString>

#include <opencv2/videoio.hpp>

// QImage 프레임을 mp4 파일로 인코딩한다. cv_bridge가 이미 OpenCV를 끌고 오므로
// (roscamerafeedprovider.cpp 참고) 그 의존성을 그대로 재사용한다. FPS는 실측하지
// 않고 첫 프레임 시점에 고정값으로 연다 — 실제 카메라 프레임 속도가 다르면
// 재생 속도가 다소 어긋날 수 있다(알려진 한계).
class VideoRecorder
{
public:
    bool start(const QString &filePath);
    void writeFrame(const QImage &frame);
    // 종료 후 실제 기록된 해상도/길이를 알아야 camera_recording에 남길 수 있어 반환한다.
    struct Result
    {
        bool wroteAnyFrame {false};
        int width {0};
        int height {0};
        double fps {0.0};
        int durationSec {0};
    };
    Result stop();

private:
    cv::VideoWriter m_writer;
    QString m_filePath;
    bool m_opened {false};
    int m_width {0};
    int m_height {0};
    double m_fps {0.0};
    int m_frameCount {0};
    qint64 m_startedAtMs {0};
};

#endif // VIDEORECORDER_H
