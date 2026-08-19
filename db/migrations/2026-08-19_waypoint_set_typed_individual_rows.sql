-- waypoint_set을 "이름 하나 = 웨이포인트+경로 전체 번들" 방식에서
-- "행 하나 = 경유점 하나 / 홈 / 경로 하나"로 재설계한다.
-- 콜존(BTN_*)은 이 테이블이 아니라 call_button에서만 관리(중복 소스 제거).
--
-- 적용 대상: 기존에 db/schema/postgresql_schema.sql로 만든 DB(type/
-- deregistered_at 컬럼 없음). 새로 만드는 DB는 db/schema/postgresql_schema.sql만
-- 실행하면 되고 이 파일은 필요 없다.

ALTER TABLE waypoint_set
    ADD COLUMN type VARCHAR(16) NOT NULL DEFAULT 'waypoint'
        CHECK (type IN ('waypoint', 'home', 'trajectory')),
    ADD COLUMN deregistered_at TIMESTAMPTZ;

ALTER TABLE waypoint_set
    ADD CONSTRAINT waypoint_set_deregistered_check
        CHECK (deregistered_at IS NULL OR deregistered_at >= created_at);

-- 기존 name='home' 행을 type='home'으로 표시.
UPDATE waypoint_set SET type = 'home' WHERE name = 'home';

-- 예전 "전체 번들" 저장 방식으로 만들어진 btn_wifi 행은 콜존+홈이 뒤섞여
-- 있던 잔재라 새 모델과 안 맞음 — 삭제(콜존은 call_button이 이미 갖고 있음).
DELETE FROM waypoint_set WHERE name = 'btn_wifi';

-- robot_id+name UNIQUE였던 걸 robot_id+type+name으로 교체 — waypoint code와
-- trajectory 이름이 우연히 같아도 서로 다른 type이면 별도 행으로 취급.
ALTER TABLE waypoint_set DROP CONSTRAINT waypoint_set_robot_id_name_key;
ALTER TABLE waypoint_set ADD CONSTRAINT waypoint_set_robot_id_type_name_key UNIQUE (robot_id, type, name);

CREATE INDEX IF NOT EXISTS idx_waypoint_set_robot_type_active
ON waypoint_set (robot_id, type) WHERE deregistered_at IS NULL;

-- 참고: 이번 재설계 이전에 "전체 번들" 방식으로 저장된 나머지 레거시 행들
-- (default/d_test/d_test2/dd/test 등, 테스트 중 생성된 것으로 추정)은 새
-- 코드가 더 이상 읽지 않지만 이 마이그레이션에서 자동 삭제하지 않았다.
-- 필요 시 아래 조회로 확인 후 직접 정리할 것:
--   SELECT id, name, jsonb_typeof(waypoints) FROM waypoint_set WHERE type = 'waypoint' AND name NOT LIKE 'W-%';
