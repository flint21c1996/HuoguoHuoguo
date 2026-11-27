# Maps - 맵 에셋

> 경로: `Content/HG/Maps/` (`Legacy/` = 안 쓰는 옛 맵, `Development/` = 개발용 맵) · 마지막 갱신: 2026-11-27

## 역할

손으로 다듬는 장소의 배치. 맵은 에디터에서 사람이 직접 만들고 고친다. 맵 생성 도구는 이미 있는 맵을 열지도 고치지도 않는다 (2026-11-27부터).

## 환경별 맵 (2026-11-27, 사용자가 에디터에서 `L_PlanetSurface`를 복제해 만듦)

어떤 환경인지, 어떤 규칙을 가지는지에 따라 맵을 나눈다. 절차적 생성을 해도 맵마다 규칙(크기, 중력, 조명, 하늘, 나오는 것)이 다르기 때문이다.
다이빙하면 고른 행성의 환경 종류에 맞는 맵이 열린다 (프로젝트 설정 → Game → HG 환경 맵, `Docs/Source/World.md`).

| 맵 | 환경 | 상태 |
|---|---|---|
| `L_Planet_Small` | 소형 행성 (지평선이 휘어 보임) | 복제 직후. 규칙과 분위기는 아직 원본과 같음 |
| `L_Planet_Medium` | 중형 행성 (걸을 때 평지처럼 느껴짐) | 위와 같음 |
| `L_AsteroidField` | 소행성 지대 (여러 천체가 끌어당김, 뛰어 건너기) | 위와 같음 |
| `L_Derelict` | 폐우주선 (밖은 무중력, 에어록 안은 인공 중력) | 위와 같음 |
| `L_Planet_Large` | 평지 행성 (대형): 구가 아니라 랜드스케이프 분지. 중력은 아래로, 가장자리 밖으로 떨어지면 착륙 지점으로 복귀 | 2026-11-27 새로 만듦 (아래) |

- 확인됨: 정거장에서 다이빙하면 소행성 지대 → `L_AsteroidField`, 소형 → `L_Planet_Small`, 중형 → `L_Planet_Medium`, 폐우주선 → `L_Derelict`가 열린다.
- 주의: 처음 콘텐츠 브라우저에서 복제한 4개는 액터가 0개인 빈 맵이었다 (생성기도 조명도 없어 행성이 안 생김).
  커맨드렛으로 `Legacy/L_PlanetSurface`를 같은 이름으로 다시 복사해 고쳤다 (액터 5개). 소형 행성에서 생성까지 확인 (109ms).
  맵을 새로 복제한 뒤에는 아웃라이너에 액터가 들어 있는지 확인한다.
- 네 맵 모두 안은 같다: 조명, 생성기(`AHGPlanetGenerator`, 고른 행성 값으로 지형을 만듦), 게임 모드 `BP_HG_ExploreGameMode`.
- 중력 규칙(사용자 의견): 점프 손맛은 모든 행성에서 거의 같게. 큰 행성이라고 점프가 낮아지면 재미가 없다. 크기 차이는 지평선, 탐험 범위로 보인다.

## 맵

### L_Planet_Large - 평지 행성 (대형, 랜드스케이프 분지) (2026-11-27)
- 구 행성은 크게 만들면 다 돌아다닐 수 없고 연산도 늘어서(사용자 판단), 대형은 구 대신 평평한 분지로 따로 만들었다. 구 행성 맵은 그대로다.
- `L_Planet_Small`을 복사해 만들고, Planet Maker 평지 탭의 **랜드스케이프에 넣기**(기본 분지 값, 시드 22)로 땅을 넣었다.
  - `BasinLandscape`: 252m x 252m (1m 격자, 63칸 부품 4 x 4), 높이 -34 ~ 28m, 머티리얼 `M_HG_LandscapeBasin`, Use Dynamic Material Instance 켬(군계 색). 맵 파일 약 0.6MB.
- 정거장에서 다이빙 확인: 접근 목록 2번 "마라탄-84 | 평지 행성 (버섯숲)" → `L_Planet_Large`, 생성 30ms, 보라 땅 + 거대 버섯, 지평선에 둑.
  - World Settings `KillZ` = -54m (땅에서 가장 낮은 곳 - 20m). 그 아래로 떨어지면 착륙 지점으로 돌아온다.
  - `PlanetGenerator`: `bFlatSite`(평지 행성 맵) 켬, `FlatHalfExtent` 103m(둑 꼭대기 안쪽). 구 지형을 만들지 않고 랜드스케이프 위에 바위, 재료, 식물, 귀환 발판을 놓는다.
- 땅은 원하는 곳만 랜드스케이프 도구로 손보면 된다. 다시 **랜드스케이프에 넣기**를 누르면 손본 것도 지워지고 새로 만들어진다 (먼저 묻는다, Ctrl+Z 가능).
- 확인 (게임 캡처, 맵 바로 열기): 생성 38ms, 바위 20 / 재료 24(지금 보이는 것 11), 풀꽃 5032포기, 아치, 귀환 발판. 140m 밖으로 옮기자 3초 뒤 착륙 지점으로 복귀.
  위에서 내려다보면 둑과 그 너머 낭떠러지가 보인다.

