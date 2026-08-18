#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <QImage>
#include <QString>

#include <opencv2/videoio.hpp>

#include <vector>

// QImage 프레임을 mp4 파일로 인코딩한다. cv_bridge가 이미 OpenCV를 끌고 오므로
// (roscamerafeedprovider.cpp 참고) 그 의존성을 그대로 재사용한다. fps를 미리
// 가정하지 않고, 이번 녹화의 첫 프레임들이 실제로 도착하는 간격을 재서 정한다
// (워밍업 동안은 프레임을 메모리에 버퍼링만 하고, 간격이 안정되면 그때
// VideoWriter를 그 fps로 열어 버퍼를 흘려보낸다) — 녹화 시작 시점의 순간적인
// 튐(예: 막 연결된 직후) 때문에 영상 전체의 재생 속도가 어긋나는 걸 막는다.
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
    void openWriter(double fps);
    void writeFrameToWriter(const QImage &frame);

    cv::VideoWriter m_writer;
    QString m_filePath;
    bool m_opened {false};
    int m_width {0};
    int m_height {0};
    double m_fps {0.0};
    int m_frameCount {0};
    qint64 m_startedAtMs {0};

    // 워밍업 상태: 첫 프레임들을 여기 모아뒀다가, 실측 fps가 나오면 한 번에 쓴다.
    std::vector<QImage> m_warmupFrames;
    qint64 m_firstFrameAtMs {0};
};

#endif // VIDEORECORDER_H
