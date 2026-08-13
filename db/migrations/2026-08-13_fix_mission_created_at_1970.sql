BEGIN;

-- mission.created_at가 1970년대로 들어간 행들을 복구한다.
-- 현재 코드/데이터 패턴상 created_at만 비정상이고 dispatched/arrived/ended는
-- 실제 2026 시각으로 들어가 있으므로, 우선순위에 따라 가장 이른 정상 시각으로
-- created_at를 대체한다.
UPDATE mission
SET created_at = COALESCE(
    dispatched_at,
    arrived_at,
    ended_at,
    NOW()
)
WHERE created_at < TIMESTAMPTZ '2000-01-01 00:00:00+09';

COMMIT;