### L_Station - 허브 정거장 훠궈집
- 게임과 에디터의 기본 맵. 경영 파트 (World Settings GameMode = `BP_HG_ShopGameMode`). 12m x 10m 가게, 바닥 윗면 Z = 0, 가게 중심 = 원점.
- 아웃라이너 폴더: `Lighting`, `Gameplay`, `Shop/Structure`, `Shop/Kitchen`, `Shop/Hall`, `Shop/Props`
- 카메라 쪽 두 벽(서, 남)은 발목 높이, 먼 쪽 두 벽(동, 북)은 높게 (기본 카메라 45도 기준).
- 주방(카운터, 화구 2개, 마라탕/백탕), 재료 진열대, 도착 전광판, 에어록 문.
- 홀(`Shop/Hall`): 가구 액터 `Table_0~3`(`HGTable`, (-300,-250) (100,-250) (-300,50) (100,50))과 `Chair_0~3`(`HGChair`, 각 식탁 남쪽 105, Yaw 90). 경영 파트에서 옮기거나 치울 수 있는 처음 배치다. 예전 도형 묶음(Table_N_Top/Leg/Pot/Seat)은 `upgrade_station()`이 이것으로 바꿨다.
- 게임플레이: `ShopCameraAnchor`(원점, Yaw 45 = 카메라 처음 방향), `ExplorerStart_Default`(-100, 220, 사장님 시작), `ExplorerStart_FromPlanet`(-450, 250, 행성에서 귀환), `TravelPad_Airlock`(-450, 400, 60, 다이빙 발판 → `L_PlanetSurface`).
- 조명: DirectionalLight, SkyAtmosphere, SkyLight(실시간 캡처).
- `PostProcess_Station`(무한 범위, 노출 EV100 2.0 고정). 자동 노출을 두면 멀리 줌아웃했을 때 화면 대부분이 어두운 우주가 되어 카메라가 밝기를 끌어올리고 가게가 하얗게 날아갔다.
  측정(FOV를 넓혀 멀리 보기를 흉내): 자동 노출일 때 가게 바닥 밝기가 가까이 175 → 멀리 254(거의 흰색), EV 2.0 고정 후 168 → 179. 가까이 본 모습은 자동 노출일 때와 거의 같다. 에디터에서 이 볼륨의 Exposure Min/Max Brightness를 고쳐 밝기를 바꾼다.

### Legacy/L_PlanetSurface - 예전의 절차적 행성 표면 (은퇴)
- 환경별 맵으로 나뉘기 전, 모든 행성을 받던 한 장짜리 맵. 2026-11-27에 `Legacy/`로 옮겼다.
- 지금은 정거장 다이빙 발판의 기본 목적지(`DestinationLevel`)로만 남아 있다. 환경 맵 설정에 없는 환경이 오면 이리로 온다 (지금은 모든 환경에 맵이 있다).
- 탐험 파트 (World Settings GameMode = `BP_HG_ExploreGameMode`).
- 배치는 조명과 생성기뿐. 지형, 바위, 재료, 귀환 발판은 실행할 때 `AHGPlanetGenerator`가 만든다.
- `Sun`(방향광 10, ForwardShadingPriority 1), `FillLight`(푸른 보조광 1.2, 그림자 없음), `PostProcess_Space`(무한 범위, 노출 EV100 2.5 고정. 검은 하늘 때문에 자동 노출을 쓰지 않는다).
- `PlanetGenerator`(원점, 귀환 → `L_Station`/`FromPlanet`), 배경 `GasPlanet` 구체.
- 하늘 대기 없음 (우주).

### Legacy/L_Asteroid_Krak7 - 이전 손배치 소행성 (연결 끊김)
- 탐험 파트 (GameMode = `BP_HG_ExploreGameMode`). 지름 40m 평평한 원판과 바위 26개, 재료 표식 9개. 절차적 행성으로 대체되어 지금은 어느 발판도 이리로 보내지 않는다.
- 이 맵의 귀환 발판은 `FromAsteroid` 태그로 보내는데 정거장에 그 시작점이 없어서 `Default`로 대체된다. 지울지 결정이 필요하다.

## 흐름 / 규칙

- 맵마다 World Settings의 GameMode Override로 파트(경영/탐험)를 정한다. 지정이 없으면 프로젝트 기본값 `BP_HG_ExploreGameMode`.
  게임 모드 → 컨트롤러 → 사장님이 모두 블루프린트로 이어진다 (`Docs/Content/Blueprints.md`).
- 맵 사이 연결은 `HGTravelPad`의 목적지 + 도착 태그로만 한다 (`Docs/Source/World.md`).
- 개발용 맵은 `Maps/Development/`에 둔다.

## 개발용 맵 (`Development/`)

### Development/L_PlanetMaker - Planet Maker 미리보기 맵 (2026-11-27)
- `Legacy/L_PlanetSurface`를 복사해 만들었다 (액터 5개: Sun, FillLight, PostProcess_Space, PlanetGenerator, GasPlanet).
- 생성기 설정: `bPreviewInEditor`(에디터에서 미리보기) 켬, `bPreviewFromSeedOnly` 끔, `bUsePreviewShape` 켬.
  맵을 열기만 해도 지형 미리보기가 생기고(약 50ms), 값을 바꾸면 다시 만든다. 미리보기 메시는 저장하지 않아 맵 파일은 약 28KB다.
- Planet Maker 창(`Docs/Tools/PlanetMaker.md`)이 이 맵의 생성기에 값을 넣는다. 게임 흐름(다이빙)은 이 맵으로 오지 않는다.

## 알려진 문제와 다음 할 일

- 정거장 하늘이 파란 대기다. 우주 배경으로 바꿔야 한다.
- 정거장 창밖으로 접근 행성을 보여 주는 배치가 없다.
