# 문서 색인

> 자동 생성 파일이다. 손으로 고치지 말고 문서를 고친 뒤 `Tools/Update-HGDocsIndex.ps1`을 실행한다.
> 각 문서의 제목과 소제목만 모았다. 작업할 폴더의 문서를 골라 본문을 읽은 뒤, 실제 코드로 확인한다.

## Project

### [프로젝트 개요 - 우주 정거장 훠궈집 경영 + 절차적 행성 파밍](Project/Overview.md)

- 한 줄 요약
- 현재 플레이 흐름 (구현됨)
- 핵심 설계 원칙
- 폴더 지도
- 아직 없는 것 (다음 후보)

### [프로젝트 설정 - uproject, Config, gitignore](Project/Config.md)

- HuoguoHuoguo.uproject
- Config/DefaultEngine.ini
- .gitignore
- 알려진 문제와 다음 할 일

## Source

### [Debug - 개발용 도우미](Source/Debug.md)

- 파일
  - HGPlaceholderPaint (.h/.cpp)
- 알려진 문제와 다음 할 일

### [Game - 게임 모드, 플레이어 컨트롤러, GameInstance](Source/Game.md)

- 역할
- 파일
  - HGGameMode (.h/.cpp)
  - HGPlayerController (.h/.cpp)
  - HGGameInstance (.h/.cpp)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

### [모듈 루트 - Build.cs와 로그 카테고리](Source/Module.md)

- 파일
  - HuoguoHuoguo.Build.cs
  - HuoguoHuoguo.h / .cpp
- 규칙

### [Planet - 행성 시드와 절차적 행성 생성기](Source/Planet.md)

- 역할
- 파일
  - HGPlanetTypes (.h/.cpp)
  - HGPlanetGenerator (.h/.cpp)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

### [Player - 경영 카메라 Pawn과 탐험 캐릭터](Source/Player.md)

- 역할
- 파일
  - HGShopCameraPawn (.h/.cpp)
  - HGExplorerCharacter (.h/.cpp)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

### [World - 맵에 배치하는 표식과 발판 액터](Source/World.md)

- 역할
- 파일
  - HGShopCameraAnchor (.h/.cpp)
  - HGExplorerStart (.h/.cpp)
  - HGTravelPad (.h/.cpp)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

## Content

### [Maps - 맵 에셋](Content/Maps.md)

- 역할
- 맵
  - L_Station - 허브 정거장 훠궈집
  - L_PlanetSurface - 절차적 행성 표면
  - L_Asteroid_Krak7 - 이전 손배치 소행성 (연결 끊김)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

### [Materials - 머티리얼 에셋](Content/Materials.md)

- Greybox
  - MI_GB_* (15개)
- 알려진 문제와 다음 할 일

## Tools

### [Tools - 빌드, 맵 생성, 문서 색인 스크립트](Tools/Tools.md)

- 스크립트
  - Invoke-HGBuild.ps1 - C++ 빌드
  - Invoke-HGMapGen.ps1 - 블록아웃 맵 생성
  - GenerateGreyboxMaps.py - 맵/머티리얼 생성 내용
  - Update-HGDocsIndex.ps1 - 문서 색인 생성
- 기타 명령
- 알려진 문제와 다음 할 일
