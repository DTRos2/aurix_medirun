#include "videorecorder.h"

#include <QDateTime>

#include <algorithm>

#include <opencv2/imgproc.hpp>

namespace {
constexpr double kFallbackFps = 10.0; // 실측이 아예 불가능할 때만 쓰는 최후 폴백
constexpr double kMinFps = 1.0;
constexpr double kMaxFps = 60.0;

// 워밍업 종료 조건: 충분한 샘플(5장) + 충분한 시간(1초)이 모이면 그 구간의
// 평균 간격으로 fps를 정한다. 프레임이 아주 빠르면(90장) 더 기다리지 않고,
// 반대로 아주 느리면(5초) 최소 2장만으로도 마감한다 — 어느 쪽이든 무한정
// 버퍼링하지 않도록.
constexpr int kWarmupMinFrames = 5;
constexpr qint64 kWarmupMinDurationMs = 1000;
constexpr int kWarmupMaxFrames = 90;
constexpr qint64 kWarmupMaxDurationMs = 5000;
}

bool VideoRecorder::start(const QString &filePath)
{
    m_filePath = filePath;
    m_opened = false;
    m_width = 0;
    m_height = 0;
    m_fps = 0.0;
    m_frameCount = 0;
    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_warmupFrames.clear();
    m_firstFrameAtMs = 0;
    return true; // 실제 VideoWriter는 워밍업으로 fps를 잰 뒤에 연다.
}

void VideoRecorder::openWriter(double fps)
{
    m_fps = std::clamp(fps > 0.0 ? fps : kFallbackFps, kMinFps, kMaxFps);
    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    // VIDEOWRITER_PROP_QUALITY로 화질을 올려보려 했으나, 이 환경의 OpenCV
    // FFmpeg/GStreamer 백엔드가 params 지정 open()을 아예 지원하지 않아
    // writer가 열리지도 않는 문제가 있었다(확인됨) — 기본 open()으로 되돌림.
    m_writer.open(m_filePath.toStdString(), fourcc, m_fps, cv::Size(m_width, m_height), true);
    m_opened = m_writer.isOpened();
}

void VideoRecorder::writeFrameToWriter(const QImage &frame)
{
    const QImage rgb = frame.size() == QSize(m_width, m_height)
        ? frame.convertToFormat(QImage::Format_RGB888)
        : frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);

    cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar *>(rgb.bits()), static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgrMat;
    cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
    m_writer.write(bgrMat);
    ++m_frameCount;
}

void VideoRecorder::writeFrame(const QImage &frame)
{
    if (frame.isNull()) {
        return;
    }

    if (m_opened) {
        writeFrameToWriter(frame);
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_warmupFrames.empty()) {
        m_firstFrameAtMs = now;
        m_width = frame.width();
        m_height = frame.height();
    }
    m_warmupFrames.push_back(frame);

    const qint64 elapsedMs = now - m_firstFrameAtMs;
    const auto frameCount = static_cast<int>(m_warmupFrames.size());
    const bool warmupDone = (frameCount >= kWarmupMinFrames && elapsedMs >= kWarmupMinDurationMs)
        || frameCount >= kWarmupMaxFrames
        || (frameCount >= 2 && elapsedMs >= kWarmupMaxDurationMs);
    if (!warmupDone) {
        return;
    }

    const double measuredFps = elapsedMs > 0 ? (frameCount - 1) * 1000.0 / static_cast<double>(elapsedMs) : kFallbackFps;
    openWriter(measuredFps);
    if (m_opened) {
        for (const QImage &buffered : m_warmupFrames) {
            writeFrameToWriter(buffered);
        }
    }
    m_warmupFrames.clear();
}

VideoRecorder::Result VideoRecorder::stop()
{
    // 워밍업이 끝나기 전에(아주 짧은 녹화) stop이 불리면, 지금까지 모은 것만으로
    // 열어서 살린다 — 안 그러면 짧은 녹화는 통째로 유실된다.
    if (!m_opened && !m_warmupFrames.empty()) {
        const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_firstFrameAtMs;
        const auto frameCount = static_cast<int>(m_warmupFrames.size());
        const double measuredFps = elapsedMs > 0 && frameCount > 1
            ? (frameCount - 1) * 1000.0 / static_cast<double>(elapsedMs)
            : kFallbackFps;
        openWriter(measuredFps);
        if (m_opened) {
            for (const QImage &buffered : m_warmupFrames) {
                writeFrameToWriter(buffered);
            }
        }
        m_warmupFrames.clear();
    }

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
