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

// DB는 TIMESTAMPTZ를 사용하고, 연결 직후 세션 timezone을 Asia/Seoul로 고정한다.
// 시간 컬럼을 읽을 때 QSqlQuery::value(...).toDateTime() 결과를 로컬(KST) 기준으로
// 일관되게 다루기 위해 이 함수를 사용한다.
QDateTime dbTimestampToLocal(const QVariant &value);

#endif // DBCONNECTION_H
