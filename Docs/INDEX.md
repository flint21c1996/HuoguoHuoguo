# 문서 색인

> 자동 생성 파일이다. 손으로 고치지 말고 문서를 고친 뒤 `Tools/Update-HGDocsIndex.ps1`을 실행한다.
> 각 문서의 제목과 소제목만 모았다. 작업할 폴더의 문서를 골라 본문을 읽은 뒤, 실제 코드로 확인한다.

## Project

### [프로젝트 개요 - 우주 정거장 훠궈집 경영 + 절차적 행성 파밍](Project/Overview.md)

- 한 줄 요약
- 현재 플레이 흐름 (구현됨)
- 핵심 설계 원칙
- 폴더 지도
- 기획 방향 (논의 중)
- 아직 없는 것 (다음 후보)

### [프로젝트 설정 - uproject, Config, gitignore](Project/Config.md)

- HuoguoHuoguo.uproject
- Config/DefaultEngine.ini
- .gitignore
- 알려진 문제와 다음 할 일

### [기획서 - 사용자가 말한 방향과 결정 모음](Project/Design.md)

- 역할
- 게임 한 줄 소개
- 큰 구조
  - 경영 파트와 탐험 파트는 완전히 분리 [구현됨]
  - 행성은 둥글고 절차적으로 만든다 [구현됨]
  - 하루 구조 [구현됨 기본, 사용자 결정]
- 경영 파트
  - 시점과 조작 [구현됨]
  - 가구 배치 [구현됨]
  - 가구 종류를 많이 [구현됨 기본, 사용자 요청]
  - 경영의 중심 = 칸 배치와 콤보 [결정, 사용자 결정 / 첫 시제품 구현됨]
  - 훠궈다운 영업 [결정]
  - 고른 경영 시스템 [결정]
  - 손님이 주는 정보 [결정, 사용자 제안]
  - 더 넣을 것 [아이디어, 사용자가 좋다고 함]
  - 사장님 역할 [일부 결정]
- 탐험 파트
  - 행성 걷기 [구현됨]
  - 재료 채집 [구현됨]
  - 전투 [아이디어]
  - 미니맵 [구현됨]
  - 큰 지도 [아이디어]
- 가방 [구현됨 일부]
- 하지 않기로 한 것
- 작업 방식에 대한 요청
- 정해야 할 것 목록
- 알려진 문제와 다음 할 일

### [참고 게임 분석 - 경영/채집 게임 비교와 훠궈훠궈 방향 후보](Project/GameReferences.md)

- 역할
- 게임별 요약
- 재미 요소별 정리
- 정해야 할 질문
  - 사장님 역할
  - 탐험 중 가게 시간
- 방향 후보 (추천안, 미확정)
  - 형태
  - 데이브 더 다이버와의 차별점
  - 핵심 반복
  - 구현 순서 후보
- 고른 시스템 (2026-11-23 사용자 선택)
  - 궤도가 손님을 데려온다
  - 손님이 메뉴를 발명한다
  - 김으로 읽는 가게
  - 정거장 환경 구역
  - 손님이 주는 정보 (사용자 제안)
  - 전체 순환
  - 뺀 것
- 알려진 문제와 다음 할 일

## Source

### [Debug - 개발용 도우미](Source/Debug.md)

- 파일
  - HGPlaceholderPaint (.h/.cpp)
- 알려진 문제와 다음 할 일

### [Explore - 탐험 파트 (3인칭으로 행성을 걷고 재료를 캐고 미니맵을 본다)](Source/Explore.md)

- 역할
- 파일
  - HGExploreGameMode (.h/.cpp)
  - HGExplorerCharacter (.h/.cpp)
  - HGExplorePlayerController (.h/.cpp)
  - HGPlanetMinimap (.h/.cpp)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

### [Game - 두 파트의 공통 부모(게임 모드, 컨트롤러), GameInstance, 재료 데이터 구조](Source/Game.md)

- 역할
- 파일
  - HGGameModeBase (.h/.cpp)
  - HGPlayerControllerBase (.h/.cpp)
  - HGGameInstance (.h/.cpp)
  - HGDayTypes.h
  - HGIngredientTypes.h
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

### [Player - 사장님 캐릭터 공통 몸체](Source/Player.md)

- 역할
- 파일
  - HGBossCharacter (.h/.cpp)
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

### [Shop - 경영 파트 (아이소 시점, 칸 배치와 콤보, 손님)](Source/Shop.md)

- 역할
- 파일
  - HGShopGameMode (.h/.cpp)
  - HGShopBossCharacter (.h/.cpp)
  - HGShopPlayerController (.h/.cpp)
  - HGFurnitureData.h
  - HGFurniture (.h/.cpp)
  - HGShopGrid (.h/.cpp)
  - HGShopCombos (.h/.cpp)
  - HGFurniturePlacer (.h/.cpp)
  - HGCustomer (.h/.cpp)
  - HGCustomerDirector (.h/.cpp)
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

### [Data - 밸런스 데이터 (CSV 원본과 DataTable 에셋)](Content/Data.md)

- 역할
- 파일
  - Data/Ingredients.csv - 재료
  - Data/Furniture.csv - 가구 목록 (엑셀로 열어 고친다)
  - Data/Combos.csv - 배치 콤보
  - Content/HG/Data/DT_Furniture - 가구 DataTable
  - Content/HG/Data/DT_Combos - 콤보 DataTable
  - Content/HG/Data/DT_Ingredients - 재료 DataTable
- 흐름 / 규칙
- 알려진 문제와 다음 할 일

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
  - GenerateGreyboxMaps.py - 맵/머티리얼/데이터 표 생성 내용
  - Update-HGDocsIndex.ps1 - 문서 색인 생성
- 기타 명령
- 알려진 문제와 다음 할 일
