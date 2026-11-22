# 훠궈훠궈 (HuoguoHuoguo)

우주 정거장 훠궈집 경영 + 행성 파밍 게임. Unreal Engine 5.8, C++ 중심.
원형 웹 프로토타입: https://claude.ai/artifact/Rzyxuvad28xVVd2dydoxoE

## 핵심 설계 원칙

- 게임 로직, 규칙, 상태는 C++에 둔다. 블루프린트는 C++ 클래스를 상속해 비주얼만 연결한다.
- 밸런스 데이터(재료, 종족, 업그레이드, 파밍지)는 CSV/JSON 텍스트로 두고 DataTable로 읽는다.
- `.uasset`/`.umap`은 사용자가 요청하지 않으면 수정하지 않는다. 에셋이 없어도 돌아가도록 코드에서 기본값을 만든다.
- 경영 파트: 가짜 아이소메트릭(원근 카메라 + 좁은 FOV + 먼 거리). 진짜 직교 투영은 쓰지 않는다.
- 파밍 파트: 3인칭 원근 카메라.
- 두 모드는 서로 다른 Pawn이며 `AHGPlayerController`가 Possess와 입력 매핑 컨텍스트를 바꾼다.

## 소스 구조 (`Source/HuoguoHuoguo/`)

- `Game/` 게임 모드, 플레이어 컨트롤러 (모드 전환, 입력)
- `Shop/` 경영 파트 (아이소 카메라 Pawn 등)
- `Explore/` 파밍 파트 (3인칭 탐험 캐릭터 등)
- `World/` 레벨에 놓이는 공용 액터 (이동 발판 등)
- `Dev/` 개발용 회색 박스 생성기 등. 정식 콘텐츠가 생기면 정리한다.
- 클래스 접두사: `HG` (예: `AHGPlayerController`)

## 빌드

에디터가 켜져 있고 Live Coding이 활성화되어 있으면 명령줄 빌드가 실패한다. 빌드 전에 에디터를 닫거나 에디터에서 Live Coding으로 컴파일한다.

```powershell
& "E:\UE_5.8\Engine\Build\BatchFiles\Build.bat" HuoguoHuoguoEditor Win64 Development -Project="E:\HuoguoHuoguo\HuoguoHuoguo.uproject" -WaitMutex
```

## 실행과 확인

- 기본 맵은 `/Engine/Maps/Entry`(빈 맵)이고, `HGGameMode`가 시작할 때 회색 박스 가게와 소행성을 코드로 만든다.
- 에디터 없이 실행: `UnrealEditor.exe "E:\HuoguoHuoguo\HuoguoHuoguo.uproject" -game -windowed -ResX=1280 -ResY=720 -log`
- 개발용 콘솔 명령 (`~` 콘솔 또는 `-ExecCmds="..."`):
  - `HGMode shop|explore` : 모드 전환
  - `HGTravel space|shop` : 소행성/가게로 이동
- 화면 확인은 게임 창만 캡처한다 (PrintWindow). 화면 전체 캡처나 키 입력 주입은 하지 않는다.

## 코드 스타일

- 주석은 한국어로, 담백하게. 이모지 금지. 코드가 하는 일과 그 이유(원리)를 적는다.
- 새 C++ 파일은 UTF-8 BOM으로 저장한다 (MSVC가 한국어 주석을 올바르게 읽도록).

## Git

- git은 PATH에 없다. `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe` 사용.
- 커밋과 푸시는 사용자 승인 후에만 한다.
