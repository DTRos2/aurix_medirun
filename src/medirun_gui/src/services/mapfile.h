#ifndef MAPFILE_H
#define MAPFILE_H

#include <QImage>
#include <QPointF>
#include <QString>

// nav2 map_server YAML(+ pgm/png) 로더. 회전(origin theta)은 지원하지 않는다 —
// map_saver 결과물 대부분이 theta=0이며, 회전이 있으면 경고만 남긴다.
struct MapMeta
{
    bool valid {false};
    int width {0};
    int height {0};
    double resolution {0.05};
    double originX {0.0};
    double originY {0.0};
    double originTheta {0.0};
    QImage image;
};

// yamlPath: nav2 map_server 형식 YAML (image/resolution/origin[/negate]).
// 성공하면 true를 반환하고 outMeta를 채운다. 실패하면 false + errorMessage.
// origin에 회전(theta)이 있으면 true를 반환하되 errorMessage에 경고를 남긴다
// (좌표 변환은 계속 진행하지만 부정확할 수 있음을 알리기 위함).
bool loadMapYaml(const QString &yamlPath, MapMeta &outMeta, QString &errorMessage);

// 맵 이미지 픽셀 좌표(col,row) <-> world(map frame, meter) 좌표 변환.
QPointF pixelToWorld(const QPointF &pixel, const MapMeta &meta);
QPointF worldToPixel(const QPointF &world, const MapMeta &meta);

#endif // MAPFILE_H
