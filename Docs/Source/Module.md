# 모듈 루트 - Build.cs와 로그 카테고리

> 경로: `Source/HuoguoHuoguo/`, `Source/HuoguoHuoguoEditor/` (하위 폴더 제외) · 마지막 갱신: 2026-11-27

## 파일

### HuoguoHuoguo.Build.cs
- `PublicIncludePaths.Add(ModuleDirectory)`: `#include "Player/HGPlayerControllerBase.h"`처럼 모듈 루트 기준 하위 폴더 경로로 include한다.
- 공개 의존: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `DeveloperSettings` (프로젝트 설정 클래스 `UHGLookSettings`)
- 비공개 의존: `ProceduralMeshComponent` (행성 지형), `UMG`, `Slate`, `SlateCore` (미니맵 등 화면 UI), `Json`, `JsonUtilities` (행성 프리셋 JSON, 2026-11-27)

### HuoguoHuoguo.h / .cpp
- 로그 카테고리 `LogHuoguo` 선언/정의. 게임 코드의 로그는 모두 이 카테고리를 쓴다.
- 스모크 테스트는 로그 파일에서 `LogHuoguo:` 줄을 찾아 상태를 확인한다.

### HuoguoHuoguoEditor 모듈 (2026-11-27)
- 에디터 전용 모듈 (`.uproject`에 `Type: Editor`, `LoadingPhase: PostEngineInit`, `HuoguoHuoguoEditor.Target.cs`의 `ExtraModuleNames`). 게임 빌드에는 들어가지 않는다.
  UnderOneUmbrella의 `<프로젝트>Editor` 모듈과 같은 구성: `Source/HuoguoHuoguoEditor/{Build.cs, Private/<기능>/}`.
- 의존: `Core`, `CoreUObject`, `Engine`, `InputCore`, `Slate`, `SlateCore`, `UnrealEd`, `ToolMenus`, `PropertyEditor`, `DesktopPlatform`, `MainFrame`, `HuoguoHuoguo`
- `Private/HuoguoHuoguoEditorModule.cpp`: 도구 창 등록. Planet Maker 탭, 메뉴 **툴 → 훠궈훠궈 → Planet Maker**, 콘솔 명령 `hg.PlanetMaker`.
- `Private/PlanetMaker/SHGPlanetMaker`: Planet Maker 창 (`Docs/Tools/PlanetMaker.md`).

## 규칙

- 클래스 접두사는 `HG`. 새 하위 폴더를 만들면 `Docs/Source/<폴더>.md`도 만든다.
- 새 C++ 파일은 UTF-8 BOM으로 저장한다 (MSVC가 한국어 주석을 올바르게 읽도록).
- 게임에 들어가는 값과 타입(예: `FHGPlanetPreset`)은 게임 모듈에, 창과 메뉴는 에디터 모듈에 둔다. 나중에 게임이 같은 JSON을 읽을 수 있게 하기 위해서다.
