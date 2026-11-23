# Explore - 탐험 파트 (3인칭으로 행성을 걷고 재료를 캐고 미니맵을 본다)

> 경로: `Source/HuoguoHuoguo/Explore/` · 마지막 갱신: 2026-11-24

## 역할

행성 표면 맵(L_PlanetSurface)의 플레이. 3인칭 카메라, 구형 중력. 재료 표식 옆에서 E를 눌러 캐서 가방에 넣는다. 정거장으로 돌아가는 길은 금색 귀환 발판뿐이다.

## 파일

### HGExploreGameMode (.h/.cpp)
- `PlayerControllerClass = AHGExplorePlayerController`. L_PlanetSurface, L_Asteroid_Krak7의 World Settings에서 지정.
- 프로젝트 기본 게임 모드이기도 하다 (지정 없는 시험용 맵을 열면 바로 걸어 다닐 수 있게).

### HGExplorerCharacter (.h/.cpp)
`AHGBossCharacter` 상속. 탐험용 사장님 + 3인칭 카메라 + 구형 중력.
- 카메라: 스프링 암 420, 충돌 검사, 소켓 오프셋 Z 60, 위치 지연, FOV 75. `Look`은 Pitch 부호 반전.
- 구형 중력
  - `SetPlanetGravity(Center, GravityScale)`: 켜면 매 Tick `CharacterMovement->SetGravityDirection(중심 방향)` (UE 5.4+ 임의 중력 지원).
  - `GetGravityFrame()`: "중력 기준 위 = 지표 바깥" 회전. 매 프레임 변화량만 이어 붙이는 평행 이동 방식이라 남극에서도 축이 튀지 않는다.
  - `GetMoveFrame()`을 중력 좌표계로 돌려줘서, 공통 `Move`가 행성 위에서도 카메라 기준으로 걷게 한다.
  - CharacterMovement는 캐릭터 Tick 뒤에 틱하므로 바뀐 중력이 같은 프레임에 쓰인다.

### HGExplorePlayerController (.h/.cpp)
`AHGPlayerControllerBase` 상속.
- 맵 시작: `AHGPlanetGenerator`가 있으면 `EnsureGenerated()` 후 `GetLandingTransform()`(북극)에 사장님을 만들고 구형 중력을 켠다. 없으면 도착 태그의 `AHGExplorerStart`. 우주복 차림.
- 조작 (`IMC_Explore`, 커서 숨김, GameOnly): WASD 이동(카메라 기준), 마우스 시점, Space 점프, E 채집.
  - E를 고른 이유: 탐험은 마우스가 시점 회전이라 커서가 없어서 클릭 채집이 어색하다. WASD 옆 상호작용 키로 E를 쓴다. `IMC_` 에셋으로 옮기면 바꿀 수 있다.
- 채집
  - `PlayerTick` → `UpdateHarvestTarget`: `AHGPlanetGenerator::FindNearestIngredient(사장님 위치, HarvestRadius 220)`. 있으면 상호작용 안내 `[E] 채집: <재료 이름>`(`SetPrompt`), 없으면 지운다.
  - `OnInteract`(E 누름): 그 순간 다시 찾아 `HarvestIngredient` → `GI->AddToBag` → 메시지 "채집: 불씨고추 +1 (가방에 N개, 이 행성 남은 재료 M)".
  - 바로 캔다 (누르고 기다리기 없음).
- 미니맵: 행성 맵이면 `CreateMinimap()`으로 `UHGPlanetMinimap`을 화면에 붙인다 (전체 화면 위젯, 지도는 오른쪽 위에 그림). M 키(`IA_Explore_Map`)로 근거리/행성 전체 전환.
- 개발 명령: `HGWarpToIngredient`(가장 가까운 남은 표식 옆 1m, 위 1.5m로 순간 이동), `HGIngredients`(남은 표식 수 + 가방),
  `HGMapInfo`(미니맵 기준 귀환 발판 방위와 표면/직선 거리), `HGFacePad`(미니맵이 가리키는 발판 방향으로 시점 회전).

