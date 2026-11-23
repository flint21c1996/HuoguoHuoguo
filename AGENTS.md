# 훠궈훠궈 (HuoguoHuoguo) - AI 작업 규칙

이 파일은 이 저장소에서 일하는 모든 AI 에이전트(Claude, Codex 등)가 따르는 공통 규칙이다.
Claude는 `CLAUDE.md`가 이 파일을 불러오고, Codex는 이 파일을 직접 읽는다.

## 1. 작업 시작 전에 반드시 읽는 순서

1. 이 파일 (`AGENTS.md`)
2. `Docs/INDEX.md` : 모든 구현 문서의 제목/소제목 색인. 프로젝트 전체를 한눈에 본다.
3. `Docs/Project/Overview.md` : 게임 개요, 현재 플레이 흐름, 설계 원칙, 폴더 지도. 기획에 닿는 작업이면 `Docs/Project/Design.md`도 읽는다.
4. 이번 작업이 닿는 폴더의 문서 (예: 행성 작업이면 `Docs/Source/Planet.md`).
5. 실제 코드. 문서는 안내일 뿐이고 코드가 기준이다. 문서와 코드가 다르면 코드를 믿고, 작업 끝에 문서를 고친다.

질문에 답하기만 하는 등 코드를 바꾸지 않는 짧은 요청은 필요한 문서만 읽어도 된다.

## 2. 문서 규칙 (Docs/)

### 구조
- `Docs/INDEX.md` : 자동 생성 색인. 손으로 고치지 않는다.
- `Docs/Project/` : 프로젝트 전체 (`Overview.md`, `Config.md`, 기획서 `Design.md`, 기획 참고 `GameReferences.md`)
  - 기획 방향이나 사용자 결정이 바뀌면 `Design.md`를 먼저 고친다. 사용자가 정하지 않은 것을 결정된 것처럼 적지 않는다.
- `Docs/Source/<폴더>.md` : `Source/HuoguoHuoguo/<폴더>/` 하나당 문서 하나 (`Module.md`는 모듈 루트)
- `Docs/Content/<폴더>.md` : `Content/HG/<폴더>/` 하나당 문서 하나
- `Docs/Tools/Tools.md` : `Tools/` 스크립트

### 작업이 끝날 때
1. 바꾼 코드/에셋/스크립트가 속한 폴더의 문서를 갱신한다. 새 폴더를 만들었으면 문서도 새로 만들고 `Docs/Project/Overview.md`의 폴더 지도에 추가한다.
2. 플레이 흐름이나 설계 원칙이 바뀌었으면 `Docs/Project/Overview.md`도 고친다.
3. 고친 문서의 "마지막 갱신" 날짜를 바꾼다.
4. 색인을 다시 만든다:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Tools\Update-HGDocsIndex.ps1
   ```
5. 문서 변경은 코드 변경과 같은 커밋에 넣는다.

### 문서 모양
색인은 제목만 모으므로, 제목만 읽어도 무엇이 있는지 알 수 있게 쓴다.

```markdown
# <폴더 이름> - <한 줄 설명>

> 경로: `<실제 경로>` · 마지막 갱신: YYYY-MM-DD

## 역할
## 파일
### <클래스/파일 이름> (.h/.cpp)
## 흐름 / 규칙
## 알려진 문제와 다음 할 일
```

- 한국어, 담백하게, 이모지 없이. 클래스별로 "무엇을 하는지, 왜 그렇게 했는지, 주요 함수/속성, 다른 곳과의 연결"을 적는다.
- 코드를 길게 붙여 넣지 않는다. 함수 이름과 핵심 수치만 적는다.
- 확인된 사실과 아직 확인하지 않은 것(예: 사람이 직접 플레이해 보지 않음)을 구분해서 적는다.
- 제목(`#`, `##`, `###`)은 세 단계까지만 쓴다. 색인이 그 세 단계만 모은다.

## 3. 설계 원칙

- 게임 로직, 규칙, 상태는 C++에 둔다. 블루프린트는 C++ 클래스를 상속해 비주얼만 연결한다.
- 밸런스 데이터는 CSV/JSON 텍스트로 두고 DataTable로 읽는다.
- 손으로 다듬는 장소(정거장)는 맵 에셋 배치가 기준이다. 매번 달라지는 장소(행성)는 시드로 만든다.
- `.uasset`/`.umap`은 사용자가 요청하지 않으면 직접 수정하지 않는다. 에셋 생성이 필요하면 `Tools/`의 에디터 Python 스크립트로 만들고, 이미 있는 에셋은 덮어쓰지 않는다.
- 경영 파트(`Shop/`)와 탐험 파트(`Explore/`)는 완전히 분리한다. 한쪽 파트 코드가 다른 쪽을 include하지 않는다. 공통은 `Game/`, `Player/`에, 파트 사이 정보는 GameInstance에만 둔다.
- 경영은 가짜 아이소메트릭(원근 + 좁은 FOV + 먼 거리, 카메라 회전은 Z축 좌우만), 탐험은 3인칭 원근. 진짜 직교 투영은 쓰지 않는다.

## 4. 코드 스타일

- 클래스 접두사 `HG`. 에셋 접두사 `L_`, `BP_`, `M_`, `MI_`, `IA_`, `IMC_`, `DA_`, `NS_`.
- 주석은 한국어로, 담백하게, 이모지 없이. 코드가 하는 일과 그 이유(원리)를 적는다.
- 새 C++ 파일은 UTF-8 BOM으로 저장한다 (MSVC가 한국어 주석을 올바르게 읽도록). 한국어 문자열이 든 `.ps1`도 BOM으로 저장한다 (Windows PowerShell 5.1).
- 생성된 언리얼 파일(`Intermediate/`, `*.generated.h`)은 고치지 않는다.

## 5. 빌드와 확인

- 빌드: `powershell -ExecutionPolicy Bypass -File .\Tools\Invoke-HGBuild.ps1`
  에디터가 켜져 있고 Live Coding이 활성이면 실패한다. 사용자에게 에디터를 닫아 달라고 하거나 에디터에서 컴파일한다.
- 맵 생성: `Tools/Invoke-HGMapGen.ps1` (에디터를 닫고 실행).
- 실행 확인은 `-game`으로 띄우고 `-ExecCmds`의 개발 명령(`HGDive`, `HGSimKey` 등, `Docs/Source/Game.md`)과 `LogHuoguo` 로그로 한다.
- 화면 확인은 게임 창만 캡처한다 (PrintWindow). OS 수준의 화면 전체 캡처나 키/마우스 입력 주입은 하지 않는다.
- 무엇을 확인했고 무엇을 확인하지 못했는지 보고한다. 증거 없이 성공이라고 말하지 않는다.

## 6. Git

- git은 PATH에 없다. `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe`를 쓴다.
- 작업 전에 `git status`로 사용자가 만든 변경을 확인하고 건드리지 않는다.
- 스테이징, 커밋, 푸시는 사용자 승인 후에만 한다. 커밋 전에 포함될 파일 목록을 보여 준다.
- 비밀값(웹훅 URL, 토큰, 자격 증명)을 추적되는 파일에 넣지 않는다.
