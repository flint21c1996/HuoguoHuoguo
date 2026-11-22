# Game - 두 파트의 공통 부모(게임 모드, 컨트롤러), GameInstance, 재료 데이터 구조

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
- 화면 안내 (정식 UI 전 임시): `ShowMessage`, `SetControlsHint`(고정 키 `0x48470001`), `SetPrompt`(상호작용 안내, 고정 키 `0x48470003`, 빈 문자열이면 지움)
  - 화면 디버그 메시지는 엔진 전역이라 맵이 바뀌어도 남는다. 그래서 `EndPlay`에서 상호작용 안내를 지운다 (경영 컨트롤러는 행성 목록 `0x48470002`도 지운다).
- 개발용 콘솔 명령: `HGTravel <맵 경로 일부>`, `HGSimKey <키> <초>`, `HGSeq <명령|명령|...>`, `HGBag`(가방 내용)
  - `HGSimKey`는 `InputKey(FInputKeyEventArgs::CreateSimulated)`로 엔진 입력에 키를 넣는다 (OS 입력 주입 아님). 뗄 때 Move 이벤트 수, 이동 거리, 이동 방향 Yaw, 위치, 파트별 정보(`DescribeForSimKey`)를 로그로 남긴다. 마우스 버튼(`LeftMouseButton`)도 된다.
  - `HGSeq`: `|`로 나눈 명령을 `DevSequenceInterval`(1.5초) 간격으로 하나씩 `ConsoleCommand`로 실행한다. `wait`는 한 칸 쉰다. 남은 명령은 GameInstance에 있어서 맵을 옮겨도 새 맵의 컨트롤러가 `BeginPlay`에서 이어 실행한다. 마지막 인자가 문자열이면 언리얼이 줄 끝까지 통째로 넘기므로 공백 있는 명령도 된다.
- `RequestTravel`은 "이동 시작" 로그에 발판 이름, 사장님 위치, 맵 시작 후 시간을 남긴다.

### HGGameInstance (.h/.cpp)
맵이 바뀌어도 유지되는 상태.
- 도착 태그: `SetPendingArrival` / `ConsumePendingArrival`
- 접근 행성: `UniverseSeed`(현재 고정값 20261123) + `OrbitCycle`로 `HGPlanet::MakeApproachingPlanets(…, 4)`를 처음 요청 때 만든다.
  - `AdvanceOrbitCycle()`, `SelectPlanet(Index)`, `GetSelectedPlanet()`
  - `CommitDiveToSelectedPlanet()` → `GetDivePlanet()` (TOptional). 표면 맵 생성기가 읽는다.
- 정거장 가구 배치 기록: `FHGPlacedFurniture`(클래스, 위치, Yaw) 배열. `HasShopLayout`, `GetShopLayout`, `SetShopLayout`.
  - 클래스는 `TSubclassOf<AActor>`로 들고 있다. 공통 폴더가 경영 파트(`Shop/HGFurniture.h`)를 include하지 않기 위해서이다.
- `GetDevSequence()`: `HGSeq`가 아직 실행하지 않은 명령들.
- 재료
  - `IngredientTable`(소프트 참조, 기본 `/Game/HG/Data/DT_Ingredients`): `GetIngredientTable()`이 처음 쓸 때 불러온다. 없으면 에러 로그 한 번.
  - `FindIngredient(Id)`, `GetIngredientName(Id)`(표에 없으면 ID 그대로).
  - 가방 `Bag`(재료 ID → 개수): `AddToBag`, `GetBagCount`, `DescribeBag()`("서리다시마 2, 불씨고추 1", 표의 행 순서).
  - 캔 표식 기록 `HarvestedByPlanet`(행성 시드 → 표식 번호 집합): `IsMarkerHarvested`, `MarkHarvested`, `GetHarvestedCount`.
    `AdvanceOrbitCycle()`이 접근 목록에서 빠진 행성의 기록을 지운다 (`ForgetDepartedPlanets`). "행성이 떠날 때까지 캔 자리는 비어 있다".

### HGIngredientTypes.h
- `FHGIngredientRow`(DataTable 행): `DisplayName`, `Color`, `Price`(아직 안 씀), `HomeBiome`. 행 이름이 재료 ID.
- 경영(손님, 요리)과 탐험(채집)이 같이 쓰므로 공통 폴더에 둔다. 원본 데이터는 `Data/Ingredients.csv` (`Docs/Content/Data.md`).

## 흐름 / 규칙

- 새 파트 공통 기능은 여기에, 한 파트에서만 쓰는 기능은 그 파트 폴더에 둔다.
- 맵 사이 정보는 GameInstance에만 둔다. 컨트롤러와 액터는 맵을 열 때마다 새로 만들어진다.
- 경영 ↔ 탐험을 한 맵 안에서 바꾸는 기능(예전 Tab 전환)은 없앴다. 오가는 길은 이동 발판뿐이다.

## 알려진 문제와 다음 할 일

- 입력이 코드 생성이다. 키 설정 화면이 필요해지면 `IA_`/`IMC_` 에셋으로 옮긴다.
- 화면 안내는 디버그 메시지다. UMG 위젯으로 바꿀 때 `ShowMessage`/`SetControlsHint`와 경영 파트의 행성 목록만 교체하면 된다.
- `UniverseSeed`, 가구 배치, 가방, 캔 표식 기록은 파일에 저장해야 한다 (저장 기능 없음).
- 재료 DataTable은 GameInstance 기본값의 소프트 경로로만 참조된다. 패키징할 때는 쿠킹 대상에 넣어야 한다 (Asset Manager 또는 "항상 쿠킹" 디렉터리). `OrbitCycle`은 아직 게임 내 날짜와 연결되지 않았다.
- 접근 행성 관리는 GameInstance 서브시스템으로 분리할 수 있다 (인벤토리가 생길 때 같이).
- 다이빙 연출은 화면 암전뿐이다.
