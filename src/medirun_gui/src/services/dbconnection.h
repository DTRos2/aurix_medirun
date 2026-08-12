#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include <QDateTime>
#include <QString>
#include <QVariant>

// config/db.yaml(호스트/포트/DB명/계정/비번)을 읽어 기본 QSqlDatabase 연결(QPSQL)을
// 연다. 이미 열려 있으면 그대로 성공 처리한다. 실패 시 false + errorMessage.
//
// 설정 파일 경로: AURIX_DB_CONFIG 환경변수가 있으면 그 경로를 쓰고, 없으면 현재
// 작업 디렉터리와 실행 파일 디렉터리에서 상위 10단계까지 올라가며
// "config/db.yaml"을 찾는다.
bool openDbConnection(QString &errorMessage);

// DB 스키마의 TIMESTAMP(시간대 없음) 컬럼은 서버 SHOW timezone이 Asia/Seoul로
// 정상 설정돼 있어도, Qt QPSQL 드라이버가 연결을 맺을 때 세션을 UTC로 돌려버려
// NOW()/바인딩된 QDateTime 둘 다 UTC 벽시계 값으로 저장된다(실측: 실제
// 18시대에 쓴 행이 09시대로 저장됨, 정확히 9시간 차이). DB 자체를 고치는 대신
// — 이미 쌓인 행도 있고 모든 쓰기 경로가 같은 어긋남을 겪으므로 — 읽어올
// 때마다 "저장된 값은 사실 UTC였다"고 재해석해서 로컬 시간대로 변환한다.
// QSqlQuery::value(...).toDateTime()으로 시간 컬럼을 읽는 모든 곳에서 이 함수를
// 대신 써야 한다.
QDateTime dbTimestampToLocal(const QVariant &value);

#endif // DBCONNECTION_H
