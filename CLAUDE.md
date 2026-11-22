# 훠궈훠궈 (HuoguoHuoguo)

우주 정거장 훠궈집 경영 + 행성 파밍 게임. Unreal Engine 5.8, C++ 중심.
원형 웹 프로토타입: https://claude.ai/artifact/Rzyxuvad28xVVd2dydoxoE

## 핵심 설계 원칙

- 게임 로직, 규칙, 상태는 C++에 둔다. 블루프린트는 C++ 클래스를 상속해 비주얼만 연결한다.
- 밸런스 데이터(재료, 종족, 업그레이드, 파밍지)는 CSV/JSON 텍스트로 두고 DataTable로 읽는다.
- 맵 배치는 맵 에셋(`.umap`)이 기준이다. 런타임에 코드로 지형을 만들지 않는다.
- `.uasset`/`.umap`은 사용자가 요청하지 않으면 직접 수정하지 않는다. 에셋 생성이 필요하면 `Tools/`의 에디터 Python 스크립트로 만들고, 이미 있는 에셋은 덮어쓰지 않는다.
- 경영 파트: 가짜 아이소메트릭(원근 카메라 + 좁은 FOV + 먼 거리). 진짜 직교 투영은 쓰지 않는다.
- 파밍 파트: 3인칭 원근 카메라.
- 두 모드는 서로 다른 Pawn이며 `AHGPlayerController`가 Possess와 입력 매핑 컨텍스트를 바꾼다.

## 폴더 구조 (메인 프로젝트 UnderOneUmbrella 규칙을 따른다)

소스 (`Source/HuoguoHuoguo/`), 클래스 접두사 `HG`:
- `Game/` 게임 모드, 플레이어 컨트롤러, GameInstance (모드 전환, 입력, 맵 이동)
- `Player/` 플레이어가 조종하는 Pawn (경영 카메라 `AHGShopCameraPawn`, 탐험 캐릭터 `AHGExplorerCharacter`)
- `World/` 맵에 배치하는 액터 (이동 발판, 경영 카메라 기준점, 사장님 시작점)
- `Debug/` 개발용 도우미 (자리 표시 도형 색칠 등)
- `Tests/` 자동화 테스트 (추가 예정)

콘텐츠 (`Content/HG/`), 에셋 접두사는 메인 프로젝트와 같다 (`L_` 맵, `BP_`, `M_`, `MI_`, `IA_`, `IMC_`, `DA_`, `NS_`):
- `Maps/` `L_Station`(허브 정거장 훠궈집), `L_Asteroid_Krak7`(첫 파밍지). 개발용 맵은 `Maps/Development/`
- `Materials/Greybox/` 블록아웃용 단색 `MI_GB_*`
- 앞으로: `Blueprints/`, `Input/`, `DataTable/`, `UI/`, `Audio/`, `Effects/`

## 맵 구성 규칙

- 정거장 맵에는 `HGShopCameraAnchor` 1개(경영 카메라 초점, 화살표 = 카메라 방향)를 둔다. 이게 없는 맵은 경영 화면이 없는 장소(파밍지)로 처리된다.
- `HGExplorerStart`(사장님 시작점)는 `StartTag`로 구분한다. 기본 시작은 `Default`, 발판으로 도착하면 발판의 `ArrivalTag`와 같은 태그.
- `HGTravelPad`는 `DestinationLevel`(맵) + `ArrivalTag`(도착 시작점)를 가진다. 맵 사이에 넘기는 정보는 `UHGGameInstance`에 둔다.
- 블록아웃 맵 처음 생성: `Tools/Invoke-HGMapGen.ps1` (에디터를 닫고 실행, 이미 있는 맵은 건너뜀, `-Force`면 지우고 다시 만듦)

## 빌드

에디터가 켜져 있고 Live Coding이 활성화되어 있으면 명령줄 빌드가 실패한다. 빌드 전에 에디터를 닫거나 에디터에서 Live Coding으로 컴파일한다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Invoke-HGBuild.ps1
```

## 실행과 확인

- 기본 맵(게임, 에디터 시작)은 `/Game/HG/Maps/L_Station`이다.
- 에디터 없이 실행: `UnrealEditor.exe "E:\HuoguoHuoguo\HuoguoHuoguo.uproject" -game -windowed -ResX=1280 -ResY=720 -log`
- 개발용 콘솔 명령 (`~` 콘솔 또는 `-ExecCmds="A,B"`):
  - `HGMode shop|explore` : 모드 전환
  - `HGTravel <맵 이름 일부>` : 그 맵으로 가는 발판을 밟은 것처럼 이동 (예: `HGTravel Asteroid`)
  - `HGSimKey <키> <초>` : 엔진 입력 처리에 키 누름을 넣고, 뗄 때 이동 결과를 로그로 남긴다 (예: `HGSimKey W 1.5`)
- 화면 확인은 게임 창만 캡처한다 (PrintWindow). OS 수준의 화면 전체 캡처나 키 입력 주입은 하지 않는다.

## 코드 스타일

- 주석은 한국어로, 담백하게. 이모지 금지. 코드가 하는 일과 그 이유(원리)를 적는다.
- 새 C++ 파일은 UTF-8 BOM으로 저장한다 (MSVC가 한국어 주석을 올바르게 읽도록).

## Git

- git은 PATH에 없다. `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe` 사용.
- 커밋과 푸시는 사용자 승인 후에만 한다.
