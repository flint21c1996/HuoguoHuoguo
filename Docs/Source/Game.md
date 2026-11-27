# Game - 두 파트의 공통 부모(게임 모드, 컨트롤러), GameInstance, 재료 데이터 구조

> 경로: `Source/HuoguoHuoguo/Game/` (플레이어 컨트롤러 공통 부모는 `Player/HGPlayerControllerBase`) · 마지막 갱신: 2026-11-26

## 역할

경영 파트(`Shop/`)와 탐험 파트(`Explore/`)가 함께 쓰는 뼈대. 두 파트는 서로를 모르고, 둘 사이를 잇는 정보는 GameInstance에만 둔다.
파트는 맵 단위로 나뉜다. 맵의 World Settings > GameMode Override가 파트를 정한다.

## 파일

### HGGameModeBase (.h/.cpp)
- 추상 클래스. `DefaultPawnClass = nullptr`, `RestartPlayer`를 비워서 엔진 기본 Pawn 생성을 막는다. 사장님은 각 파트 컨트롤러가 직접 만든다.
- 하위: `AHGShopGameMode`(L_Station), `AHGExploreGameMode`(L_PlanetSurface, Legacy/L_Asteroid_Krak7, 프로젝트 기본값).
  맵과 프로젝트 기본값에는 이들의 블루프린트(`BP_HG_ShopGameMode`, `BP_HG_ExploreGameMode`)가 들어간다 (`Docs/Content/Blueprints.md`).

### HGPlayerControllerBase (.h/.cpp)
추상 클래스. 두 파트 컨트롤러의 공통 기능.
- `Boss`: 지금 조종 중인 사장님(`AHGBossCharacter`). 하위 클래스가 만들어 넣는다.
- 맵 이동 `RequestTravel(Pad)`: 이동 발판이 부른다.
  - 다이빙 발판(`bDiveToSelectedPlanet`)이면 `GI->CommitDiveToSelectedPlanet()`으로 고른 행성을 고정한다. 문 닫을 시간(`!GI->CanDive()`)이면 거절 메시지를 띄우고 이동하지 않는다.
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
  - 도감 기록: `DiscoverIngredient`/`IsIngredientDiscovered`(스캔으로 보거나 캐면), `AddHarvestTotal`/`GetHarvestTotal`(캔 횟수), `DiscoverPlace`/`IsPlaceDiscovered`(`PlaceKey(군계, 종류)`, 종류 0 행성 1 소행성 지대 2 폐우주선), `AddBlueprintFound`/`GetBlueprintsFound`. 게임이 켜져 있는 동안만.
  - `HGSeq`: `|`로 나눈 명령을 `DevSequenceInterval`(1.5초, 콘솔 변수 `hg.SeqInterval`이 0보다 크면 그 값. 맵을 옮긴 뒤에도 적용) 간격으로 하나씩 `ConsoleCommand`로 실행한다. `wait`는 한 칸 쉰다. 하위 클래스의 `IsDevSequenceBusy()`가 참이면(예: 탐험 자동 조종 `HGGoTo` 진행 중) 다음 명령을 꺼내지 않고 기다린다. 걸리는 시간이 매번 다른 동작 뒤의 명령을 "끝난 뒤"에 내기 위해서다. 남은 명령은 GameInstance에 있어서 맵을 옮겨도 새 맵의 컨트롤러가 `BeginPlay`에서 이어 실행한다. 마지막 인자가 문자열이면 언리얼이 줄 끝까지 통째로 넘기므로 공백 있는 명령도 된다.
- `RequestTravel`은 "이동 시작" 로그에 발판 이름, 사장님 위치, 맵 시작 후 시간을 남긴다.
- 그림체 비교 스위치 (콘솔 변수 `hg.Look`, 기본 0, 명령 `HGStyle 0|1`, 게임 중 **L**로 번갈아): 0 = 지금 그대로, 1 = 셀 셰이딩.
  - 2026-11-26에 2 = 픽셀(큰 픽셀 + 색 단계)도 있었는데 사용자가 안 쓰기로 해서 뺐다.
  - 처음에는 F8이었는데 PIE에서 에디터가 F8(빙의 해제)을 먼저 가져가서 게임까지 오지 않았다. L은 두 파트 모두 안 쓰는 키다.
  - 화면 전체에 얹는 포스트 프로세스 머티리얼(`M_HG_LookToon`, `Docs/Content/Materials.md`)을 동적 인스턴스로 끼운다.
  - 콘솔 변수라 맵을 옮겨도 남는다. `PlayerTick`이 값이 바뀐 것을 보고 다시 끼운다. 로그 `[Look]`.
  - 조절 값은 프로젝트 설정 → Game → **HG 그림체**(`UHGLookSettings`, 아래)에 있다. `PlayerTick`은 설정 번호(`Revision`)가 바뀌었거나 머티리얼 인스턴스를 새로 만들었을 때만 값을 넣는다 (평소 프레임은 정수 비교 하나).
  - 그림체를 바꿀 때 스텐실 1(`HGLook::NoOutlineStencil`) 물체의 커스텀 깊이 그리기를 켜고 끈다 (셀일 때만 켬). 지금 그림체(0)에서는 커스텀 깊이 패스 비용이 0이다.

