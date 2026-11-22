# World - 맵에 배치하는 표식과 발판 액터

> 경로: `Source/HuoguoHuoguo/World/` · 마지막 갱신: 2026-11-23

## 역할

디자이너가 맵에 놓아서 게임 흐름을 정하는 액터들. 게임 중에는 대부분 보이지 않고, 컨트롤러가 읽는다.

## 파일

### HGShopCameraAnchor (.h/.cpp)
- 경영 카메라의 초점 위치와 방향(화살표 = 카메라가 보는 수평 방향). `PanRadius`(기본 900)로 초점 이동 범위를 정한다.
- 이게 있는 맵만 경영 화면이 있다. 없으면 파밍지(탐험 전용)로 처리된다.
- 화살표는 게임 중 숨김, 에디터 전용 빌보드 아이콘.

### HGExplorerStart (.h/.cpp)
- 사장님이 나타나는 위치. `StartTag`(기본 `Default`)로 구분, `bSpaceSuit`면 헬멧 차림.
- 캐릭터와 같은 크기 캡슐(40/92, 게임 중 숨김, 충돌 없음)과 화살표. 바닥 + 93에 두면 발이 닿는다.
- 행성 맵에는 두지 않는다 (지형이 시드마다 달라서 생성기가 착륙 지점을 준다).

### HGTravelPad (.h/.cpp)
- 밟으면(탐험 캐릭터와 겹치면) `AHGPlayerController::RequestTravel(this)`.
- 속성: `DestinationLevel`(소프트 참조), `ArrivalTag`, `DestinationName`, `PadColor`, `bDiveToSelectedPlanet`.
- `bDiveToSelectedPlanet`: 다이빙 발판. 정거장에서 고른 접근 행성을 표면 맵으로 넘긴다.
- 트리거 상자(70x70x60)가 루트, 얇은 원기둥 메시가 바닥 표시. 액터는 바닥 + 60에 둔다. `OnConstruction`에서 색칠.

## 흐름 / 규칙

- 발판은 "어디로 보낼지"만 갖고, 암전/맵 열기는 컨트롤러가 한다.
- 현재 태그 약속: 정거장 `Default`(가게 안), `FromPlanet`(에어록 앞, 행성에서 귀환).

## 알려진 문제와 다음 할 일

- 행성 귀환 발판은 생성기가 런타임에 만든다 (`Docs/Source/Planet.md`).
