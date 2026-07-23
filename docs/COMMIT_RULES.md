# Commit & Branch Rules

> 원본: `docs/AURIX_MediRun_06_개발_규칙서.pdf` 2장(Git 작업 규칙). 이 문서는 원본 표를 옮기고, 이를 강제하는 Git 훅 사용법을 추가한다. 규칙 자체를 바꾸려면 원본 PDF의 6.3절 변경 절차를 따른다.
>
> **PDF 원본과의 차이 1건**: PDF 1.2절은 "커밋 제목에 한글을 쓰지 않는다"고 규정하지만, 팀 결정으로 커밋 `subject`(설명 부분)는 한글로 쓰기로 했다. `type`/`scope`는 PDF 원본대로 영문을 유지한다.

## 1. 브랜치 전략 [확정]

| 브랜치 | 머지 방향 | 용도 |
|---|---|---|
| `main` | ← `develop` | 동작 확인된 소스만. 주차 종결 시점에 태그를 붙인다 |
| `develop` | ← `feature/*` 등 | 통합 브랜치. 평소 작업은 여기로 모인다 |
| `feature/<기능명>` | → `develop` | 기능 개발 (`feature/aruco-docking`) |
| `fix/<내용>` | → `develop` | 버그 수정 (`fix/nav2-goal-timeout`) |
| `docs/<문서명>` | → `develop` | 문서 작업 (`docs/interface-spec`) |
| `chore/<내용>` | → `develop` | 빌드·설정 변경 (`chore/gitignore-update`) |

**절대 규칙: `main`에 직접 커밋하지 않는다.** 항상 `feature/`, `fix/`, `docs/`, `chore/` 중 하나로 브랜치를 만들고, `develop`을 거쳐 PR로만 `main`에 반영한다.

브랜치명 형식: 소문자 + 하이픈 (`feature/aruco-docking`). 브랜치명 자체에는 한글을 쓰지 않는다(PDF 1.2절, 유지).

## 2. 커밋 메시지 [확정]

형식:

```
<type>(<scope>): <subject>
```

- `type`, `scope`는 영문으로 쓴다.
- `scope`는 선택이며 `nav`, `vision`, `sensor`, `gui`, `db` 중 하나만 쓴다.
- 시험 기록 커밋은 `scope`에 시험 ID(`T-` + 두 자리, 예: `T-07`)를 넣는다.
- `subject`는 **한글로**, 명령형·서술형으로 간결하게 쓰고 마침표를 붙이지 않는다.

| type | 설명 | 예시 |
|---|---|---|
| `feat` | 새 기능 추가 | `feat(nav): 장애물 회피 로직 추가` |
| `fix` | 버그 수정 | `fix(sensor): 홀센서 오배출 감지 오류 수정` |
| `docs` | 문서 변경 | `docs: 시스템 요구사항 문서 추가` |
| `test` | 시험 추가·기록 | `test(T-07): 저단 장애물 회피 결과 기록` |
| `refactor` | 동작 변화 없는 구조 변경 | `refactor: 도킹 매니저를 미션 매니저에서 분리` |
| `chore` | 빌드·설정·의존성 | `chore: rosbag git 제외 처리` |

## 3. PR 및 머지 [확정]

| 구간 | 셀프 머지 | 조건 |
|---|---|---|
| `feature/*` → `develop` | 허용 | PR 생성 필수, 빌드 성공 확인, PR 본문에 작업 내용과 확인 방법 기재 |
| `develop` → `main` | 허용 | PR 생성 필수, 해당 주차 완료 기준 충족, 머지 직후 동결 태그 부착 |

- 2인 팀 기준: 리뷰 승인을 머지 조건으로 두지 않는다. 대신 모든 머지가 PR을 거치게 해 변경 이력이 남도록 하고, 실행 확인을 승인 대신 조건으로 삼는다.
- 하나의 PR은 하나의 기능 또는 하나의 수정만 담는다.
- 되돌리기: 머지 후 동작이 깨지면 추가 수정보다 revert를 우선한다. `main`이 항상 실행 가능한 상태를 유지하는 것이 최우선이다.

## 4. 주차별 동결과 태그 [확정]

| 시점 | 동결 대상 | 태그 |
|---|---|---|
| 1주차 말 | 웨이포인트 이름, 토픽 이름 | `v0.1-week1` |
| 2주차 말 | 상태명, 메시지 필드 | `v0.2-week2` |
| 3주차 말 | GUI 화면 구성, DB 필드 | `v0.3-week3` |
| 4주차 말 | 전체 (제출본) | `v1.0` |

동결 이후 변경 시 PDF 6.3절 변경 절차를 따르고, 영향 받는 노드와 시험 ID를 커밋 메시지에 함께 기록한다.

## 5. 강제(Git Hooks) 적용 방법

이 저장소에는 위 규칙을 강제하는 훅이 `.githooks/`에 들어 있다. 훅은 저장소 기본 `.git/hooks/`가 아니라 버전 관리되는 `.githooks/`에 있으므로, **클론 후 한 번** 아래 명령으로 활성화해야 한다.

```bash
./scripts/setup-git-hooks.sh
# 또는 직접
git config core.hooksPath .githooks
```

활성화하면:

- **`commit-msg`**: 커밋 메시지가 `<type>(<scope>): <subject>` 형식과 위 규칙(type 목록, scope 목록, subject 한글·마침표 금지)을 지키지 않으면 커밋을 거부한다.
- **`pre-commit`**: 현재 브랜치가 `main`이면 커밋을 거부하고, `feature/fix/docs/chore` 브랜치를 만들라는 안내를 출력한다. 브랜치명이 `feature/`, `fix/`, `docs/`, `chore/` 접두어 형식(소문자+하이픈)을 벗어나면 경고한다.

훅 스크립트를 수정해야 하면 `.githooks/commit-msg`, `.githooks/pre-commit`을 고치고, 변경 사유를 PDF 6.4절 문서 변경 이력에 남긴다.
