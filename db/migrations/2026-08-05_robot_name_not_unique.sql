-- 같은 로봇을 여러 세션/모드로 중복 등록할 수 있어야 해서 robot.name UNIQUE
-- 제약을 제거한다. 이후 robot은 id로만 구분한다.
-- 적용 대상: 기존에 db/schema/postgresql_schema.sql로 만든 DB (UNIQUE 제약 있음).
-- 새로 만드는 DB는 db/schema/postgresql_schema.sql만 실행하면 되고 이 파일은 필요 없다.

ALTER TABLE robot DROP CONSTRAINT IF EXISTS robot_name_key;
