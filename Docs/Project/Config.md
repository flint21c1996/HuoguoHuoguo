# 프로젝트 설정 - uproject, Config, gitignore

> 대상: `HuoguoHuoguo.uproject`, `Config/`, `.gitignore` · 마지막 갱신: 2026-11-26

## HuoguoHuoguo.uproject

- 엔진: `5.8` (설치 위치 `E:\UE_5.8`)
- 모듈: `HuoguoHuoguo` (Runtime)
- 켠 플러그인
  - `ModelingToolsEditorMode` (에디터 전용, 템플릿 기본)
  - `PythonScriptPlugin`, `EditorScriptingUtilities`: `Tools/GenerateGreyboxMaps.py`를 커맨드렛으로 실행하기 위해
  - `ProceduralMeshComponent`: 행성 지형 메시를 런타임에 만들기 위해

## Config/DefaultEngine.ini

`[/Script/EngineSettings.GameMapsSettings]`
- `GameDefaultMap`, `EditorStartupMap` = `/Game/HG/Maps/L_Station`
- `GlobalDefaultGameMode` = `/Script/HuoguoHuoguo.HGExploreGameMode` (맵에 지정이 없을 때만 쓴다. 실제 파트는 맵 World Settings가 정한다)
- `GameInstanceClass` = `/Script/HuoguoHuoguo.HGGameInstance`

`[/Script/Engine.RendererSettings]`
- `r.CustomDepth=3`: 커스텀 깊이 + 스텐실. 그림체 비교 윤곽선에서 행성 외계 식물을 빼는 데 스텐실 1을 쓴다 (`Docs/Content/Materials.md`). 바꾸면 에디터를 다시 켜야 적용된다.

주의: 템플릿이 만든 AndroidFileServer `SecurityToken` 값이 들어 있다. 안드로이드 빌드를 하지 않으면 영향 없다.

## .gitignore

`Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `.vs`, `*.sln`, `*.slnx`, `.vsconfig`, `.claude/settings.local.json`을 제외한다.
VS 솔루션은 `Build.bat -projectfiles`로 다시 만든다 (`Docs/Tools/Tools.md` 참고).

## 알려진 문제와 다음 할 일

- 맵/에셋이 바이너리라 아트 에셋이 늘기 전에 Git LFS 설정이 필요하다.
