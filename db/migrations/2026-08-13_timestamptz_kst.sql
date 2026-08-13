BEGIN;

-- 기존 TIMESTAMP 컬럼들은 "UTC 벽시계값이 들어간 timestamp without time zone"
-- 상태였으므로, 실제 의미를 보존하려면 UTC 기준 절대시각으로 해석해
-- timestamptz로 바꿔야 한다.

ALTER TABLE aurix_user
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC',
    ALTER COLUMN last_login_at TYPE TIMESTAMPTZ USING last_login_at AT TIME ZONE 'UTC',
    ALTER COLUMN deleted_at TYPE TIMESTAMPTZ USING deleted_at AT TIME ZONE 'UTC';

ALTER TABLE robot
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC',
    ALTER COLUMN deregistered_at TYPE TIMESTAMPTZ USING deregistered_at AT TIME ZONE 'UTC';

ALTER TABLE robot_session
    ALTER COLUMN started_at TYPE TIMESTAMPTZ USING started_at AT TIME ZONE 'UTC',
    ALTER COLUMN ended_at TYPE TIMESTAMPTZ USING ended_at AT TIME ZONE 'UTC';

ALTER TABLE system_event
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC';

ALTER TABLE connection_status_log
    ALTER COLUMN changed_at TYPE TIMESTAMPTZ USING changed_at AT TIME ZONE 'UTC';

ALTER TABLE user_action_log
    ALTER COLUMN occurred_at TYPE TIMESTAMPTZ USING occurred_at AT TIME ZONE 'UTC',
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC';

ALTER TABLE control_command_log
    ALTER COLUMN issued_at TYPE TIMESTAMPTZ USING issued_at AT TIME ZONE 'UTC';

ALTER TABLE call_button
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC',
    ALTER COLUMN deregistered_at TYPE TIMESTAMPTZ USING deregistered_at AT TIME ZONE 'UTC';

ALTER TABLE mission
    ALTER COLUMN dispensed_at TYPE TIMESTAMPTZ USING dispensed_at AT TIME ZONE 'UTC',
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC',
    ALTER COLUMN dispatched_at TYPE TIMESTAMPTZ USING dispatched_at AT TIME ZONE 'UTC',
    ALTER COLUMN arrived_at TYPE TIMESTAMPTZ USING arrived_at AT TIME ZONE 'UTC',
    ALTER COLUMN ended_at TYPE TIMESTAMPTZ USING ended_at AT TIME ZONE 'UTC';

ALTER TABLE map
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC';

ALTER TABLE slam_job
    ALTER COLUMN started_at TYPE TIMESTAMPTZ USING started_at AT TIME ZONE 'UTC',
    ALTER COLUMN ended_at TYPE TIMESTAMPTZ USING ended_at AT TIME ZONE 'UTC';

ALTER TABLE waypoint_set
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC';

ALTER TABLE nav_goal_log
    ALTER COLUMN sent_at TYPE TIMESTAMPTZ USING sent_at AT TIME ZONE 'UTC',
    ALTER COLUMN finished_at TYPE TIMESTAMPTZ USING finished_at AT TIME ZONE 'UTC';

ALTER TABLE camera_session
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC';

ALTER TABLE camera_recording
    ALTER COLUMN recorded_at TYPE TIMESTAMPTZ USING recorded_at AT TIME ZONE 'UTC';

ALTER TABLE object_detection
    ALTER COLUMN detected_at TYPE TIMESTAMPTZ USING detected_at AT TIME ZONE 'UTC';

ALTER TABLE files
    ALTER COLUMN created_at TYPE TIMESTAMPTZ USING created_at AT TIME ZONE 'UTC',
    ALTER COLUMN updated_at TYPE TIMESTAMPTZ USING updated_at AT TIME ZONE 'UTC',
    ALTER COLUMN deleted_at TYPE TIMESTAMPTZ USING deleted_at AT TIME ZONE 'UTC';

ALTER TABLE driving_session
    ALTER COLUMN started_at TYPE TIMESTAMPTZ USING started_at AT TIME ZONE 'UTC',
    ALTER COLUMN ended_at TYPE TIMESTAMPTZ USING ended_at AT TIME ZONE 'UTC';

ALTER TABLE driving_record
    ALTER COLUMN recorded_at TYPE TIMESTAMPTZ USING recorded_at AT TIME ZONE 'UTC';

COMMIT;
