-- =========================================================
-- AURIX MediRun Dashboard DB Schema
-- PostgreSQL
--
-- 최종 ERD 반영 사항 (원안 유지)
-- - PK id 컬럼: INTEGER GENERATED ALWAYS AS IDENTITY
-- - *_id 논리 연결 컬럼: INTEGER
-- - 시간 컬럼: TIMESTAMPTZ
-- - waypoint_set.waypoints: JSONB
-- - 실제 FK 제약조건은 생성하지 않음 (애플리케이션 코드에서 관리)
-- - 제거된 컬럼: system_event.source / driving_session.label /
--   files.description / files.file_size
-- =========================================================

BEGIN;

-- =========================================================
-- 0. 기존 테이블 삭제
-- =========================================================
DROP TABLE IF EXISTS driving_record;
DROP TABLE IF EXISTS driving_session;
DROP TABLE IF EXISTS files;
DROP TABLE IF EXISTS object_detection;
DROP TABLE IF EXISTS camera_recording;
DROP TABLE IF EXISTS camera_session;
DROP TABLE IF EXISTS nav_goal_log;
DROP TABLE IF EXISTS waypoint_set;
DROP TABLE IF EXISTS slam_job;
DROP TABLE IF EXISTS map;
DROP TABLE IF EXISTS mission;
DROP TABLE IF EXISTS call_button;
DROP TABLE IF EXISTS control_command_log;
DROP TABLE IF EXISTS user_action_log;
DROP TABLE IF EXISTS connection_status_log;
DROP TABLE IF EXISTS system_event;
DROP TABLE IF EXISTS robot_session;
DROP TABLE IF EXISTS robot;
DROP TABLE IF EXISTS aurix_user;

-- =========================================================
-- 1. Users / Auth
-- =========================================================

CREATE TABLE aurix_user (
    id             INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    login_id       VARCHAR(64) NOT NULL,
    password_hash  VARCHAR(255) NOT NULL,
    role           VARCHAR(16) NOT NULL CHECK (
        role IN ('admin', 'staff')
    ),
    created_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_login_at  TIMESTAMPTZ,
    deleted_at     TIMESTAMPTZ,
    CHECK (deleted_at IS NULL OR deleted_at >= created_at)
);

-- login_id는 삭제된(deleted_at IS NOT NULL) 계정끼리는 중복 가능(재사용 목적) —
-- 동시에 활성 상태인 계정끼리만 중복을 막는 부분 유니크 인덱스.
CREATE UNIQUE INDEX aurix_user_active_login_id_idx
ON aurix_user (login_id) WHERE deleted_at IS NULL;

COMMENT ON TABLE aurix_user IS '대시보드 로그인 계정 (usermanagementpage / AuthService)';
COMMENT ON COLUMN aurix_user.id IS '사용자 PK, 자동 증가';
COMMENT ON COLUMN aurix_user.login_id IS '로그인 ID. 활성 계정끼리만 유니크(삭제된 아이디는 재사용 가능)';
COMMENT ON COLUMN aurix_user.password_hash IS '비밀번호 해시';
COMMENT ON COLUMN aurix_user.role IS '권한 (UserRole: admin/staff)';
COMMENT ON COLUMN aurix_user.created_at IS '계정 생성 시간';
COMMENT ON COLUMN aurix_user.last_login_at IS '마지막 로그인 시간';
COMMENT ON COLUMN aurix_user.deleted_at IS '계정 삭제 시간. NULL이면 사용 중';

-- =========================================================
-- 2. Core / Robot
-- =========================================================

CREATE TABLE robot (
    id              INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name            VARCHAR(64) NOT NULL,
    model           VARCHAR(64) NOT NULL DEFAULT 'turtlebot3_burger',
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    deregistered_at TIMESTAMPTZ,
    CHECK (deregistered_at IS NULL OR deregistered_at >= created_at)
);

COMMENT ON TABLE robot IS '로봇 등록 정보';
COMMENT ON COLUMN robot.id IS '로봇 PK, 자동 증가';
COMMENT ON COLUMN robot.name IS '로봇 이름, 예: burger01';
COMMENT ON COLUMN robot.model IS '로봇 모델명';
COMMENT ON COLUMN robot.created_at IS '로봇 등록 시간';
COMMENT ON COLUMN robot.deregistered_at IS '로봇 등록 해제 시간. NULL이면 사용 중';

