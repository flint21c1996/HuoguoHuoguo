# Shop - 경영 파트 (아이소 시점에서 사장님이 가게를 걷는다)

> 경로: `Source/HuoguoHuoguo/Shop/` · 마지막 갱신: 2026-11-23

## 역할

정거장 맵(L_Station)의 플레이. 가짜 아이소메트릭 카메라가 사장님을 따라가고, 사장님은 WASD로 가게 안을 걷는다.
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
  - Q/E: 90도 회전 (`RotateStepDegrees`). 휠: 확대/축소. 숫자 1~4: 다이빙할 행성 선택.
- `UpdateRotation`: 기본 처리 뒤 Pitch를 `CameraPitch`로, Roll을 0으로 되돌린다. 회전은 Z축 기준 Yaw만 남는다.
- 접근 행성 목록: 화면 고정 메시지(키 `0x48470002`), 고른 행성 앞에 `>>`.
- 개발 명령: `HGPlanets`, `HGSelectPlanet <번호>`, `HGDive [번호]`, `HGNextCycle`, `HGRotate <도>`.
- `DescribeForSimKey`: 카메라 Yaw/Pitch.

## 흐름 / 규칙

- 회전은 Z축(좌우)만. 위아래 기울기를 풀면 아이소 시점의 "내려다보는 판" 느낌과 가림 규칙이 무너진다.
- 확인됨 (스모크 테스트): 카메라 Yaw 45에서 S → 이동 방향 -135, `HGRotate 90` 후 S → -45. W로 에어록 발판까지 걸어가 다이빙, 탐험 파트로 전환.
- 확인 안 됨: 실제 마우스 우클릭 드래그 회전 감도와 커서 동작 (OS 입력을 넣지 않으므로 사람이 확인해야 한다).

## 알려진 문제와 다음 할 일

- 카메라를 돌리면 높은 벽이 사장님을 가린다. 벽 자동 낮추기(컷어웨이)가 필요하다.
- 손님, 주문, 요리, 재료 진열 등 경영 루프 전체가 아직 없다.
- 행성 선택과 출항이 키 입력 + 발판이다. 정식 UI(레이더/출항 화면)로 바꿀 후보.