### HGPlanetMinimap (.h/.cpp)
탐험 파트 오른쪽 위 원형 미니맵. 위젯 에셋 없이 C++ `UUserWidget`의 `NativePaint`에서 Slate로 직접 그린다.
- 투영: 방위 등거리 투영 (`ProjectToMap`). 사장님 자리가 지도 중심.
  - 중심에서 점까지 거리 = (행성 중심에서 본 두 방향 사이 각도) × 평균 반지름 = 표면을 따라 걷는 거리.
  - 방향 = 사장님 발밑 접평면에서 그 점 쪽으로 출발하는 방향. 지도 위쪽 = 카메라가 보는 방향(지표에 눕힘). 지도에서 위에 있으면 W로 가면 된다.
  - 왜: 구는 평면으로 펼칠 수 없고, 위도/경도로 펼치면 착륙 지점(북극) 주변이 가장 찌그러진다. 이 투영은 "어디로 몇 m"가 정확하다.
- 범위: 근거리 `LocalRangeCm` 40m / 행성 전체(반 바퀴 = pi × 반지름, 테두리 = 정반대편, 안쪽 흐린 원 = 90도 선).
- 그리는 것: 반투명 원 배경, 테두리, 안내 원, 캐지 않은 재료 표식(재료 색 점), 귀환 발판(금색 원, 범위 밖이면 테두리의 금색 삼각형 화살표), 중심의 사장님 화살표, 아래 글자 "귀환 발판 N m"와 "[M] 근거리 40m / 행성 전체".
- 크기 `MapDiameter` 220, 화면 모서리 여백 `ScreenMargin` 24 (Slate 단위, 해상도 배율이 곱해짐).
- 위젯은 화면 전체를 덮고(입력 안 받음) 지도를 직접 오른쪽 위에 그린다. 뷰포트 슬롯 앵커는 쓰지 않는다: `SetPositionInViewport`가 앵커를 (0,0)으로 되돌려 지도가 화면 왼쪽 밖에 그려진 적이 있다.
- 처음 그릴 때 위치와 크기를 `[Map]` 로그로 한 번 남긴다.
- `UpdateRotation`: 행성 중력 중이면 시점을 중력 좌표계로 옮겨 Pitch 제한/Roll 제거 후 월드로 되돌린다. 지난 프레임 이후 중력 좌표계가 돈 만큼 시점도 같이 돌려 지평선을 유지한다.
- `DescribeForSimKey`: 행성 중심까지 거리.

## 흐름 / 규칙

- 확인됨 (스모크 테스트): 반지름 51m 행성에서 W 25초로 적도 너머까지 걸어도 지표에 붙어 있음(중심 거리 약 52m), 정거장에서 다이빙해 착륙.
- 확인됨 (채집, `HGSeq` + `HGWarpToIngredient` + `HGSimKey E`): 반지름 72m 얼음 행성에서 표식 3개를 캠(거리 117~124). 가방 "서리다시마 2, 불씨고추 1". 정거장 귀환 후 같은 행성에 다시 내려가면 "이미 캔 것 3개 숨김", 남은 표식 21 → 18. 궤도 주기를 넘기면 그 행성 기록 삭제.
- 확인됨 (미니맵): 귀환 발판이 지도 방위 110.2도, 36.1m일 때 `HGFacePad`로 그쪽을 보고 W 3초 → 방위 0도 유지, 21.1m로 15m 가까워짐 (지도 방향 = 실제 걷는 방향).
  화면 캡처로 근거리(범위 안 발판 = 금색 점), 행성 전체(남은 재료 20개 색 점, 90도 원), 범위 밖 발판(144m, 테두리 화살표) 표시 확인. 한국어 글자 정상.
- 확인 안 됨: 실제 마우스 시점 조작감, 남극 근처 카메라 안정성, 실제로 걸어가서 E로 캐는 손맛, 1280x720 외 해상도에서 미니맵 크기.

## 알려진 문제와 다음 할 일

- 산소 같은 탐험 제한 요소 없음. 가방 용량 제한 없음.
- 채집 연출 없음 (표식이 바로 사라진다). 표식이 작은 구라서 멀리서 찾기 어렵다 (미니맵으로 보완).
- 미니맵: 바위와 지형 높낮이는 그리지 않는다. 원 둘레 시작점(3시 방향)에 선 끝이 겹친 작은 눈금이 보인다. 화면 크게 펼치는 전체 지도(UMG 화면)는 아직 없다.
- 착륙/귀환 연출 없음 (화면 암전뿐).
