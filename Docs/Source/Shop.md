# Shop - 경영 파트 (아이소 시점에서 사장님이 가게를 걷고 가구를 배치한다)

> 경로: `Source/HuoguoHuoguo/Shop/` · 마지막 갱신: 2026-11-24

## 역할

정거장 맵(L_Station)의 플레이. 가짜 아이소메트릭 카메라가 사장님을 따라가고, 사장님은 WASD로 가게 안을 걷는다.
B로 가구 배치 모드에 들어가 식탁과 의자를 놓고, 옮기고, 치운다.
접근 행성을 고르고 에어록 다이빙 발판을 밟으면 탐험 파트로 넘어간다.

## 파일

### HGShopGameMode (.h/.cpp)
- `PlayerControllerClass = AHGShopPlayerController`. L_Station의 World Settings에서 지정.

### HGShopBossCharacter (.h/.cpp)
`AHGBossCharacter` 상속. 경영용 사장님 + 아이소 카메라.
- 가짜 아이소메트릭: 원근 카메라 FOV 15도, 스프링 암 기본 5000 (2500~9000), `CameraPitch` -35도.
- 스프링 암은 `bUsePawnControlRotation`(컨트롤러 Yaw를 따름), 충돌 검사 끔(벽에 끌려 들어오면 시점이 깨짐).
- 위치 지연(`CameraLagSpeed` 6)과 회전 지연(`CameraRotationLagSpeed` 10): 따라가기와 회전이 부드럽다.
- `AddZoomInput`: 목표 암 길이를 비율(12%)로 바꾸고 Tick에서 보간.
- 걷기 속도 420 (탐험보다 느림, 좁은 가게라서).

### HGShopPlayerController (.h/.cpp)
`AHGPlayerControllerBase` 상속.
- 맵 시작: 도착 태그(`FromPlanet` 등)에 맞는 `AHGExplorerStart`에 경영용 사장님을 만든다. 카메라 처음 방향 = `AHGShopCameraAnchor`의 Yaw(없으면 시작점 Yaw).
- 조작 (`IMC_Shop`, 커서 보임, GameAndUI)
  - WASD: 사장님 이동. 카메라 Yaw 기준이라 W = 화면 위쪽. (`AHGBossCharacter::Move`)
  - 우클릭 드래그: 카메라 좌우 자유 회전. 우클릭을 누르는 동안만 마우스 X를 `AddYawInput`. 커서는 손님/UI 클릭용으로 남겨 둔다.
  - Q/E: 90도 회전 (`RotateStepDegrees`). 휠: 확대/축소. 숫자 1~4: 다이빙할 행성 선택. B: 가구 배치 모드.
  - T: 시간 넘기기 (`HGPassTime`, `IA_Shop_PassTime`, 평소 컨텍스트에만). 낮 → 밤 → 문 닫을 시간 → 하루 마치기(다음 날 낮, 새 행성 목록). 손님 영업이 생기기 전 임시 조작이다.
- 가구 배치 모드 (`IMC_ShopBuild`로 컨텍스트를 통째로 바꾼다. 이동/카메라 키는 같다)
  - 1/2: 식탁/의자 고르기. 좌클릭: 놓기, 또는 가리킨 가구 집기. R: 45도 회전. X: 옮기기 취소 또는 가리킨 가구 치우기. B: 끝.
  - `PlayerTick`에서 매 프레임 `UHGFurniturePlacer::UpdateCursor`를 부른다. 조작 안내 줄에 손에 든 가구를 표시한다.
