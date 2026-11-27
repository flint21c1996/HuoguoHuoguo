# 프로젝트 개요 - 우주 정거장 훠궈집 경영 + 절차적 행성 파밍

> 대상: 프로젝트 전체 · 마지막 갱신: 2026-11-24

## 한 줄 요약

우주 정거장에서 훠궈집을 경영하고(아이소메트릭 시점), 정거장 근처로 접근하는 절차적 행성에 다이빙해서 재료를 파밍한다(3인칭 시점). Unreal Engine 5.8, C++ 중심.
원형 웹 프로토타입: https://claude.ai/artifact/Rzyxuvad28xVVd2dydoxoE

## 현재 플레이 흐름 (구현됨)

경영 파트와 탐험 파트는 맵 단위로 완전히 나뉜다. 한 맵 안에서 두 파트를 오가는 전환은 없다.

1. **경영 파트** (`L_Station`, `Shop/`): 가짜 아이소 카메라가 사장님을 따라가고, 사장님은 WASD(카메라 기준)로 가게를 걷는다. 우클릭 드래그로 카메라 좌우 회전, Q/E 90도 회전, 휠 확대.
   B를 누르면 가구 배치 모드: 바닥에 칸 무늬가 깔리고, 가구는 크기만큼 칸을 차지한다(식탁 2x2, 소스 바 2x1, 의자/화분/등불 1x1). 가구 종류는 `Data/Furniture.csv`(엑셀로 고침)에 있고 지금 19종(냄비가 다른 식탁 6, 의자 5, 장식 8). 1/2/3으로 분류, Z/C로 가구를 고른다. 처음엔 3종만 열려 있고(돈 300), 나머지는 돈이나 행성 재료를 내고(U) 또는 손님이 많이 오면 열린다. 새로 놓으면 값을 내고, 치우면 절반을 돌려받는다. 가리킨 칸에 유령 가구와 차지할 칸(초록 = 놓을 수 있음, 빨강 = 없음)이 보이고, 클릭해서 놓고, 기존 가구를 집어 옮기거나 치운다. 배치는 GameInstance에 기록되어 행성에 다녀와도 유지된다.
   식탁 둘레에 장식을 붙이면 꼬리표(Plant, Light 등)에 따라 콤보가 생긴다(놓기 전에 미리보기). 낮과 밤에 손님이 들어와 진열대에서 꼬치를 골라 접시에 같은 방향으로 쌓고 식탁으로 간다. 사장님이 대기 중인 식탁 근처에서 F로 육수를 내면 손님이 하나씩 담가 먹고, 식탁/의자 보너스와 콤보를 더한 돈을 낸다.
2. 화면 왼쪽 위 "접근 중인 행성" 목록에서 숫자 키 1~4로 다이빙할 행성을 고른다. 목록은 `UHGGameInstance`가 궤도 주기(= 하루)마다 시드로 뽑는다.
   목록 첫 줄에 "N일째 낮/밤"이 보인다. 하루는 낮 → 밤 → 문 닫을 시간. T로 시간을 넘기고, 문 닫을 시간에 T를 누르면 다음 날이 되어 새 행성들이 온다.
3. 사장님이 에어록의 청록 다이빙 발판을 밟으면 화면이 어두워지며 `L_PlanetSurface`로 이동한다. 문 닫을 시간에는 다이빙할 수 없다.
4. **탐험 파트** (`L_PlanetSurface`, `Explore/`): `AHGPlanetGenerator`가 고른 행성의 시드로 구형 지형, 바위, 재료 표식, 귀환 발판을 만든다. 사장님은 북극에 착륙하고 행성 중심 방향 중력을 받으며 3인칭으로 걷는다.
   재료 표식(색 구) 옆에 가면 `[E] 채집` 안내가 뜨고, E로 캐서 가방(GameInstance)에 넣는다. 캔 자리는 그 행성이 떠날 때까지 비어 있다.
   낮에 내려오면 낮 재료, 밤에 내려오면 밤 재료만 보인다 (같은 행성이어도).
   오른쪽 위 원형 미니맵이 사장님 중심으로 행성 표면을 펼쳐 재료와 귀환 발판(범위 밖이면 테두리 화살표 + 거리)을 보여 준다. M으로 근거리/행성 전체.
5. 착륙 지점 옆 금색 귀환 발판을 밟으면 정거장의 `FromPlanet` 시작점(경영 파트)으로 돌아오고, 시간대가 한 칸 넘어가며(낮 → 밤, 밤 → 문 닫을 시간), 가방 내용이 메시지로 뜬다.

## 그림체