CREATE TABLE robot_session (
    id           INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id     INTEGER NOT NULL,
    session_name VARCHAR(120) NOT NULL,
    mode         VARCHAR(32) NOT NULL CHECK (
        mode IN ('manual', 'auto', 'unity', 'playback', 'mapping')
    ),
    status       VARCHAR(32) NOT NULL CHECK (
        status IN ('planned', 'running', 'paused', 'completed', 'failed', 'aborted')
    ),
    started_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at     TIMESTAMPTZ,
    CHECK (ended_at IS NULL OR ended_at >= started_at)
);

COMMENT ON TABLE robot_session IS '로봇의 작업 실행 단위';
COMMENT ON COLUMN robot_session.id IS '세션 PK, 자동 증가';
COMMENT ON COLUMN robot_session.robot_id IS '대상 로봇 id. 실제 FK 제약 없이 코드에서 관리';
COMMENT ON COLUMN robot_session.session_name IS '세션 이름';
COMMENT ON COLUMN robot_session.mode IS '세션 작업 모드';
COMMENT ON COLUMN robot_session.status IS '세션 상태';
COMMENT ON COLUMN robot_session.started_at IS '세션 시작 시간';
COMMENT ON COLUMN robot_session.ended_at IS '세션 종료 시간';

CREATE INDEX idx_robot_session_robot_started_at
ON robot_session (robot_id, started_at DESC);

-- =========================================================
-- 3. Logs / Control
-- =========================================================

CREATE TABLE system_event (
    id          INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id  INTEGER,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    event_type  VARCHAR(64) NOT NULL,
    severity    VARCHAR(16) NOT NULL CHECK (
        severity IN ('info', 'warn', 'error', 'critical')
    ),
    message     TEXT NOT NULL
);

COMMENT ON TABLE system_event IS '시스템이 자동 감지해 남기는 이벤트 로그 (연결 상태/사용자 조작/제어 명령 제외)';
COMMENT ON COLUMN system_event.id IS '이벤트 PK, 자동 증가';
COMMENT ON COLUMN system_event.session_id IS '이벤트가 발생한 세션 id. 세션 시작 전/외부에서 발생한 이벤트는 NULL';
COMMENT ON COLUMN system_event.created_at IS '이벤트 발생 시간';
COMMENT ON COLUMN system_event.event_type IS '이벤트 종류, 예: nav_plan_failed, estop_triggered, battery_low, obstacle_avoided';
COMMENT ON COLUMN system_event.severity IS '심각도';
COMMENT ON COLUMN system_event.message IS '이벤트 메시지';

CREATE INDEX idx_system_event_session_created_at
ON system_event (session_id, created_at DESC);