- `UpdateRotation`: 기본 처리 뒤 Pitch를 `CameraPitch`로, Roll을 0으로 되돌린다. 회전은 Z축 기준 Yaw만 남는다.
- 접근 행성 목록: 화면 고정 메시지(키 `0x48470002`), 고른 행성 앞에 `>>`. 첫 줄에 "N일째 낮/밤"과 "지금 내려가면 낮/밤 재료"를 쓴다. 문 닫을 시간이면 "오늘은 다이빙 끝. T로 하루를 마친다".
- 맵 시작 때 `FurniturePlacer->RestoreLayout()`으로 가구 배치를 되돌린다. 이동해 왔으면(행성에서 귀환) `GI->FinishDive()`로 시간대를 한 칸 넘기고("행성에서 돌아오니 1일째 밤이 되었다") 가방 내용을 메시지로 보여 준다 (`HGBag`).
- `EndPlay`: 접근 행성 목록 메시지(`0x48470002`)를 지운다. 지우지 않으면 행성 맵 화면에도 남았다.
- 개발 명령: `HGPlanets`, `HGSelectPlanet <번호>`, `HGDive [번호]`, `HGNextCycle`(다음 날 낮으로), `HGPassTime`(T와 같음), `HGRotate <도>`,
  `HGBuild`(배치 모드 전환), `HGBuildSelect <번호>`, `HGCursorAt <X> <Y>`(시험용 커서를 바닥 지점에), `HGCursorClear`, `HGFurniture`(배치 기록과 커서 상태 로그).
- `DescribeForSimKey`: 카메라 Yaw/Pitch.

### HGFurniture (.h/.cpp)
가구 공통 부모 `AHGFurniture`(추상)와 `AHGTable`, `AHGChair`.
- 외형은 엔진 기본 도형 부품 조합 (`AddPart`). 액터 원점 = 바닥 위 가구 중심.
  - 식탁: 상판 지름 156(높이 74~82), 가운데 다리, 바닥 받침(지름 70), 냄비와 국물(장식, 충돌 없음).
  - 의자: 좌석 지름 56(높이 0~45), 등받이(-X 쪽, 높이 45~95). 앞쪽 +X가 식탁을 보게 돌린다.
- 실제 가구는 부품이 `BlockAll`(사장님을 막고 커서 추적에 걸림).
- 유령(`MakeGhost`): 부품을 `QueryOnly` + 모든 채널 Overlap + 겹침 이벤트 끔. 아무것도 막지 않지만 충돌 모양이 있어 겹침 검사에 쓴다.
- `SetPreviewState`: None(원래 색) / Valid(초록) / Invalid(빨강) / Hover(주황). 동적 머티리얼이 있으면 색 값만 바꾼다.
- 나중에 BP_Table 같은 블루프린트 자식에서 메시만 바꿔도 배치 규칙이 그대로 동작하도록, 별도의 "발자국 크기" 없이 부품 충돌 모양으로 판정한다.

### HGFurniturePlacer (.h/.cpp)
경영 컨트롤러에 붙는 배치 모드 컴포넌트.
- 속성: `Catalog`(기본 식탁, 의자), `GridSize` 25, `RotateStepDegrees` 45, `FloorZ` 0.
- 커서 → 바닥: 마우스 화면 좌표를 `DeprojectScreenPositionToWorld`로 광선으로 바꾸고, 광선이 `FloorZ` 수평면과 만나는 점을 격자에 맞춘다.
  시험용 `SetCursorWorldOverride`는 월드 지점을 `ProjectWorldLocationToScreen`으로 화면 좌표로 바꾼 뒤 같은 경로를 탄다.
- 가리키기: 같은 광선을 Visibility로 추적해 맞은 것이 가구면 Hover. 옮기는 중에는 보지 않는다.
- `CheckPlacement`
  1. 겹침: 유령의 충돌 부품마다 `UWorld::ComponentOverlapMulti`(바닥에서 2 띄움, 물체 종류 WorldStatic/WorldDynamic/Pawn/PhysicsBody). 벽, 카운터, 다른 가구, 사장님, 발판 트리거와 겹치면 불가. 부품 모양 그대로라 의자를 식탁 상판 밑으로 밀어 넣을 수 있다.
  2. 바닥: 외곽 상자 네 모서리(4 안쪽)와 중심에서 아래로 추적해 `FloorZ` ±6 안에 바닥이 있어야 한다.
