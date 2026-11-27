# Tools - 빌드, 맵 생성, 문서 색인 스크립트

> 경로: `Tools/` · 마지막 갱신: 2026-11-28

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

### GenerateGreyboxMaps.py - 맵/머티리얼/데이터 표 생성 내용
- `ensure_data_tables()`: `DATA_TABLES` 목록의 CSV(`Data/*.csv`)를 DataTable(`/Game/HG/DataTable/DT_*`)로 읽어 들인다 (지금 `DT_Ingredients`, `DT_Combos`, `DT_Furniture`). 에셋이 없으면 만들고, 있으면 CSV로 다시 채운 뒤 내용이 바뀌었을 때만 저장한다 (`Docs/Content/Data.md`).
- `MI_GB_*` 머티리얼, `L_Station`, `Legacy/L_Asteroid_Krak7`, `Legacy/L_PlanetSurface`를 만든다. 이미 있으면 건너뛴다.
- 이미 있는 맵은 열지도 고치지도 않는다 (2026-11-27). 맵은 에디터에서 사람이 고친다. 환경별 맵(`L_Planet_Small` 등)은 이 도구가 모른다.
- `ensure_glow_material()`: 발광 머티리얼 `M_HG_Glow`를 만든다. 이미 있으면 인스턴스 메시 사용 표시만 켠다 (`Docs/Content/Materials.md`).
- `ensure_planet_materials()`: 행성 그림체용 `M_HG_PlanetSky`(하늘 돔, HLSL은 `SKY_HLSL`)와 `M_HG_VertexColor`(각진 지형)를 만든다. 하늘 머티리얼이 이미 있으면 하늘(Is Sky) 표시만 켠다 (`Docs/Content/Materials.md`).
- `ensure_landscape_material()` (2026-11-27): 평지 행성 랜드스케이프 머티리얼 `M_HG_LandscapeBasin`을 없을 때만 만든다 (`Docs/Content/Materials.md`).
- `ensure_scan_material()`: 스캐너 파동 화면 효과 `M_HG_ScanPulse`(포스트 프로세스)를 만든다. 이 도구만 만드는 효과라 판 번호 `ScanVersion`이 `SCAN_VERSION`과 다르면 지우고 다시 만든다 (같으면 건너뛴다). 이웃 파라미터는 4칸이라 RGBA 출력에 잇는다 (RGB에 이었다가 셰이더 컴파일이 실패했다).
- `ensure_look_materials()`: 그림체 비교 화면 효과 `M_HG_LookToon`(셀 셰이딩, 톤 매핑 전)을 만든다. HLSL은 `TOON_HLSL`. 판 번호 `LookVersion`이 `LOOK_VERSION`(지금 8)과 다르면 지우고 다시 만든다. Custom 노드에서 깊이와 GBuffer를 읽으려면 그 화면 텍스처 노드(SceneTexture)를 Custom 입력에 이어 둬야 엔진이 텍스처를 묶는다.
- `ensure_fixed_exposure(label, ev)`: 맵 전체 노출을 고정하는 포스트 프로세스 볼륨. 없을 때만 만들고, 있으면 에디터에서 고친 값을 존중해 건드리지 않는다. 정거장은 `STATION_EXPOSURE_EV`(2.0). 값을 맞춰 볼 때는 환경 변수 `HG_STATION_EXPOSURE`를 주고 실행하면 그 값으로 덮어쓴다.
- 새 맵에 놓는 게임 액터 (2026-11-28): C++ 클래스 대신 BP(`ACTOR_BPS`: 발판, 생성기, 카메라 기준점, 가구)를 놓는다. BP가 없으면 C++ 클래스. 시작점은 엔진 PlayerStart(`player_start`, Player Start Tag). 예전 `HGExplorerStart`는 지웠다.
- `MAP_GAME_MODES`: 맵마다 World Settings GameMode를 정한다 (L_Station = 경영, 나머지 = 탐험). 새 맵을 만들 때와 이미 있는 맵 모두에 적용한다.
  값은 (C++ 클래스, 블루프린트 경로). `/Game/HG/BluePrint/Game`의 게임 모드 BP가 있으면 BP를, 없으면 C++ 클래스를 넣는다. BP 자체는 만들거나 고치지 않는다.
- `upgrade_station()` / `upgrade_existing_map()`: (2026-11-27부터 부르지 않음) 예전에 이미 있는 맵의 값(게임 모드, 에어록 발판, 시작점, 홀 가구)을 새 규칙으로 바꾸던 함수. 손으로 고친 맵을 덮어쓸 수 있어서 기록으로만 남긴다.
- 예외로 액터를 지우는 경우 하나: 정거장에 가구 액터(`HGFurniture`)가 하나도 없고 홀 식탁이 예전 도형 묶음(`Table_N_Top/Leg/Pot/Seat`)이면, 그 도형들을 지우고 `place_hall_furniture()`로 식탁 4 + 의자 4 가구 액터를 놓는다. 가구 액터가 하나라도 있으면 건드리지 않는다.
- 파이썬에서 C++ 속성 이름은 `b` 접두사를 뺀 snake_case다 (`bDiveToSelectedPlanet` → `dive_to_selected_planet`).

### Update-HGDocsIndex.ps1 - 문서 색인 생성
- `Docs/` 아래 모든 `.md`(INDEX.md 제외)의 `#`, `##`, `###` 제목만 모아 `Docs/INDEX.md`를 다시 쓴다. 코드 블록 안의 `#`은 무시한다.
- 문서를 고친 뒤 반드시 실행한다. `INDEX.md`는 손으로 고치지 않는다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Update-HGDocsIndex.ps1
```

## 에디터 도구

- Planet Maker (에디터 창, C++ 에디터 모듈): `Docs/Tools/PlanetMaker.md`

## 기타 명령

- VS 프로젝트 파일 재생성 (새 소스 파일이 VS에 안 보일 때):
  `E:\UE_5.8\Engine\Build\BatchFiles\Build.bat -projectfiles -project="E:\HuoguoHuoguo\HuoguoHuoguo.uproject" -game -engine`
- 에디터 없이 실행:
  `UnrealEditor.exe "E:\HuoguoHuoguo\HuoguoHuoguo.uproject" [맵 경로] -game -windowed -ResX=1280 -ResY=720 -log -ExecCmds="HGDive 1"`

## 알려진 문제와 다음 할 일

- 스모크 테스트(실행 → 로그 확인 → 게임 창만 캡처) 스크립트는 아직 저장소에 없다. 필요하면 `Tools/`로 옮긴다.
  여러 단계 시험은 `-ExecCmds="HGSeq 명령|명령|..."`으로 한 번에 넣는다 (`Docs/Source/Game.md`).
