# Explore - 탐험 파트 (3인칭으로 행성을 걷고 재료를 캔다)

> 경로: `Source/HuoguoHuoguo/Explore/` · 마지막 갱신: 2026-11-23

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
- 개발 명령: `HGWarpToIngredient`(가장 가까운 남은 표식 옆 1m, 위 1.5m로 순간 이동), `HGIngredients`(남은 표식 수 + 가방).
- `UpdateRotation`: 행성 중력 중이면 시점을 중력 좌표계로 옮겨 Pitch 제한/Roll 제거 후 월드로 되돌린다. 지난 프레임 이후 중력 좌표계가 돈 만큼 시점도 같이 돌려 지평선을 유지한다.
- `DescribeForSimKey`: 행성 중심까지 거리.

## 흐름 / 규칙

- 확인됨 (스모크 테스트): 반지름 51m 행성에서 W 25초로 적도 너머까지 걸어도 지표에 붙어 있음(중심 거리 약 52m), 정거장에서 다이빙해 착륙.
- 확인됨 (채집, `HGSeq` + `HGWarpToIngredient` + `HGSimKey E`): 반지름 72m 얼음 행성에서 표식 3개를 캠(거리 117~124). 가방 "서리다시마 2, 불씨고추 1". 정거장 귀환 후 같은 행성에 다시 내려가면 "이미 캔 것 3개 숨김", 남은 표식 21 → 18. 궤도 주기를 넘기면 그 행성 기록 삭제.
- 확인 안 됨: 실제 마우스 시점 조작감, 남극 근처 카메라 안정성, 실제로 걸어가서 E로 캐는 손맛.

## 알려진 문제와 다음 할 일

- 산소 같은 탐험 제한 요소 없음. 가방 용량 제한 없음.
- 채집 연출 없음 (표식이 바로 사라진다). 표식이 작은 구라서 멀리서 찾기 어렵다.
- 착륙/귀환 연출 없음 (화면 암전뿐).
