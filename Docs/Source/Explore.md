# Explore - 탐험 파트 (3인칭으로 행성을 걷는다)

> 경로: `Source/HuoguoHuoguo/Explore/` · 마지막 갱신: 2026-11-23

## 역할

행성 표면 맵(L_PlanetSurface)의 플레이. 3인칭 카메라, 구형 중력. 정거장으로 돌아가는 길은 금색 귀환 발판뿐이다.

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
- 조작 (`IMC_Explore`, 커서 숨김, GameOnly): WASD 이동(카메라 기준), 마우스 시점, Space 점프.
- `UpdateRotation`: 행성 중력 중이면 시점을 중력 좌표계로 옮겨 Pitch 제한/Roll 제거 후 월드로 되돌린다. 지난 프레임 이후 중력 좌표계가 돈 만큼 시점도 같이 돌려 지평선을 유지한다.
- `DescribeForSimKey`: 행성 중심까지 거리.

## 흐름 / 규칙

- 확인됨 (스모크 테스트): 반지름 51m 행성에서 W 25초로 적도 너머까지 걸어도 지표에 붙어 있음(중심 거리 약 52m), 정거장에서 다이빙해 착륙.
- 확인 안 됨: 실제 마우스 시점 조작감, 남극 근처 카메라 안정성.

## 알려진 문제와 다음 할 일

- 재료 채집, 인벤토리 없음. 산소 같은 탐험 제한 요소 없음.
- 착륙/귀환 연출 없음 (화면 암전뿐).
