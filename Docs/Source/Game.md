# Game - 게임 모드, 플레이어 컨트롤러, GameInstance

> 경로: `Source/HuoguoHuoguo/Game/` · 마지막 갱신: 2026-11-23

## 역할

게임 전체 흐름을 쥔다. 맵을 열면 맵에 배치된 표식을 읽어 Pawn을 만들고, 경영/탐험 모드 전환, 입력, 맵 이동(발판, 다이빙)을 처리한다. 맵 사이에 넘길 상태는 GameInstance에 둔다.

## 파일

### HGGameMode (.h/.cpp)
- `DefaultPawnClass = nullptr`, `PlayerControllerClass = AHGPlayerController`.
- `RestartPlayer`를 비워서 엔진 기본 Pawn 생성을 막는다. Pawn은 컨트롤러가 직접 만든다.

### HGPlayerController (.h/.cpp)
두 모드를 오가는 중심 컨트롤러.
- 맵 초기화 `InitializeFromLevel()` (BeginPlay에서 한 번)
  - GameInstance의 도착 태그를 꺼낸다 (`ConsumePendingArrival`).
  - `AHGPlanetGenerator`가 있으면 행성 맵: `EnsureGenerated()` 후 `GetLandingTransform()`에 사장님을 만들고 `SetPlanetGravity`를 켠다. 우주복 차림.
  - 없으면 `AHGExplorerStart` 중 태그가 맞는 것 > `Default` > 아무거나 순으로 골라 사장님을 만든다.
  - `AHGShopCameraAnchor`가 있으면 경영 카메라 Pawn을 만든다. 시작 모드: 가게가 있고 이동해 온 것이 아니면 경영, 아니면 탐험.
- 모드 전환 `SetPlayMode` (Tab)
  - Possess를 바꾸고 `SetViewTargetWithBlend(EaseInOut)`로 카메라를 블렌드한다. `bAutoManageActiveCameraTarget = false`.
  - 탐험으로 갈 때 경영 카메라의 Yaw를 이어받고, 경영으로 갈 때 초점을 사장님 위치로 옮긴다.
  - 경영 화면이 없는 맵(행성)에서는 경영 모드로 바꾸지 않는다.
- 맵 이동 `RequestTravel(Pad)`
  - 발판이 `bDiveToSelectedPlanet`이면 `GI->CommitDiveToSelectedPlanet()`으로 고른 행성을 고정한다.
  - 화면 암전 후 타이머로 도착 태그를 GameInstance에 맡기고 `OpenLevelBySoftObjectPtr`.
- 시점 `UpdateRotation` 재정의
  - 행성 중력 중이면 시점을 중력 좌표계(`Explorer->GetGravityFrame()`)로 옮겨 Pitch 제한/Roll 제거를 한 뒤 월드로 되돌린다.
  - 지난 프레임 이후 중력 좌표계가 돈 만큼 시점도 같이 돌린다 (지평선 유지).
- 입력: Enhanced Input 액션/매핑 컨텍스트를 에셋 없이 코드에서 만든다 (`BuildInputObjects`).
  - 공용 `IMC_Common`: Tab(모드), 숫자 1~4(행성 선택, 액션 하나 + Scalar 수정자로 값 1~4)
  - 경영 `IMC_Shop`: WASD 카메라 이동, 휠 확대, Q/E 90도 회전. 커서 보임.
  - 탐험 `IMC_Explore`: WASD 이동, 마우스 시점, Space 점프. 커서 숨김.
  - WASD 2D 축은 Swizzle(YXZ)/Negate 수정자로 만든다.
- 화면 안내 (정식 UI 전 임시, `AddOnScreenDebugMessage` 고정 키)
  - `UpdateControlsHint()` 키 `0x48470001`: 모드별 조작 안내
  - `UpdatePlanetBoard()` 키 `0x48470002`: 정거장에서 접근 행성 목록, 고른 행성에 `>>`
- 개발용 콘솔 명령 (`~` 또는 `-ExecCmds="A,B"`)
  - `HGMode shop|explore`, `HGTravel <맵 경로 일부>`
  - `HGPlanets`, `HGSelectPlanet <번호>`, `HGDive [번호]`, `HGNextCycle`
  - `HGSimKey <키> <초>`: `InputKey(FInputKeyEventArgs::CreateSimulated)`로 엔진 입력에 키를 넣고, 뗄 때 Move 이벤트 수, 이동 거리, 행성 중심 거리, 위치를 로그로 남긴다. OS 입력 주입이 아니다.

### HGGameInstance (.h/.cpp)
맵이 바뀌어도 유지되는 상태.
- 도착 태그: `SetPendingArrival` / `ConsumePendingArrival`
- 접근 행성: `UniverseSeed`(현재 고정값 20261123) + `OrbitCycle`로 `HGPlanet::MakeApproachingPlanets(…, 4)`를 처음 요청 때 만든다.
  - `AdvanceOrbitCycle()`, `SelectPlanet(Index)`, `GetSelectedPlanet()`
  - `CommitDiveToSelectedPlanet()` → `GetDivePlanet()` (TOptional). 표면 맵 생성기가 읽는다.

## 흐름 / 규칙

- 맵 사이 정보는 GameInstance에만 둔다. 컨트롤러와 액터는 맵을 열 때마다 새로 만들어진다.
- 지형이 먼저 있어야 하는 초기화는 컨트롤러가 생성기의 `EnsureGenerated()`를 직접 불러 순서를 보장한다 (BeginPlay 순서는 정해져 있지 않다).

## 알려진 문제와 다음 할 일

- 입력이 코드 생성이다. 키 설정 화면이 필요해지면 `IA_`/`IMC_` 에셋으로 옮긴다.
- 화면 안내는 디버그 메시지다. UMG 위젯으로 바꿀 때 `UpdateControlsHint`/`UpdatePlanetBoard`만 교체하면 된다.
- `UniverseSeed`는 새 게임 시작 시 무작위로 정하고 저장해야 한다 (저장 기능 없음). `OrbitCycle`은 아직 게임 내 날짜와 연결되지 않았다.
- 다이빙 연출은 화면 암전뿐이다.
