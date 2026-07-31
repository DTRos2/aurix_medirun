#include "mapfile.h"

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cctype>
#include <cstring>

namespace {

// nav2 map_server가 만드는 binary PGM(P5)용 최소 파서. Qt에는 PNM 플러그인이
// 기본 포함되어 있지 않아 occupancy-grid 맵(.pgm)은 직접 읽어야 한다.
QImage loadPgmImage(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    auto skipWhitespaceAndComments = [&file]() {
        while (!file.atEnd()) {
            char c = 0;
            if (!file.getChar(&c)) {
                break;
            }
            if (c == '#') {
                file.readLine();
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                continue;
            }
            file.ungetChar(c);
            break;
        }
    };

    auto readToken = [&file, &skipWhitespaceAndComments]() {
        skipWhitespaceAndComments();
        QByteArray token;
        char c = 0;
        while (file.getChar(&c)) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
            token.append(c);
        }
        return token;
    };

    if (readToken() != QByteArrayLiteral("P5")) {
        return QImage();
    }

    bool widthOk = false;
    bool heightOk = false;
    bool maxValOk = false;
    const int width = readToken().toInt(&widthOk);
    const int height = readToken().toInt(&heightOk);
    const int maxVal = readToken().toInt(&maxValOk);
    if (!widthOk || !heightOk || !maxValOk || width <= 0 || height <= 0 || maxVal <= 0 || maxVal > 255) {
        return QImage();
    }

    const QByteArray raster = file.read(static_cast<qint64>(width) * height);
    if (raster.size() != width * height) {
        return QImage();
    }

    QImage image(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        std::memcpy(image.scanLine(y), raster.constData() + y * width, width);
    }
    return image;
}

void negateInPlace(QImage &image)
{
    image = image.convertToFormat(QImage::Format_Grayscale8);
    for (int y = 0; y < image.height(); ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            line[x] = static_cast<uchar>(255 - line[x]);
        }
    }
}

} // namespace

bool loadMapYaml(const QString &yamlPath, MapMeta &outMeta, QString &errorMessage)
{
    outMeta = MapMeta();

    YAML::Node root;
    try {
        root = YAML::LoadFile(yamlPath.toStdString());
    } catch (const std::exception &exc) {
        errorMessage = QStringLiteral("맵 YAML을 읽지 못했습니다: %1").arg(QString::fromStdString(exc.what()));
        return false;
    }

    if (!root.IsMap() || !root["image"] || !root["resolution"]) {
        errorMessage = QStringLiteral("맵 YAML에 'image' 또는 'resolution' 항목이 없습니다.");
        return false;
    }

    const QString imageName = QString::fromStdString(root["image"].as<std::string>());
    const double resolution = root["resolution"].as<double>();

    double originX = 0.0;
    double originY = 0.0;
    double originTheta = 0.0;
    if (root["origin"] && root["origin"].IsSequence()) {
        const YAML::Node origin = root["origin"];
        if (origin.size() > 0) {
            originX = origin[0].as<double>();
        }
        if (origin.size() > 1) {
            originY = origin[1].as<double>();
        }
        if (origin.size() > 2) {
            originTheta = origin[2].as<double>();
        }
    }

    const bool negate = root["negate"] ? (root["negate"].as<int>() != 0) : false;

    const QFileInfo yamlInfo(yamlPath);
    const QString imagePath = QDir(yamlInfo.absolutePath()).filePath(imageName);
    if (!QFile::exists(imagePath)) {
        errorMessage = QStringLiteral("맵 이미지 파일을 찾을 수 없습니다: %1").arg(imagePath);
        return false;
    }

    QImage image;
    if (imagePath.endsWith(QStringLiteral(".pgm"), Qt::CaseInsensitive)) {
        image = loadPgmImage(imagePath);
    } else {
        image.load(imagePath);
    }

    if (image.isNull()) {
        errorMessage = QStringLiteral("맵 이미지를 읽을 수 없습니다: %1").arg(imagePath);
        return false;
    }

    if (negate) {
        negateInPlace(image);
    } else {
        image = image.convertToFormat(QImage::Format_Grayscale8);
    }

    outMeta.valid = true;
    outMeta.width = image.width();
    outMeta.height = image.height();
    outMeta.resolution = resolution;
    outMeta.originX = originX;
    outMeta.originY = originY;
    outMeta.originTheta = originTheta;
    outMeta.image = image;

    if (std::abs(originTheta) > 1e-6) {
        errorMessage = QStringLiteral("맵 origin에 회전(theta)이 있어 좌표 변환이 정확하지 않을 수 있습니다.");
        // 회전 미지원은 경고일 뿐 실패는 아니므로 true를 반환한다.
    }

    return true;
}

QPointF pixelToWorld(const QPointF &pixel, const MapMeta &meta)
{
    const double wx = meta.originX + (pixel.x() + 0.5) * meta.resolution;
    const double wy = meta.originY + (meta.height - pixel.y() - 0.5) * meta.resolution;
    return QPointF(wx, wy);
}

QPointF worldToPixel(const QPointF &world, const MapMeta &meta)
{
    const double col = (world.x() - meta.originX) / meta.resolution - 0.5;
    const double row = meta.height - 0.5 - (world.y() - meta.originY) / meta.resolution;
    return QPointF(col, row);
}
