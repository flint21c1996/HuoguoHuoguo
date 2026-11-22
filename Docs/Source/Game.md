# Game - 두 파트의 공통 부모(게임 모드, 컨트롤러)와 GameInstance

> 경로: `Source/HuoguoHuoguo/Game/` · 마지막 갱신: 2026-11-23

## 역할

경영 파트(`Shop/`)와 탐험 파트(`Explore/`)가 함께 쓰는 뼈대. 두 파트는 서로를 모르고, 둘 사이를 잇는 정보는 GameInstance에만 둔다.
파트는 맵 단위로 나뉜다. 맵의 World Settings > GameMode Override가 파트를 정한다.

## 파일

### HGGameModeBase (.h/.cpp)
- 추상 클래스. `DefaultPawnClass = nullptr`, `RestartPlayer`를 비워서 엔진 기본 Pawn 생성을 막는다. 사장님은 각 파트 컨트롤러가 직접 만든다.
- 하위: `AHGShopGameMode`(L_Station), `AHGExploreGameMode`(L_PlanetSurface, L_Asteroid_Krak7, 프로젝트 기본값).

### HGPlayerControllerBase (.h/.cpp)
추상 클래스. 두 파트 컨트롤러의 공통 기능.
- `Boss`: 지금 조종 중인 사장님(`AHGBossCharacter`). 하위 클래스가 만들어 넣는다.
- 맵 이동 `RequestTravel(Pad)`: 이동 발판이 부른다.
  - 다이빙 발판(`bDiveToSelectedPlanet`)이면 `GI->CommitDiveToSelectedPlanet()`으로 고른 행성을 고정한다.
  - 화면 암전 후 타이머로 도착 태그를 GameInstance에 맡기고 `OpenLevelBySoftObjectPtr`.
- 맵 시작 도우미: `ConsumeArrivalTag()`, `FindStart(Tag)`(태그 > Default > 아무거나), `FadeInAfterTravel()`.
- 입력 도우미 (Enhanced Input을 에셋 없이 코드로 만든다)
  - `MakeAction`, `ApplyMappingContext`(컨텍스트 하나만 남김)
  - `MapWASD`/`MapDirectionalKey`: 키보드 키를 Swizzle(YXZ)/Negate 수정자로 2D 축의 한 방향으로 매핑
  - `MapScaledKey`: Scalar 수정자로 키마다 다른 값 (숫자 키 1~4 → 1~4)
- 화면 안내 (정식 UI 전 임시): `ShowMessage`, `SetControlsHint`(고정 키 `0x48470001`)
- 개발용 콘솔 명령: `HGTravel <맵 경로 일부>`, `HGSimKey <키> <초>`
  - `HGSimKey`는 `InputKey(FInputKeyEventArgs::CreateSimulated)`로 엔진 입력에 키를 넣는다 (OS 입력 주입 아님). 뗄 때 Move 이벤트 수, 이동 거리, 이동 방향 Yaw, 위치, 파트별 정보(`DescribeForSimKey`)를 로그로 남긴다.

### HGGameInstance (.h/.cpp)
맵이 바뀌어도 유지되는 상태.
- 도착 태그: `SetPendingArrival` / `ConsumePendingArrival`
- 접근 행성: `UniverseSeed`(현재 고정값 20261123) + `OrbitCycle`로 `HGPlanet::MakeApproachingPlanets(…, 4)`를 처음 요청 때 만든다.
  - `AdvanceOrbitCycle()`, `SelectPlanet(Index)`, `GetSelectedPlanet()`
  - `CommitDiveToSelectedPlanet()` → `GetDivePlanet()` (TOptional). 표면 맵 생성기가 읽는다.

## 흐름 / 규칙

- 새 파트 공통 기능은 여기에, 한 파트에서만 쓰는 기능은 그 파트 폴더에 둔다.
- 맵 사이 정보는 GameInstance에만 둔다. 컨트롤러와 액터는 맵을 열 때마다 새로 만들어진다.
- 경영 ↔ 탐험을 한 맵 안에서 바꾸는 기능(예전 Tab 전환)은 없앴다. 오가는 길은 이동 발판뿐이다.

## 알려진 문제와 다음 할 일

- 입력이 코드 생성이다. 키 설정 화면이 필요해지면 `IA_`/`IMC_` 에셋으로 옮긴다.
- 화면 안내는 디버그 메시지다. UMG 위젯으로 바꿀 때 `ShowMessage`/`SetControlsHint`와 경영 파트의 행성 목록만 교체하면 된다.
- `UniverseSeed`는 새 게임 시작 시 무작위로 정하고 저장해야 한다 (저장 기능 없음). `OrbitCycle`은 아직 게임 내 날짜와 연결되지 않았다.
- 접근 행성 관리는 GameInstance 서브시스템으로 분리할 수 있다 (인벤토리가 생길 때 같이).
- 다이빙 연출은 화면 암전뿐이다.
