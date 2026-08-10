#include "videorecorder.h"

#include <QDateTime>

#include <opencv2/imgproc.hpp>

namespace {
constexpr double kAssumedFps = 10.0; // RosCameraFeedProvider 실측 FPS를 안 재서 고정값으로 가정
}

bool VideoRecorder::start(const QString &filePath)
{
    m_filePath = filePath;
    m_opened = false;
    m_width = 0;
    m_height = 0;
    m_fps = kAssumedFps;
    m_frameCount = 0;
    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
    return true; // 실제 VideoWriter는 첫 프레임의 해상도를 알아야 열 수 있어 writeFrame에서 연다.
}

void VideoRecorder::writeFrame(const QImage &frame)
{
    if (frame.isNull()) {
        return;
    }

    if (!m_opened) {
        m_width = frame.width();
        m_height = frame.height();
        const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        // VIDEOWRITER_PROP_QUALITY로 화질을 올려보려 했으나, 이 환경의 OpenCV
        // FFmpeg/GStreamer 백엔드가 params 지정 open()을 아예 지원하지 않아
        // writer가 열리지도 않는 문제가 있었다(확인됨) — 기본 open()으로 되돌림.
        m_writer.open(m_filePath.toStdString(), fourcc, m_fps, cv::Size(m_width, m_height), true);
        m_opened = m_writer.isOpened();
        if (!m_opened) {
            return;
        }
    }

    const QImage rgb = frame.size() == QSize(m_width, m_height)
        ? frame.convertToFormat(QImage::Format_RGB888)
        : frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);

    cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar *>(rgb.bits()), static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgrMat;
    cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
    m_writer.write(bgrMat);
    ++m_frameCount;
}

VideoRecorder::Result VideoRecorder::stop()
{
    Result result;
    result.wroteAnyFrame = m_opened && m_frameCount > 0;
    result.width = m_width;
    result.height = m_height;
    result.fps = m_fps;
    result.durationSec = static_cast<int>((QDateTime::currentMSecsSinceEpoch() - m_startedAtMs) / 1000);

    if (m_writer.isOpened()) {
        m_writer.release();
    }
    m_opened = false;
    return result;
}
