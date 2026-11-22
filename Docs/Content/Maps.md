# Maps - 맵 에셋

> 경로: `Content/HG/Maps/` · 마지막 갱신: 2026-11-23

## 역할

손으로 다듬는 장소의 배치. 처음 배치는 `Tools/GenerateGreyboxMaps.py`가 만들고, 이후 수정은 에디터에서 맵을 직접 고치는 것이 기준이다.

## 맵

### L_Station - 허브 정거장 훠궈집
- 게임과 에디터의 기본 맵. 경영 파트 (World Settings GameMode = `HGShopGameMode`). 12m x 10m 가게, 바닥 윗면 Z = 0, 가게 중심 = 원점.
- 아웃라이너 폴더: `Lighting`, `Gameplay`, `Shop/Structure`, `Shop/Kitchen`, `Shop/Hall`, `Shop/Props`
- 카메라 쪽 두 벽(서, 남)은 발목 높이, 먼 쪽 두 벽(동, 북)은 높게 (기본 카메라 45도 기준).
- 주방(카운터, 화구 2개, 마라탕/백탕), 홀(테이블 4개), 재료 진열대, 도착 전광판, 에어록 문.
- 게임플레이: `ShopCameraAnchor`(원점, Yaw 45 = 카메라 처음 방향), `ExplorerStart_Default`(-100, 220, 사장님 시작), `ExplorerStart_FromPlanet`(-450, 250, 행성에서 귀환), `TravelPad_Airlock`(-450, 400, 60, 다이빙 발판 → `L_PlanetSurface`).
- 조명: DirectionalLight, SkyAtmosphere, SkyLight(실시간 캡처).

### L_PlanetSurface - 절차적 행성 표면
- 탐험 파트 (World Settings GameMode = `HGExploreGameMode`).
- 배치는 조명과 생성기뿐. 지형, 바위, 재료, 귀환 발판은 실행할 때 `AHGPlanetGenerator`가 만든다.
- `Sun`(방향광 10, ForwardShadingPriority 1), `FillLight`(푸른 보조광 1.2, 그림자 없음), `PostProcess_Space`(무한 범위, 노출 EV100 2.5 고정. 검은 하늘 때문에 자동 노출을 쓰지 않는다).
- `PlanetGenerator`(원점, 귀환 → `L_Station`/`FromPlanet`), 배경 `GasPlanet` 구체.
- 하늘 대기 없음 (우주).

### L_Asteroid_Krak7 - 이전 손배치 소행성 (연결 끊김)
- 탐험 파트 (GameMode = `HGExploreGameMode`). 지름 40m 평평한 원판과 바위 26개, 재료 표식 9개. 절차적 행성으로 대체되어 지금은 어느 발판도 이리로 보내지 않는다.
- 이 맵의 귀환 발판은 `FromAsteroid` 태그로 보내는데 정거장에 그 시작점이 없어서 `Default`로 대체된다. 지울지 결정이 필요하다.

## 흐름 / 규칙

- 맵마다 World Settings의 GameMode Override로 파트(경영/탐험)를 정한다. 지정이 없으면 프로젝트 기본값 `HGExploreGameMode`.
- 맵 사이 연결은 `HGTravelPad`의 목적지 + 도착 태그로만 한다 (`Docs/Source/World.md`).
- 개발용 맵은 `Maps/Development/`에 둔다 (아직 없음).

## 알려진 문제와 다음 할 일

- 정거장 하늘이 파란 대기다. 우주 배경으로 바꿔야 한다.
- 정거장 창밖으로 접근 행성을 보여 주는 배치가 없다.