- 집기: 가구를 지우고 같은 종류/방향 유령을 든다. 원래 자리는 `MovingFrom`에 남겨 X나 배치 모드 끝, 다른 가구 고르기 때 되돌린다.
- 배치 기록: 놓기/치우기/취소 때 맵의 모든 실제 가구(클래스, 위치, Yaw)를 `UHGGameInstance::SetShopLayout`에 통째로 쓴다.
  `RestoreLayout`: 기록이 없으면(게임 켜고 처음) 맵의 가구를 기록, 있으면 맵의 가구를 치우고 기록대로 다시 놓는다.

## 흐름 / 규칙

- 회전은 Z축(좌우)만. 위아래 기울기를 풀면 아이소 시점의 "내려다보는 판" 느낌과 가림 규칙이 무너진다.
- 가구의 처음 배치는 맵(L_Station의 `Shop/Hall` 폴더 가구 액터)이 기준이다. 에디터에서 옮겨 두면 그게 새 게임의 처음 배치가 된다.
- 확인됨 (스모크 테스트): 카메라 Yaw 45에서 S → 이동 방향 -135, `HGRotate 90` 후 S → -45. W로 에어록 발판까지 걸어가 다이빙, 탐험 파트로 전환.
- 확인됨 (가구, `HGSeq` + `HGCursorAt` + `HGSimKey`로 실제 입력 경로를 거침):
  - 빈 바닥 (350, 150)에 식탁 놓기 성공. 카운터 위는 "겹침: Counter", 가게 밖 (800, 0)은 "바닥이 없음"으로 거부.
  - 의자를 R 두 번(90도)으로 돌려 식탁에서 100 떨어진 자리에 놓기 성공 (상판 밑으로 들어감).
  - 기존 식탁을 가리키면 Hover, 클릭으로 집어 다른 자리에 놓기 성공. X로 가리킨 의자 치우기 성공.
  - 정거장 → 행성 → 정거장 왕복 뒤 "기록에서 8개를 다시 놓음"(처음 8 + 식탁 1 - 의자 1).
- 확인됨 (하루 구조, `HGSeq`): 1일째 낮 다이빙 → 귀환 "1일째 밤" → 같은 행성 다시 다이빙 → 귀환 "1일째 문 닫을 시간" → 다이빙 거절 "문 닫을 시간에는 다이빙할 수 없다" → `HGPassTime` "2일째 낮", 새 행성 4개, 떠난 행성의 캔 기록 삭제. 화면 캡처로 행성 목록 첫 줄 "2일째 낮 ... (지금 내려가면 낮 재료)" 확인.
- 확인됨 (사용자가 직접 플레이): 실제 T 키로 2일째 ~ 6일째까지 낮 → 밤 → 문 닫을 시간 → 다음 날이 반복되고 메시지가 맞게 나온다.
- 확인 안 됨: 실제 마우스 우클릭 드래그 회전 감도와 커서 동작, 실제 마우스로 가구를 놓는 손맛 (OS 입력을 넣지 않으므로 사람이 확인해야 한다).

## 알려진 문제와 다음 할 일

- 카메라를 돌리면 높은 벽이 사장님을 가린다. 벽 자동 낮추기(컷어웨이)가 필요하다.
- 손님, 주문, 요리, 재료 진열 등 경영 루프 전체가 아직 없다. 손님이 앉을 자리는 의자의 +X 방향을 쓰면 된다.
- 가구 배치 기록은 게임이 켜져 있는 동안만 남는다 (저장 파일 없음). 가구 값(돈)과 보유 수량 제한도 없다.
- 배치 모드에서 유령은 단색(초록/빨강)이다. 반투명 머티리얼은 아트 단계에서 바꾼다.
- 한 번 관찰됨 (재현 안 됨): 행성에서 정거장으로 돌아온 지 1.3초 만에 사장님이 다이빙 발판에 닿아 다시 다이빙했다. 같은 순서로 5번 더 돌렸을 때는 나오지 않았다. 원인 확인을 위해 `RequestTravel` 로그에 사장님 위치와 맵 시작 후 시간을 남긴다.
- 행성 선택과 출항이 키 입력 + 발판이다. 정식 UI(레이더/출항 화면)로 바꿀 후보.