모양은 복셀, 가게 밤 분위기는 니발리스, 행성 색과 하늘은 노 맨즈 스카이에서 배운다 (사용자 결정). 가구, 손님, 사장님은 10cm 복셀이 기본이고, 밤에는 정거장이 어두워지고 냄비, 등불, 수조 불빛이 켜진다. 정거장 벽과 행성은 아직 매끈한 도형이다.

## 핵심 설계 원칙

- 게임 로직, 규칙, 상태는 C++에 둔다. 블루프린트는 C++ 클래스를 상속해 비주얼만 연결한다.
- 기획/아트가 바꿀 것(어떤 모델/머티리얼, 조절 수치, 어떤 클래스를 쓸지)은 C++에 경로나 상수로 박지 않는다.
  블루프린트 디테일 패널(`UPROPERTY(EditAnywhere/EditDefaultsOnly)`, `TSubclassOf`), 데이터 테이블, 프로젝트 설정에서 정한다.
  도구 스크립트는 처음 한 번 만들어 주는 용도이고, 사람이 고친 에셋을 덮어쓰지 않는다. 지금 블루프린트 목록: `Docs/Content/Blueprints.md`.
- 밸런스 데이터(재료, 콤보, 종족, 업그레이드, 파밍지)는 CSV/JSON 텍스트로 두고 DataTable로 읽는다. 지금은 재료(`Data/Ingredients.csv` → `DT_Ingredients`), 콤보(`Data/Combos.csv` → `DT_Combos`), 가구(`Data/Furniture.csv` → `DT_Furniture`)가 있다. 가구 생김새도 데이터(분류 + Style)로 정하고, 아트가 생기면 행의 블루프린트 클래스로 바꾼다.
- 손으로 다듬는 장소(정거장)는 맵 에셋(`.umap`) 배치가 기준이다. 런타임에 코드로 지형을 만들지 않는다.
- 매번 달라지는 장소(행성)는 맵이 아니라 시드(`FHGPlanetSeed`)로 존재하고, 표면 맵에서 생성기가 만든다.
- 경영 파트와 탐험 파트는 완전히 분리한다. 파트마다 GameMode, PlayerController, 사장님 캐릭터, 입력이 따로 있고, 맵의 World Settings가 파트를 정한다. 공통 부분은 `Game/`(부모 클래스, GameInstance)과 `Player/`(사장님 공통 몸체)에만 둔다.
- 경영 파트: 직교 투영 아이소메트릭 (2026-11-28, 사용자 결정. 예전에는 원근 카메라 + FOV 15도의 가짜 아이소). 카메라 회전은 Z축(좌우)만, Q/E 45도.
  투영은 카메라 컴포넌트의 Projection Mode로 고르고(경영 사장님 BP 기본 직교), 원근으로 돌려도 확대/축소가 맞게 동작한다.
- 탐험 파트: 3인칭 원근 카메라 (FOV 75도), 구형 중력.
- 두 파트를 잇는 정보(도착 태그, 고른 행성, 앞으로 인벤토리)는 GameInstance에만 둔다.

## 폴더 지도

폴더는 UnderOneUmbrella 프로젝트와 같은 방식으로 나눈다 (2026-11-27). Content는 게임 것(`HG/`)과 가져온 아트 원본(`ArtSource/`)을 나누고,
Source는 `Game / Player / UI / Shop/<기능> / World/<기능> / Debug`로 나눈다. 엔진 템플릿 마네킹은 `Content/Characters/Mannequins`(저장소 밖, 이 PC에만).

