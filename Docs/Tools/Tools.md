# Tools - 빌드, 맵 생성, 문서 색인 스크립트

> 경로: `Tools/` · 마지막 갱신: 2026-11-23

## 스크립트

### Invoke-HGBuild.ps1 - C++ 빌드
- `Build.bat HuoguoHuoguoEditor Win64 Development -Project=… -WaitMutex`. `-Configuration DebugGame` 가능.
- 에디터가 켜져 있고 Live Coding이 활성이면 UBT가 거부한다. 에디터를 닫거나 에디터에서 Live Coding으로 컴파일한다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Invoke-HGBuild.ps1
```

### Invoke-HGMapGen.ps1 - 블록아웃 맵 생성
- `UnrealEditor-Cmd -run=pythonscript -script=Tools/GenerateGreyboxMaps.py -unattended -nullrhi`로 에디터 없이 실행.
- HuoguoHuoguo 에디터가 열려 있으면 거부한다. `-Force`면 기존 맵을 지우고 다시 만든다 (에디터에서 고친 배치가 사라짐).

### GenerateGreyboxMaps.py - 맵/머티리얼 생성 내용
- `MI_GB_*` 머티리얼, `L_Station`, `L_Asteroid_Krak7`, `L_PlanetSurface`를 만든다. 이미 있으면 건너뛴다.
- `upgrade_station()`: 이미 있는 정거장에서 에어록 발판을 다이빙 발판으로, `FromAsteroid` 시작점을 `FromPlanet`으로 값만 바꾼다. 바뀐 것이 없으면 저장하지 않는다.
- 파이썬에서 C++ 속성 이름은 `b` 접두사를 뺀 snake_case다 (`bDiveToSelectedPlanet` → `dive_to_selected_planet`).

### Update-HGDocsIndex.ps1 - 문서 색인 생성
- `Docs/` 아래 모든 `.md`(INDEX.md 제외)의 `#`, `##`, `###` 제목만 모아 `Docs/INDEX.md`를 다시 쓴다. 코드 블록 안의 `#`은 무시한다.
- 문서를 고친 뒤 반드시 실행한다. `INDEX.md`는 손으로 고치지 않는다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Update-HGDocsIndex.ps1
```

## 기타 명령

- VS 프로젝트 파일 재생성 (새 소스 파일이 VS에 안 보일 때):
  `E:\UE_5.8\Engine\Build\BatchFiles\Build.bat -projectfiles -project="E:\HuoguoHuoguo\HuoguoHuoguo.uproject" -game -engine`
- 에디터 없이 실행:
  `UnrealEditor.exe "E:\HuoguoHuoguo\HuoguoHuoguo.uproject" [맵 경로] -game -windowed -ResX=1280 -ResY=720 -log -ExecCmds="HGDive 1"`

## 알려진 문제와 다음 할 일

- 스모크 테스트(실행 → 로그 확인 → 게임 창만 캡처) 스크립트는 아직 저장소에 없다. 필요하면 `Tools/`로 옮긴다.