### HGLookSettings.h
- `UHGLookSettings`(`UDeveloperSettings`, config=Game, `DefaultGame.ini`): 그림체 비교 화면 효과의 조절 값. 프로젝트 설정 → Game → HG 그림체.
  - 셀 외곽선: 굵기(0.5, 1 미만은 1px 선의 진하기), 색, 진한 선 기준(0.004)과 거르기(4), 옅은 선(높낮이) 기준(0.002), 거르기(1.5), 진하기(0.35).
  - 셀 명암: 계단 간격(1 = 밝기 2배마다 한 단, 2 = 4배마다 한 단, 0 = 끔), 경계 부드러움(0.08), 채도(1.2).
  - 에디터에서 값을 바꾸면 `PostEditChangeProperty`가 `Revision`을 올린다. PIE 중에 바꾸면 다음 프레임에 바로 보인다.
- `HGLook::NoOutlineStencil`(1), `HGLook::NeedsStencil()`(`hg.Look >= 1`): 행성 생성기와 컨트롤러가 같이 쓴다.
- 처음에는 콘솔 변수(`hg.LookLineWidth`, `hg.LookBands`, `hg.LookBandSoft`)였는데, 사용자가 언리얼에서 직접 만지고 싶다고 해서 프로젝트 설정으로 옮겼다.
  - 포스트 프로세스 컴포넌트는 컨트롤러가 아니라 컨트롤러가 만든 빈 액터(`LookHolder`)에 붙인다. 컨트롤러는 늘 숨김 액터라서 직접 붙이면 `ShouldRender`가 거짓이 되어 화면에 안 나왔다.
  - 이름이 `HGStyle`인 것은 탐험 컨트롤러에 카메라 돌리기 명령 `HGLook`이 이미 있어서다.

### HGGameInstance (.h/.cpp)
맵이 바뀌어도 유지되는 상태.
- 도착 태그: `SetPendingArrival` / `ConsumePendingArrival`
- 접근 행성: `UniverseSeed`(현재 고정값 20261123) + `OrbitCycle`로 `HGPlanet::MakeApproachingPlanets(…, 4)`를 처음 요청 때 만든다.
  - `AdvanceOrbitCycle()`(다음 날 낮으로 되돌림), `SelectPlanet(Index)`, `GetSelectedPlanet()`
  - `CommitDiveToSelectedPlanet()` → `GetDivePlanet()` (TOptional). 표면 맵 생성기가 읽는다. 다이빙 중 표시(`bDiveInProgress`)도 켠다.
- 하루와 시간대 (기획서 "하루 구조")
  - `DayPhase`(`EHGDayPhase`: 낮/밤/문 닫을 시간), `GetDayNumber()` = 궤도 주기 + 1 (하루 = 궤도 주기 하나), `DescribeTime()` "1일째 밤".
  - `PassTime()`: 낮 → 밤 → 문 닫을 시간 → `AdvanceOrbitCycle()`(다음 날 낮). 다음 날로 넘어갔으면 true.
  - `FinishDive()`: 정거장에 돌아왔을 때 다이빙 중이었으면 `PassTime()` 한 번. 행성에 있는 동안은 내려간 시간대 그대로라서, 표면 맵은 `GetDayPhase()`로 내려간 시간대를 읽는다.
  - `CanDive()`: 문 닫을 시간이 아니면 true.
- 정거장 가구 배치 기록: `FHGPlacedFurniture`(가구 ID, 위치, Yaw) 배열. `HasShopLayout`, `GetShopLayout`, `SetShopLayout`.
  - 가구는 ID(`Data/Furniture.csv` 행 이름)로 남긴다. 다시 놓을 때 경영 파트가 그 행으로 가구를 만든다. 공통 폴더는 경영 파트의 가구 클래스를 모른다.
