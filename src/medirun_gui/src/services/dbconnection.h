#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include <QString>

// config/db.yaml(호스트/포트/DB명/계정/비번)을 읽어 기본 QSqlDatabase 연결(QPSQL)을
// 연다. 이미 열려 있으면 그대로 성공 처리한다. 실패 시 false + errorMessage.
//
// 설정 파일 경로: AURIX_DB_CONFIG 환경변수가 있으면 그 경로, 없으면 현재 작업
// 디렉터리 기준 "config/db.yaml" (README 실행 절차상 항상 저장소 루트에서
// 실행되므로 이 상대 경로가 맞다. CONVENTIONS.md 3.2 — 계정별 절대 경로 금지).
bool openDbConnection(QString &errorMessage);

#endif // DBCONNECTION_H
