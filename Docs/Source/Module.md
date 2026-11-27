# 모듈 루트 - Build.cs와 로그 카테고리

> 경로: `Source/HuoguoHuoguo/` (하위 폴더 제외) · 마지막 갱신: 2026-11-26

## 파일

### HuoguoHuoguo.Build.cs
- `PublicIncludePaths.Add(ModuleDirectory)`: `#include "Player/HGPlayerControllerBase.h"`처럼 모듈 루트 기준 하위 폴더 경로로 include한다.
- 공개 의존: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `DeveloperSettings` (프로젝트 설정 클래스 `UHGLookSettings`)
- 비공개 의존: `ProceduralMeshComponent` (행성 지형), `UMG`, `Slate`, `SlateCore` (미니맵 등 화면 UI)

### HuoguoHuoguo.h / .cpp
- 로그 카테고리 `LogHuoguo` 선언/정의. 게임 코드의 로그는 모두 이 카테고리를 쓴다.
- 스모크 테스트는 로그 파일에서 `LogHuoguo:` 줄을 찾아 상태를 확인한다.

## 규칙

- 클래스 접두사는 `HG`. 새 하위 폴더를 만들면 `Docs/Source/<폴더>.md`도 만든다.
- 새 C++ 파일은 UTF-8 BOM으로 저장한다 (MSVC가 한국어 주석을 올바르게 읽도록).