- `GetDevSequence()`: `HGSeq`가 아직 실행하지 않은 명령들.
- 재료
  - `IngredientTable`(소프트 참조, 기본 `/Game/HG/DataTable/DT_Ingredients`): `GetIngredientTable()`이 처음 쓸 때 불러온다. 없으면 에러 로그 한 번.
  - `FindIngredient(Id)`, `GetIngredientName(Id)`(표에 없으면 ID 그대로).
  - 가방 `Bag`(재료 ID → 개수): `AddToBag`, `GetBagCount`, `DescribeBag()`("서리다시마 2, 불씨고추 1", 표의 행 순서).
  - `RemoveFromBag(Id, Count)`: 그 재료를 Count개 꺼낸다 (가구 해금). 모자라면 꺼내지 않고 false.
  - `TakeOneFromBag(OutId)`: 표의 행 순서로 처음 남은 재료 하나를 꺼낸다. 가게 손님이 꼬치로 먹는다 (지금은 가방 = 가게 창고).
  - 캔 표식 기록 `HarvestedByPlanet`(행성 시드 → 표식 번호 집합): `IsMarkerHarvested`, `MarkHarvested`, `GetHarvestedCount`.
    `AdvanceOrbitCycle()`이 접근 목록에서 빠진 행성의 기록을 지운다 (`ForgetDepartedPlanets`). "행성이 떠날 때까지 캔 자리는 비어 있다".
- 가게: 돈과 콤보
  - `Money`(처음 300): `GetMoney`, `AddMoney`. 손님 계산으로 늘고, 가구를 놓거나 열 때 줄고, 가구를 치우면 절반이 돌아온다.
  - 열린 가구 `UnlockedFurniture`(가구 ID 집합): `IsFurnitureUnlocked`, `UnlockFurniture`(새로 열었으면 true). 처음부터 열린 가구(Start)는 여기 없어도 열린 것으로 친다 (판정은 경영 파트 `HGFurnitureUnlock`).
  - 받은 손님 수 `ServedCustomers`(게임 전체): `GetServedCustomers`, `AddServedCustomer`. 손님 수로 열리는 가구가 쓴다.
  - `ComboTable`(소프트 참조, 기본 `/Game/HG/DataTable/DT_Combos`): `GetComboTable()`. 행 구조체 `FHGComboRow`는 경영 파트(`Shop/Core/HGShopCombos.h`)에 있어서, 공통 폴더인 여기서는 표 에셋(`UDataTable`)만 들고 있는다.
  - 발견한 콤보 `DiscoveredCombos`(콤보 ID 집합): `DiscoverCombo(Id)`(처음이면 true), `GetDiscoveredCombos`. 나중에 도감 화면이 읽는다.

### HGDayTypes.h
- `EHGDayPhase`(`Day`/`Night`/`Closing`), `EHGIngredientTime`(`Any`/`Day`/`Night`, 재료 CSV의 `AvailableAt`).
- `HGDay::PhaseName(Phase)`("낮"/"밤"/"문 닫을 시간"), `HGDay::IsAvailable(Time, Phase)`(문 닫을 시간은 밤으로 친다).

### HGIngredientTypes.h
- `FHGIngredientRow`(DataTable 행): `DisplayName`, `Color`, `Price`(아직 안 씀), `HomeBiome`, `AvailableAt`(행성에서 보이는 시간대). 행 이름이 재료 ID.
- 경영(손님, 요리)과 탐험(채집)이 같이 쓰므로 공통 폴더에 둔다. 원본 데이터는 `Data/Ingredients.csv` (`Docs/Content/Data.md`).

## 흐름 / 규칙

- 새 파트 공통 기능은 여기에, 한 파트에서만 쓰는 기능은 그 파트 폴더에 둔다.
- 맵 사이 정보는 GameInstance에만 둔다. 컨트롤러와 액터는 맵을 열 때마다 새로 만들어진다.
- 경영 ↔ 탐험을 한 맵 안에서 바꾸는 기능(예전 Tab 전환)은 없앴다. 오가는 길은 이동 발판뿐이다.

## 알려진 문제와 다음 할 일

- 입력이 코드 생성이다. 키 설정 화면이 필요해지면 `IA_`/`IMC_` 에셋으로 옮긴다.
- 화면 안내는 디버그 메시지다. UMG 위젯으로 바꿀 때 `ShowMessage`/`SetControlsHint`와 경영 파트의 행성 목록만 교체하면 된다.
- `UniverseSeed`, 날짜와 시간대, 가구 배치, 가방, 캔 표식 기록, 돈, 발견한 콤보, 열린 가구, 받은 손님 수는 파일에 저장해야 한다 (저장 기능 없음).
- 재료/콤보 DataTable은 GameInstance 기본값의 소프트 경로로만 참조된다. 패키징할 때는 쿠킹 대상에 넣어야 한다 (Asset Manager 또는 "항상 쿠킹" 디렉터리).
- 시간대는 칸 단위로만 넘어간다 (시계, 시간 흐름 없음). 정거장과 행성의 조명은 시간대에 따라 바뀌지 않는다.
- 접근 행성 관리는 GameInstance 서브시스템으로 분리할 수 있다 (인벤토리가 생길 때 같이).
- 다이빙 연출은 화면 암전뿐이다.