| 프로젝트 경로 | 내용 | 문서 |
|---|---|---|
| `Source/HuoguoHuoguo/` (루트) | 모듈 정의, 로그 카테고리, Build.cs | `Docs/Source/Module.md` |
| `Source/HuoguoHuoguo/Game/` | 게임 모드(공통 부모, 경영, 탐험), GameInstance(가방, 기록), 재료/하루 타입, 그림체 설정 | `Docs/Source/Game.md` |
| `Source/HuoguoHuoguo/Player/` | 사장님 캐릭터(공통, 경영, 탐험)와 플레이어 컨트롤러(공통, 경영, 탐험) | `Docs/Source/Player.md` (경영/탐험 쪽은 `Shop.md`, `Explore.md`) |
| `Source/HuoguoHuoguo/UI/` | 탐험 UI 위젯 (미니맵, 도감) | `Docs/Source/Explore.md` |
| `Source/HuoguoHuoguo/Shop/Core/`, `Customer/`, `Furniture/` | 경영 시스템: 칸/콤보, 손님 흐름, 가구와 배치 모드 | `Docs/Source/Shop.md` |
| `Source/HuoguoHuoguo/World/Planet/` | 행성 시드, 절차적 행성 생성기, 폐우주선 | `Docs/Source/Planet.md` |
| `Source/HuoguoHuoguo/World/Travel/`, `Camera/` | 맵에 배치하는 이동 발판, 가게 카메라 기준점 (시작점은 엔진 PlayerStart) | `Docs/Source/World.md` |
| `Source/HuoguoHuoguo/World/Light/`, `Scan/` | 정거장 조명, 스캐너 파동 | `Docs/Source/Shop.md`, `Docs/Source/Explore.md` |
| `Source/HuoguoHuoguo/Debug/` | 개발용 도우미 | `Docs/Source/Debug.md` |
| `Source/HuoguoHuoguoEditor/` | 에디터 전용 모듈 (게임에 안 들어감): Planet Maker 창 | `Docs/Tools/PlanetMaker.md`, `Docs/Source/Module.md` |
| `Content/HG/Maps/` (`Legacy/` = 안 쓰는 옛 맵, `Development/` = 개발용 맵) | 맵 에셋 | `Docs/Content/Maps.md` |
| `Content/HG/Planets/` | Planet Maker로 저장한 행성 프리셋 JSON = 게임의 고정 행성 (아직 없음, 처음 저장할 때 생김) | `Docs/Tools/PlanetMaker.md` |
| `Content/HG/BluePrint/Game/`, `Content/HG/Player/` | 게임 모드 BP / 사장님·컨트롤러 BP, 애니메이션 BP·블렌드 스페이스·리그 | `Docs/Content/Blueprints.md` |
| `Content/HG/Materials/`, `Content/HG/DebugMaterials/Greybox/` | 게임 머티리얼 / 회색 박스용 단색 머티리얼 | `Docs/Content/Materials.md` |
| `Content/ArtSource/` | 가져온 아트 원본: `Fbx/Characters/Player`(메시), `Fbx/Animation/Character`(애니메이션), `Material/`, `Texture/` | `Docs/Content/Blueprints.md` |
| `Data/`, `Content/HG/DataTable/` | 밸런스 데이터 CSV 원본과 DataTable 에셋 | `Docs/Content/Data.md` |
| `Tools/` | 빌드, 맵 생성, 문서 색인 스크립트 | `Docs/Tools/Tools.md` |
| `Config/`, `.uproject`, `.gitignore` | 엔진/프로젝트 설정 | `Docs/Project/Config.md` |

## 기획 방향 (논의 중)

경영 파트를 카이로소프트식 "판 짜기 + 손님 관찰"로 만든다. 하루는 낮 → 밤 → 문 닫을 시간이고, 언제 다이빙할지는 유저가 정한다(다이빙 한 번 = 시간대 한 칸). 낮에 다이빙하면 가게는 쉬고, 밤에 다이빙하면 직원이 대신 돌린다. 가게는 정거장 방을 통째로 붙여 키운다.
가게 파트의 중심은 카이로식 칸(그리드) 배치와 콤보다: 정해진 칸 안에만 가구를 놓고, 식탁 주변 배치에 따라 손님이 돈을 더 내며, 가게를 점점 키운다 (꼬치 진열은 하지 않는다).
고른 시스템: 궤도가 손님을 데려온다, 손님이 메뉴를 발명한다, 김으로 읽는 가게, 정거장 환경 구역, 손님이 주는 정보(새 행성/재료).
더 넣을 아이디어: 손님 도감, 단골 이야기, 행성 이벤트, 꼬치 막대 탑, 정거장 확장.
사용자가 말한 방향, 결정, 아이디어(행성 전투 등), 정해야 할 것은 기획서 `Docs/Project/Design.md`에 모은다.
참고 게임 비교와 고른 시스템 설명은 `Docs/Project/GameReferences.md`.

## 아직 없는 것 (다음 후보)

- 다이빙 연출 (포드 낙하, 행성이 커지는 장면, 대기권 섬광 뒤 로딩)
- 정거장 창밖으로 접근 행성을 실제 구체로 보여 주기
- 경영 카메라 회전 시 벽 자동 낮추기 (벽이 사장님을 가림)
- 가게 키우기(배치 영역 넓히기, 돈 쓰기), 손님 종족과 취향, 손님 도감 (칸 배치, 콤보, 손님 첫 시제품만 있음)
- 저장 파일 (가구 배치, 가방, 캔 기록, 돈, 발견한 콤보, 세계 시드)
- 지도/레이더 등 정식 UI (UMG)
- 입력을 `IA_`/`IMC_` 에셋으로 전환, 정식 UI(UMG), 자동화 테스트(`Tests/`), Git LFS