CREATE TABLE connection_status_log (
    id         INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id   INTEGER NOT NULL,
    session_id INTEGER,
    target     VARCHAR(32) NOT NULL CHECK (
        target IN ('ros_bridge', 'database', 'serial', 'camera', 'unity')
    ),
    status     VARCHAR(16) NOT NULL CHECK (
        status IN ('connected', 'disconnected', 'reconnecting', 'error')
    ),
    detail     TEXT,
    changed_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

COMMENT ON TABLE connection_status_log IS '외부 연결 상태 변화 로그';
COMMENT ON COLUMN connection_status_log.id IS '연결 상태 로그 PK, 자동 증가';
COMMENT ON COLUMN connection_status_log.robot_id IS '대상 로봇 id';
COMMENT ON COLUMN connection_status_log.session_id IS '연관 세션 id';
COMMENT ON COLUMN connection_status_log.target IS '연결 대상';
COMMENT ON COLUMN connection_status_log.status IS '연결 상태';
COMMENT ON COLUMN connection_status_log.detail IS '상세 내용';
COMMENT ON COLUMN connection_status_log.changed_at IS '상태 변경 시간';

CREATE INDEX idx_connection_status_log_robot_target_changed_at
ON connection_status_log (robot_id, target, changed_at DESC);

CREATE TABLE user_action_log (
    id          INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id    INTEGER NOT NULL,
    session_id  INTEGER,
    user_id     INTEGER,
    mission_id  INTEGER,
    action_type VARCHAR(32) NOT NULL CHECK (
        action_type IN (
            'slam_start',
            'slam_stop',
            'map_save',
            'map_load',
            'waypoint_set_create',
            'waypoint_set_delete',
            'waypoint_set_run',
            'recording_start',
            'recording_stop',
            'mode_change',
            'mission_accept',
            'mission_retry',
            'mission_cancel',
            'mission_return_home'
        )
    ),
    payload     JSONB,
    issued_by   VARCHAR(64),
    occurred_at TIMESTAMPTZ,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

COMMENT ON TABLE user_action_log IS 'Qt GUI 사용자 조작 로그';
COMMENT ON COLUMN user_action_log.id IS '사용자 액션 로그 PK, 자동 증가';
COMMENT ON COLUMN user_action_log.robot_id IS '대상 로봇 id';
COMMENT ON COLUMN user_action_log.session_id IS '연관 세션 id';
COMMENT ON COLUMN user_action_log.user_id IS '조작한 계정 id (aurix_user). 로그인 없이 발생한 조작은 NULL';
COMMENT ON COLUMN user_action_log.mission_id IS '연관 미션 id. 미션과 무관한 조작(SLAM/웨이포인트 등)은 NULL';
COMMENT ON COLUMN user_action_log.action_type IS '사용자 조작 종류';
COMMENT ON COLUMN user_action_log.payload IS '조작 상세 데이터';
COMMENT ON COLUMN user_action_log.issued_by IS '조작 사용자 (표시용 문자열, 신뢰 가능한 식별은 user_id 사용)';
COMMENT ON COLUMN user_action_log.occurred_at IS '사용자가 실제 조작한 시간';
COMMENT ON COLUMN user_action_log.created_at IS 'DB 저장 시간';

CREATE INDEX idx_user_action_log_robot_created_at
ON user_action_log (robot_id, created_at DESC);

CREATE INDEX idx_user_action_log_mission_created_at
ON user_action_log (mission_id, created_at DESC);

CREATE TABLE control_command_log (
    id                    INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id            INTEGER NOT NULL,
    command_source        VARCHAR(32) NOT NULL CHECK (
        command_source IN ('qt', 'unity', 'navigation', 'playback', 'safety')
    ),
    authority_state       VARCHAR(32) NOT NULL CHECK (
        authority_state IN ('granted', 'blocked', 'revoked')
    ),
    command_type          VARCHAR(32) NOT NULL CHECK (
        command_type IN ('drive', 'stop', 'estop', 'mode_change', 'goal', 'resume')
    ),
    issued_by             VARCHAR(64),
    linear_speed          DOUBLE PRECISION NOT NULL DEFAULT 0,
    angular_speed         DOUBLE PRECISION NOT NULL DEFAULT 0,
    emergency_stop_active BOOLEAN NOT NULL DEFAULT FALSE,
    issued_at             TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

COMMENT ON TABLE control_command_log IS '제어 명령 감사 로그';
COMMENT ON COLUMN control_command_log.id IS '제어 명령 로그 PK, 자동 증가';
COMMENT ON COLUMN control_command_log.session_id IS '연관 세션 id';
COMMENT ON COLUMN control_command_log.command_source IS '명령 출처';
COMMENT ON COLUMN control_command_log.authority_state IS '제어권 상태';
COMMENT ON COLUMN control_command_log.command_type IS '명령 종류';
COMMENT ON COLUMN control_command_log.issued_by IS '명령 사용자';
COMMENT ON COLUMN control_command_log.linear_speed IS '명령 선속도';
COMMENT ON COLUMN control_command_log.angular_speed IS '명령 각속도';
COMMENT ON COLUMN control_command_log.emergency_stop_active IS '비상정지 활성 여부';
COMMENT ON COLUMN control_command_log.issued_at IS '명령 발생 시간';

CREATE INDEX idx_control_command_log_session_issued_at
ON control_command_log (session_id, issued_at DESC);

-- =========================================================
-- 4. Missions / Calls
-- =========================================================

CREATE TABLE call_button (
    id              INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    code            VARCHAR(32) NOT NULL UNIQUE,
    zone_name       VARCHAR(120) NOT NULL,
    map_id          INTEGER,
    pos_x           DOUBLE PRECISION,
    pos_y           DOUBLE PRECISION,
    yaw             DOUBLE PRECISION,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    deregistered_at TIMESTAMPTZ,
    CHECK (deregistered_at IS NULL OR deregistered_at >= created_at)
);

COMMENT ON TABLE call_button IS '웨이포인트별 호출 버튼 등록 (하드웨어는 ESP32+MQTT). Qt GUI 웨이포인트 관리 페이지가 BTN_* 콜존을 추가/수정할 때마다 upsert하며, GUI 시작 시 이 테이블 값이 웨이포인트 파일보다 우선한다';
COMMENT ON COLUMN call_button.id IS '호출 버튼 PK, 자동 증가';
COMMENT ON COLUMN call_button.code IS '버튼 코드, 예: BTN_01';
COMMENT ON COLUMN call_button.zone_name IS '버튼 설치 구역 이름, 예: 중앙 대합실';
COMMENT ON COLUMN call_button.map_id IS '버튼 좌표 기준 맵 id. 좌표 보정 전엔 NULL';
COMMENT ON COLUMN call_button.pos_x IS '맵 기준 버튼 x 좌표. 좌표 보정 전엔 NULL';
COMMENT ON COLUMN call_button.pos_y IS '맵 기준 버튼 y 좌표. 좌표 보정 전엔 NULL';
COMMENT ON COLUMN call_button.yaw IS '맵 기준 버튼 도착 시 로봇이 바라볼 방향(라디안). 좌표 보정 전엔 NULL';
COMMENT ON COLUMN call_button.created_at IS '버튼 등록 시간';
COMMENT ON COLUMN call_button.deregistered_at IS '버튼 등록 해제 시간. NULL이면 사용 중';

CREATE TABLE mission (
    id             INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id       INTEGER,
    session_id     INTEGER,
    call_button_id INTEGER NOT NULL,
    is_emergency   BOOLEAN NOT NULL DEFAULT FALSE,
    status         VARCHAR(32) NOT NULL CHECK (
        status IN (
            'queued', 'dispatched', 'navigating', 'replanning',
            'arrived', 'dispensing', 'completed', 'cancelled', 'failed'
        )
    ),
    dispensed_at   TIMESTAMPTZ,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    dispatched_at  TIMESTAMPTZ,
    arrived_at     TIMESTAMPTZ,
    ended_at       TIMESTAMPTZ,
    CHECK (dispatched_at IS NULL OR dispatched_at >= created_at),
    CHECK (arrived_at IS NULL OR dispatched_at IS NULL OR arrived_at >= dispatched_at),
    CHECK (ended_at IS NULL OR ended_at >= created_at)
);

COMMENT ON TABLE mission IS '호출 -> 출동 -> 도착 -> 반출 -> 완료 흐름의 배송 미션';
COMMENT ON COLUMN mission.id IS '미션 PK, 자동 증가. 화면에 표시되는 미션 번호(#142 등)';
COMMENT ON COLUMN mission.robot_id IS '배정된 로봇 id. 대기 큐 상태(미배정)에서는 NULL';
COMMENT ON COLUMN mission.session_id IS '수행 중 생성된 robot_session id. 출동 전에는 NULL';
COMMENT ON COLUMN mission.call_button_id IS '이 미션을 발생시킨 호출 버튼 id';
COMMENT ON COLUMN mission.is_emergency IS '긴급 호출 여부';
COMMENT ON COLUMN mission.status IS '미션 상태';
COMMENT ON COLUMN mission.dispensed_at IS '반출(물품 수령) 확인 시간. NULL이면 미반출';
COMMENT ON COLUMN mission.created_at IS '호출 접수 시간';
COMMENT ON COLUMN mission.dispatched_at IS '로봇 출발 시간';
COMMENT ON COLUMN mission.arrived_at IS '목적지 도착 시간';
COMMENT ON COLUMN mission.ended_at IS '완료/취소/실패로 종료된 시간';

CREATE INDEX idx_mission_status_created_at
ON mission (status, created_at DESC);

CREATE INDEX idx_mission_robot_created_at
ON mission (robot_id, created_at DESC);

CREATE INDEX idx_mission_call_button_created_at
ON mission (call_button_id, created_at DESC);

-- =========================================================
-- 5. Map / SLAM / Navigation
-- =========================================================

CREATE TABLE map (
    id          INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id    INTEGER NOT NULL,
    name        VARCHAR(120) NOT NULL,
    raw_yaml    JSONB NOT NULL,
    resolution  DOUBLE PRECISION CHECK (resolution > 0),
    origin_x    DOUBLE PRECISION NOT NULL DEFAULT 0,
    origin_y    DOUBLE PRECISION NOT NULL DEFAULT 0,
    origin_yaw  DOUBLE PRECISION NOT NULL DEFAULT 0,
    width_px    INTEGER CHECK (width_px > 0),
    height_px   INTEGER CHECK (height_px > 0),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (robot_id, name)
);

COMMENT ON TABLE map IS 'SLAM 맵 메타데이터';
COMMENT ON COLUMN map.id IS '맵 PK, 자동 증가';
COMMENT ON COLUMN map.robot_id IS '대상 로봇 id';
COMMENT ON COLUMN map.name IS '맵 이름';
COMMENT ON COLUMN map.raw_yaml IS 'map.yaml 내용을 JSONB로 저장';
COMMENT ON COLUMN map.resolution IS '맵 해상도';
COMMENT ON COLUMN map.origin_x IS '맵 원점 x';
COMMENT ON COLUMN map.origin_y IS '맵 원점 y';
COMMENT ON COLUMN map.origin_yaw IS '맵 원점 yaw';
COMMENT ON COLUMN map.width_px IS '맵 이미지 가로 픽셀';
COMMENT ON COLUMN map.height_px IS '맵 이미지 세로 픽셀';
COMMENT ON COLUMN map.created_at IS '맵 생성 시간';

CREATE INDEX idx_map_robot_created_at
ON map (robot_id, created_at DESC);

CREATE TABLE slam_job (
    id           INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id   INTEGER NOT NULL,
    map_id       INTEGER,
    map_name     VARCHAR(120) NOT NULL,
    slam_backend VARCHAR(32),
    status       VARCHAR(32) NOT NULL CHECK (
        status IN ('queued', 'running', 'saved', 'failed', 'cancelled')
    ),
    started_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at     TIMESTAMPTZ,
    CHECK (ended_at IS NULL OR ended_at >= started_at)
);

COMMENT ON TABLE slam_job IS 'SLAM 매핑 작업 단위';
COMMENT ON COLUMN slam_job.id IS 'SLAM 작업 PK, 자동 증가';
COMMENT ON COLUMN slam_job.session_id IS '연관 세션 id';
COMMENT ON COLUMN slam_job.map_id IS '저장된 맵 id';
COMMENT ON COLUMN slam_job.map_name IS '저장할 맵 이름';
COMMENT ON COLUMN slam_job.slam_backend IS 'SLAM 백엔드';
COMMENT ON COLUMN slam_job.status IS 'SLAM 작업 상태';
COMMENT ON COLUMN slam_job.started_at IS '작업 시작 시간';
COMMENT ON COLUMN slam_job.ended_at IS '작업 종료 시간';

CREATE INDEX idx_slam_job_session_started_at
ON slam_job (session_id, started_at DESC);

CREATE TABLE waypoint_set (
    id              INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id        INTEGER NOT NULL,
    map_id          INTEGER,
    type            VARCHAR(16) NOT NULL DEFAULT 'waypoint' CHECK (
        type IN ('waypoint', 'home', 'trajectory')
    ),
    name            VARCHAR(120) NOT NULL,
    description     TEXT,
    waypoints       JSONB NOT NULL DEFAULT '[]'::jsonb,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    deregistered_at TIMESTAMPTZ,
    UNIQUE (robot_id, type, name),
    CHECK (jsonb_typeof(waypoints) IN ('array', 'object')),
    CHECK (deregistered_at IS NULL OR deregistered_at >= created_at)
);

COMMENT ON TABLE waypoint_set IS '경유점/홈/경로를 type으로 구분해 개별 행으로 관리. 경유점·홈은 한 행 = 한 지점(waypoints: 단일 항목 object), 경로는 한 행 = 한 저장 경로(waypoints: 웨이포인트 code 순서 배열). 콜존은 이 테이블이 아니라 call_button에서 관리';
COMMENT ON COLUMN waypoint_set.id IS 'PK, 자동 증가';
COMMENT ON COLUMN waypoint_set.robot_id IS '대상 로봇 id';
COMMENT ON COLUMN waypoint_set.map_id IS '기준 맵 id';
COMMENT ON COLUMN waypoint_set.type IS '행 종류 — waypoint(경유점)/home(홈)/trajectory(경로)';
COMMENT ON COLUMN waypoint_set.name IS 'waypoint/home 행은 웨이포인트 code(예: W-1, HOME), trajectory 행은 사용자가 입력한 경로 이름';
COMMENT ON COLUMN waypoint_set.description IS '설명(선택)';
COMMENT ON COLUMN waypoint_set.waypoints IS 'type=waypoint/home: 단일 지점 object({x,y,yaw,label}). type=trajectory: 순서대로 방문할 스텝 배열(JSONB array), 각 원소는 {"table":"call_button"|"waypoint","id":N} — call_button.code/waypoint_set.name 문자열은 두 테이블에서 각자 독립적으로 매겨져 서로 겹칠 수 있어 테이블+id로 명시';
COMMENT ON COLUMN waypoint_set.created_at IS '등록 시간';
COMMENT ON COLUMN waypoint_set.deregistered_at IS '등록 해제 시간. NULL이면 사용 중';

CREATE INDEX idx_waypoint_set_robot_type_active
ON waypoint_set (robot_id, type) WHERE deregistered_at IS NULL;

CREATE TABLE nav_goal_log (
    id                  INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id            INTEGER NOT NULL,
    session_id          INTEGER NOT NULL,
    waypoint_set_id     INTEGER,
    waypoint_seq_no     INTEGER CHECK (waypoint_seq_no IS NULL OR waypoint_seq_no >= 0),
    start_x             DOUBLE PRECISION,
    start_y             DOUBLE PRECISION,
    start_yaw           DOUBLE PRECISION,
    goal_x              DOUBLE PRECISION NOT NULL,
    goal_y              DOUBLE PRECISION NOT NULL,
    goal_yaw            DOUBLE PRECISION NOT NULL DEFAULT 0,
    applied_speed_scale REAL CHECK (applied_speed_scale IS NULL OR applied_speed_scale > 0),
    status              VARCHAR(32) NOT NULL CHECK (
        status IN ('queued', 'accepted', 'navigating', 'arrived', 'failed', 'cancelled')
    ),
    sent_at             TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    finished_at         TIMESTAMPTZ,
    CHECK (finished_at IS NULL OR finished_at >= sent_at)
);

COMMENT ON TABLE nav_goal_log IS 'Nav2 목표 전송 및 결과 로그';
COMMENT ON COLUMN nav_goal_log.id IS 'Nav goal 로그 PK, 자동 증가';
COMMENT ON COLUMN nav_goal_log.robot_id IS '대상 로봇 id';
COMMENT ON COLUMN nav_goal_log.session_id IS '연관 세션 id';
COMMENT ON COLUMN nav_goal_log.waypoint_set_id IS 'Waypoint 세트 id';
COMMENT ON COLUMN nav_goal_log.waypoint_seq_no IS 'Waypoint JSON 배열 내 순서 번호';
COMMENT ON COLUMN nav_goal_log.start_x IS '목표 전송 시점 시작 x';
COMMENT ON COLUMN nav_goal_log.start_y IS '목표 전송 시점 시작 y';
COMMENT ON COLUMN nav_goal_log.start_yaw IS '목표 전송 시점 시작 yaw';
COMMENT ON COLUMN nav_goal_log.goal_x IS '목표 x';
COMMENT ON COLUMN nav_goal_log.goal_y IS '목표 y';
COMMENT ON COLUMN nav_goal_log.goal_yaw IS '목표 yaw';
COMMENT ON COLUMN nav_goal_log.applied_speed_scale IS '적용 속도 배율';
COMMENT ON COLUMN nav_goal_log.status IS '목표 상태';
COMMENT ON COLUMN nav_goal_log.sent_at IS '목표 전송 시간';
COMMENT ON COLUMN nav_goal_log.finished_at IS '목표 종료 시간';

CREATE INDEX idx_nav_goal_log_session_sent_at
ON nav_goal_log (session_id, sent_at DESC);

-- =========================================================
-- 6. Camera / Vision
-- =========================================================

CREATE TABLE camera_session (
    id                INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id        INTEGER NOT NULL,
    driving_session_id INTEGER,
    camera_name       VARCHAR(64) NOT NULL,
    topic_name        VARCHAR(128) NOT NULL,
    resolution_width  INTEGER CHECK (resolution_width IS NULL OR resolution_width > 0),
    resolution_height INTEGER CHECK (resolution_height IS NULL OR resolution_height > 0),
    target_fps        REAL CHECK (target_fps IS NULL OR target_fps > 0),
    encoder_name      VARCHAR(32),
    stream_status     VARCHAR(16) NOT NULL DEFAULT 'ready' CHECK (
        stream_status IN ('ready', 'live', 'recording', 'offline', 'error')
    ),
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (session_id, camera_name)
);

COMMENT ON TABLE camera_session IS '카메라 스트리밍 세션';
COMMENT ON COLUMN camera_session.id IS '카메라 세션 PK, 자동 증가';
COMMENT ON COLUMN camera_session.session_id IS '연관 세션 id';
COMMENT ON COLUMN camera_session.driving_session_id IS '연동된 주행 녹화 세션 id';
COMMENT ON COLUMN camera_session.camera_name IS '카메라 이름';
COMMENT ON COLUMN camera_session.topic_name IS '카메라 ROS 토픽 이름';
COMMENT ON COLUMN camera_session.resolution_width IS '가로 해상도';
COMMENT ON COLUMN camera_session.resolution_height IS '세로 해상도';
COMMENT ON COLUMN camera_session.target_fps IS '목표 FPS';
COMMENT ON COLUMN camera_session.encoder_name IS '인코더 이름';
COMMENT ON COLUMN camera_session.stream_status IS '스트림 상태';
COMMENT ON COLUMN camera_session.created_at IS '카메라 세션 생성 시간';

CREATE INDEX idx_camera_session_session_created_at
ON camera_session (session_id, created_at DESC);

CREATE TABLE camera_recording (
    id                INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id        INTEGER NOT NULL,
    camera_session_id INTEGER NOT NULL,
    duration_sec      INTEGER NOT NULL DEFAULT 0 CHECK (duration_sec >= 0),
    resolution_width  INTEGER CHECK (resolution_width IS NULL OR resolution_width > 0),
    resolution_height INTEGER CHECK (resolution_height IS NULL OR resolution_height > 0),
    fps               REAL CHECK (fps IS NULL OR fps > 0),
    codec             VARCHAR(32),
    recorded_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

COMMENT ON TABLE camera_recording IS '카메라 녹화 메타데이터';
COMMENT ON COLUMN camera_recording.id IS '녹화 PK, 자동 증가';
COMMENT ON COLUMN camera_recording.session_id IS '연관 세션 id';
COMMENT ON COLUMN camera_recording.camera_session_id IS '연관 카메라 세션 id';
COMMENT ON COLUMN camera_recording.duration_sec IS '녹화 길이 초';
COMMENT ON COLUMN camera_recording.resolution_width IS '녹화 가로 해상도';
COMMENT ON COLUMN camera_recording.resolution_height IS '녹화 세로 해상도';
COMMENT ON COLUMN camera_recording.fps IS '녹화 FPS';
COMMENT ON COLUMN camera_recording.codec IS '코덱';
COMMENT ON COLUMN camera_recording.recorded_at IS '녹화 시작 시간';

CREATE INDEX idx_camera_recording_session_recorded_at
ON camera_recording (session_id, recorded_at DESC);

CREATE TABLE object_detection (
    id                INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id        INTEGER NOT NULL,
    camera_session_id INTEGER NOT NULL,
    recording_id      INTEGER,
    detected_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    label             VARCHAR(64) NOT NULL,
    confidence        REAL NOT NULL CHECK (confidence BETWEEN 0 AND 1),
    bbox_x            INTEGER NOT NULL CHECK (bbox_x >= 0),
    bbox_y            INTEGER NOT NULL CHECK (bbox_y >= 0),
    bbox_width        INTEGER NOT NULL CHECK (bbox_width > 0),
    bbox_height       INTEGER NOT NULL CHECK (bbox_height > 0),
    tracking_id       VARCHAR(64)
);

COMMENT ON TABLE object_detection IS '객체 인식 결과';
COMMENT ON COLUMN object_detection.id IS '객체 인식 결과 PK, 자동 증가';
COMMENT ON COLUMN object_detection.session_id IS '연관 세션 id';
COMMENT ON COLUMN object_detection.camera_session_id IS '연관 카메라 세션 id';
COMMENT ON COLUMN object_detection.recording_id IS '녹화 영상 기준 인식이면 camera_recording id';
COMMENT ON COLUMN object_detection.detected_at IS '객체 인식 시간';
COMMENT ON COLUMN object_detection.label IS '인식 객체 라벨';
COMMENT ON COLUMN object_detection.confidence IS '인식 신뢰도';
COMMENT ON COLUMN object_detection.bbox_x IS '바운딩 박스 x';
COMMENT ON COLUMN object_detection.bbox_y IS '바운딩 박스 y';
COMMENT ON COLUMN object_detection.bbox_width IS '바운딩 박스 너비';
COMMENT ON COLUMN object_detection.bbox_height IS '바운딩 박스 높이';
COMMENT ON COLUMN object_detection.tracking_id IS '추적 id';

CREATE INDEX idx_object_detection_session_detected_at
ON object_detection (session_id, detected_at DESC);

-- =========================================================
-- 7. Files / Playback
-- =========================================================

CREATE TABLE files (
    id                INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    entity_type       VARCHAR(50) NOT NULL,
    entity_id         VARCHAR(100) NOT NULL,
    file_category     VARCHAR(50),
    storage_backend   VARCHAR(20) NOT NULL DEFAULT 'local' CHECK (
        storage_backend IN ('local', 's3')
    ),
    storage_path      TEXT NOT NULL,
    storage_bucket    VARCHAR(100),
    original_filename TEXT NOT NULL,
    mime_type         VARCHAR(100) NOT NULL,
    checksum          VARCHAR(64) UNIQUE,
    uploaded_by       VARCHAR(100),
    display_order     INTEGER DEFAULT 0,
    is_public         BOOLEAN DEFAULT FALSE,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at        TIMESTAMPTZ,
    deleted_at        TIMESTAMPTZ
);

COMMENT ON TABLE files IS '통합 파일 관리 테이블';
COMMENT ON COLUMN files.id IS '파일 PK, 자동 증가';
COMMENT ON COLUMN files.entity_type IS '파일이 연결되는 대상 종류';
COMMENT ON COLUMN files.entity_id IS '연결 대상 id. 폴리모픽 연결이라 문자열 사용';
COMMENT ON COLUMN files.file_category IS '파일 분류';
COMMENT ON COLUMN files.storage_backend IS '저장소 종류';
COMMENT ON COLUMN files.storage_path IS '파일 저장 경로';
COMMENT ON COLUMN files.storage_bucket IS 'S3 버킷명';
COMMENT ON COLUMN files.original_filename IS '원본 파일명';
COMMENT ON COLUMN files.mime_type IS 'MIME 타입';
COMMENT ON COLUMN files.checksum IS '파일 체크섬';
COMMENT ON COLUMN files.uploaded_by IS '업로드 사용자';
COMMENT ON COLUMN files.display_order IS '표시 순서';
COMMENT ON COLUMN files.is_public IS '공개 여부';
COMMENT ON COLUMN files.created_at IS '파일 레코드 생성 시간';
COMMENT ON COLUMN files.updated_at IS '파일 레코드 수정 시간';
COMMENT ON COLUMN files.deleted_at IS 'soft delete 시간';

CREATE INDEX idx_files_entity
ON files (entity_type, entity_id);

CREATE INDEX idx_files_active
ON files (deleted_at)
WHERE deleted_at IS NULL;

CREATE TABLE driving_session (
    id              INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    robot_id        INTEGER NOT NULL,
    session_id      INTEGER NOT NULL,
    waypoint_set_id INTEGER,
    started_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at        TIMESTAMPTZ,
    CHECK (ended_at IS NULL OR ended_at >= started_at)
);

COMMENT ON TABLE driving_session IS '주행 녹화 세션';
COMMENT ON COLUMN driving_session.id IS '주행 녹화 세션 PK, 자동 증가';
COMMENT ON COLUMN driving_session.robot_id IS '대상 로봇 id';
COMMENT ON COLUMN driving_session.session_id IS '연관 robot_session id';
COMMENT ON COLUMN driving_session.waypoint_set_id IS '실행한 waypoint 세트 id';
COMMENT ON COLUMN driving_session.started_at IS '주행 녹화 시작 시간';
COMMENT ON COLUMN driving_session.ended_at IS '주행 녹화 종료 시간';

CREATE INDEX idx_driving_session_robot_started_at
ON driving_session (robot_id, started_at DESC);

CREATE TABLE driving_record (
    id                 INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    driving_session_id INTEGER NOT NULL,
    recorded_at        TIMESTAMPTZ NOT NULL,
    pos_x              DOUBLE PRECISION,
    pos_y              DOUBLE PRECISION,
    yaw                DOUBLE PRECISION,
    linear_vel         DOUBLE PRECISION,
    angular_vel        DOUBLE PRECISION,
    battery_percent    DOUBLE PRECISION
);

COMMENT ON TABLE driving_record IS '주행 경로 재생용 기록 포인트';
COMMENT ON COLUMN driving_record.id IS '주행 기록 PK, 자동 증가';
COMMENT ON COLUMN driving_record.driving_session_id IS '소속 주행 녹화 세션 id';
COMMENT ON COLUMN driving_record.recorded_at IS '기록 시간';
COMMENT ON COLUMN driving_record.pos_x IS '로봇 x 좌표';
COMMENT ON COLUMN driving_record.pos_y IS '로봇 y 좌표';
COMMENT ON COLUMN driving_record.yaw IS '로봇 방향';
COMMENT ON COLUMN driving_record.linear_vel IS '선속도';
COMMENT ON COLUMN driving_record.angular_vel IS '각속도';
COMMENT ON COLUMN driving_record.battery_percent IS '배터리 잔량';

CREATE INDEX idx_driving_record_session_time
ON driving_record (driving_session_id, recorded_at);

COMMIT;
