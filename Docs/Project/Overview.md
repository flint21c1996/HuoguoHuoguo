# 프로젝트 개요 - 우주 정거장 훠궈집 경영 + 절차적 행성 파밍

> 대상: 프로젝트 전체 · 마지막 갱신: 2026-11-23

## 한 줄 요약

우주 정거장에서 훠궈집을 경영하고(아이소메트릭 시점), 정거장 근처로 접근하는 절차적 행성에 다이빙해서 재료를 파밍한다(3인칭 시점). Unreal Engine 5.8, C++ 중심.
원형 웹 프로토타입: https://claude.ai/artifact/Rzyxuvad28xVVd2dydoxoE

## 현재 플레이 흐름 (구현됨)

1. 게임은 `L_Station`(정거장 훠궈집)에서 경영 모드로 시작한다. 화면 왼쪽 위에 조작 안내와 "접근 중인 행성" 목록이 계속 떠 있다.
2. 숫자 키 1~4로 다이빙할 행성을 고른다. 목록은 `UHGGameInstance`가 궤도 주기마다 시드로 뽑는다.
3. Tab으로 탐험 모드(사장님 3인칭)로 바꾸고, 에어록의 청록 다이빙 발판을 밟으면 화면이 어두워지며 `L_PlanetSurface`로 이동한다.
4. 행성 표면 맵의 `AHGPlanetGenerator`가 고른 행성의 시드로 구형 지형, 바위, 재료 표식, 귀환 발판을 만든다. 사장님은 북극에 착륙하고 행성 중심 방향 중력을 받는다.
5. 착륙 지점 옆 금색 귀환 발판을 밟으면 정거장의 `FromPlanet` 시작점으로 돌아온다.

## 핵심 설계 원칙

- 게임 로직, 규칙, 상태는 C++에 둔다. 블루프린트는 C++ 클래스를 상속해 비주얼만 연결한다.
- 밸런스 데이터(재료, 종족, 업그레이드, 파밍지)는 CSV/JSON 텍스트로 두고 DataTable로 읽는다 (아직 없음).
- 손으로 다듬는 장소(정거장)는 맵 에셋(`.umap`) 배치가 기준이다. 런타임에 코드로 지형을 만들지 않는다.
- 매번 달라지는 장소(행성)는 맵이 아니라 시드(`FHGPlanetSeed`)로 존재하고, 표면 맵에서 생성기가 만든다.
- 경영 파트: 가짜 아이소메트릭(원근 카메라 + 좁은 FOV 15도 + 먼 거리). 진짜 직교 투영은 쓰지 않는다.
- 파밍 파트: 3인칭 원근 카메라 (FOV 75도).
- 두 모드는 서로 다른 Pawn이며 `AHGPlayerController`가 Possess, 뷰 블렌드, 입력 매핑 컨텍스트를 바꾼다.

## 폴더 지도

| 프로젝트 경로 | 내용 | 문서 |
|---|---|---|
| `Source/HuoguoHuoguo/` (루트) | 모듈 정의, 로그 카테고리, Build.cs | `Docs/Source/Module.md` |
| `Source/HuoguoHuoguo/Game/` | 게임 모드, 컨트롤러, GameInstance | `Docs/Source/Game.md` |
| `Source/HuoguoHuoguo/Player/` | 경영 카메라 Pawn, 탐험 캐릭터 | `Docs/Source/Player.md` |
| `Source/HuoguoHuoguo/World/` | 맵에 배치하는 표식/발판 액터 | `Docs/Source/World.md` |
| `Source/HuoguoHuoguo/Planet/` | 행성 시드, 절차적 행성 생성기 | `Docs/Source/Planet.md` |
| `Source/HuoguoHuoguo/Debug/` | 개발용 도우미 | `Docs/Source/Debug.md` |
| `Content/HG/Maps/` | 맵 에셋 | `Docs/Content/Maps.md` |
| `Content/HG/Materials/` | 머티리얼 | `Docs/Content/Materials.md` |
| `Tools/` | 빌드, 맵 생성, 문서 색인 스크립트 | `Docs/Tools/Tools.md` |
| `Config/`, `.uproject`, `.gitignore` | 엔진/프로젝트 설정 | `Docs/Project/Config.md` |

## 아직 없는 것 (다음 후보)

- 다이빙 연출 (포드 낙하, 행성이 커지는 장면, 대기권 섬광 뒤 로딩)
- 정거장 창밖으로 접근 행성을 실제 구체로 보여 주기
- 경영 카메라 회전 시 벽 자동 낮추기 (벽이 시야를 가림)
- 재료 채집, 인벤토리, 손님/주문/요리 등 경영 루프 전체
- 입력을 `IA_`/`IMC_` 에셋으로 전환, 정식 UI(UMG), 자동화 테스트(`Tests/`), Git LFS
