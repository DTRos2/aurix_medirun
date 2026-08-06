-- Qt GUI 웨이포인트 관리 페이지가 YAML 파일과 동일한 구조(frame_id + 이름있는
-- waypoint 딕셔너리 + trajectory 딕셔너리)를 통째로 waypoint_set.waypoints에
-- 저장할 수 있도록, 배열만 허용하던 CHECK 제약을 배열/객체 모두 허용하게 완화한다.
-- 적용 대상: 기존에 db/schema/postgresql_schema.sql로 만든 DB(배열만 허용하는 제약 있음).
-- 새로 만드는 DB는 db/schema/postgresql_schema.sql만 실행하면 되고 이 파일은 필요 없다.

ALTER TABLE waypoint_set DROP CONSTRAINT IF EXISTS waypoint_set_waypoints_check;
ALTER TABLE waypoint_set ADD CONSTRAINT waypoint_set_waypoints_check
    CHECK (jsonb_typeof(waypoints) IN ('array', 'object'));
