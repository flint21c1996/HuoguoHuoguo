# Data - 밸런스 데이터 (CSV 원본과 DataTable 에셋)

> 경로: `Data/` (CSV 원본), `Content/HG/Data/` (DataTable 에셋) · 마지막 갱신: 2026-11-24

## 역할

재료 같은 밸런스 데이터를 텍스트(CSV)로 관리한다. 원본은 저장소의 `Data/*.csv`이고, 게임은 그것을 읽어 들인 DataTable 에셋을 쓴다.
텍스트라서 diff로 바뀐 값을 보고, 사람과 AI가 쉽게 고칠 수 있다.

## 파일

### Data/Ingredients.csv - 재료
- 열: `Name`(행 이름 = 재료 ID), `DisplayName`, `Color`(`"(R=,G=,B=,A=)"`), `Price`, `HomeBiome`(`Rock`/`Ice`/`Fungal`), `AvailableAt`(`Day`/`Night`/`Any`, 행성에서 보이는 시간대). 행 구조체는 `FHGIngredientRow` (`Docs/Source/Game.md`).
- 지금 재료 (군계마다 낮 하나, 밤 하나)

| 군계 | 낮 | 밤 |
|---|---|---|
| 버섯숲 `Fungal` | `SporeMushroom` 포자버섯 (보라) | `GlowCap` 야광버섯 (연두) |
| 얼음 `Ice` | `FrostKelp` 서리다시마 (청록) | `MoonLotus` 달빛연근 (흰빛 하늘) |
| 바위 `Rock` | `EmberChili` 불씨고추 (빨강) | `CinderPotato` 숯불감자 (주황) |

- UTF-8 BOM으로 저장한다 (한국어).

### Data/Furniture.csv - 가구 목록 (엑셀로 열어 고친다)
- 한 줄 = 가구 한 종류. 행 순서가 배치 모드에서 고르는 순서(분류마다 위에서 아래로)다. 행 구조체는 `FHGFurnitureRow` (`Docs/Source/Shop.md`).
- 열

| 열 | 뜻 |
|---|---|
| `Name` | 가구 ID (배치 기록, 콤보가 가리킨다). 바꾸지 않는다. `Table`, `Chair`는 맵에 놓인 옛 가구가 쓰는 ID라 지우지 않는다 |
| `DisplayName` | 화면 이름 |
| `Category` | `Table`(식탁) / `Chair`(의자) / `Decor`(장식) |
| `Style` | 생김새. 식탁 `Round`/`Square`, 의자 `Stool`/`Wood`/`Cushion`/`Arm`, 장식 `Bush`/`Bamboo`/`Lantern`/`PaperLantern`/`SauceBar`/`Tank`/`Screen`. 모르는 값이면 회색 상자 |
| `Pot` | 식탁의 훠궈 냄비: `Single`(단일) / `YinYang`(원앙, 홍탕/백탕 반반) / `NineGrid`(구궁격 9칸) / `Big`(큰 냄비). 식탁이 아니면 비운다 |
| `SizeX`, `SizeY` | 차지하는 칸 (회전 0도 기준) |
| `Price` | 하나 놓을 때마다 내는 값. 치우면 절반을 돌려받는다. 옮기기는 공짜 |
| `Unlock` | 여는 방법: `Start`(처음부터) / `Money`(U로 돈을 내고) / `Customers`(손님 수가 차면 저절로) / `Ingredient`(U로 재료를 내고) |
| `UnlockAmount` | 여는 조건의 수: 돈, 손님 수, 재료 개수 |
| `UnlockIngredient` | `Ingredient`일 때 내는 재료 ID (`Data/Ingredients.csv`의 `Name`) |
| `BonusPercent` | 손님 값에 더하는 %. 식탁은 그 식탁 손님 모두, 의자는 앉은 손님. 장식은 0 (콤보로 더한다) |
| `Tags` | 콤보용 꼬리표, `\|`로 여러 개 (`Plant`, `Light`, `Sauce`, `Water`, `Art`, `Planet`) |
| `ColorA`, `ColorB` | 주 색 / 보조 색 `#RRGGBB` (식탁: 상판/냄비, 의자: 몸체/방석, 화분: 잎/화분, 등: 등/기둥) |
| `ActorClass` | 블루프린트 아트 클래스 경로 (예: `/Game/HG/Furniture/BP_TableYinYang.BP_TableYinYang_C`). 비우면 기본 도형 |
| `Description` | 설명 한 줄 (가구 목록 화면이 생기면 쓴다) |

- 처음 열린 가구는 원목 원탁, 나무 의자, 화분 3종이고, 처음 가진 돈은 300이다 (GameInstance).
- 지금 가구 19종

| 분류 | 가구 (ID, 칸, 값, 보너스) |
|---|---|
| 식탁 | 원목 원탁 `Table` 2x2 100 · 원앙 냄비 원탁 `TableYinYang` 2x2 180 +10% · 구궁격 사각탁 `TableNineGrid` 2x2 260 +15% · 구리 솥 원탁 `TableCopper` 2x2 320 +20% · 2인 사각탁 `TableDuo` 2x1 70 · 연회 대원탁 `TableBanquet` 3x3 400 +20% |
| 의자 | 둥근 의자 `Stool` 20 · 나무 의자 `Chair` 40 +5% · 방석 의자 `CushionChair` 70 +10% · 팔걸이 의자 `ArmChair` 120 +15% · 둥실 우주 의자 `SpaceChair` 150 +20% (Planet) |
| 장식 | 화분 `Planter` (Plant) · 대나무 화분 `Bamboo` (Plant) · 야광버섯 화분 `GlowShroomPot` (Plant, Planet) · 등불 `Lantern` (Light) · 홍등 `RedLantern` (Light) · 소스 바 `SauceBar` 2x1 (Sauce) · 수조 `FishTank` 2x1 (Water) · 병풍 `Screen` 2x1 (Art) |

- 여는 조건 (지금 값)
  - 처음부터: 원목 원탁, 나무 의자, 화분
  - 돈: 2인 사각탁 100, 원앙 냄비 원탁 250, 구궁격 사각탁 500, 구리 솥 원탁 800, 둥근 의자 30, 방석 의자 150, 팔걸이 의자 350, 대나무 100, 등불 80, 소스 바 200, 병풍 300
  - 손님 수: 홍등 10명, 둥실 우주 의자 25명, 연회 대원탁 40명
  - 행성 재료: 야광버섯 화분 = 야광버섯(GlowCap) 3개, 수조 = 서리다시마(FrostKelp) 3개
- 엑셀에서 저장할 때는 "CSV UTF-8(쉼표로 분리)" 형식을 고른다. 그냥 "CSV"로 저장하면 한국어가 깨진다. 쉼표가 든 칸은 엑셀이 알아서 따옴표로 감싼다.

### Data/Combos.csv - 배치 콤보
- 열: `Name`(행 이름 = 콤보 ID), `DisplayName`, `Needs`(식탁 둘레에 있어야 하는 가구 ID나 꼬리표), `BonusPercent`(그 식탁 손님이 더 내는 %). 행 구조체는 `FHGComboRow` (`Docs/Source/Shop.md`).
- `Needs` 형식: 가구 ID(`Bamboo`)나 꼬리표(`Plant`)를 `+`로 잇고, 개수는 `*숫자`. 예: `Plant`, `Plant+Light`, `Plant*3`.
- 판정: 식탁 둘레 한 칸(대각선 포함)에 걸친 가구를 ID별, 꼬리표별로 센다. 의자도 센다 (우주 의자의 Planet). 한 식탁에 성립한 콤보의 보너스는 모두 더한다.
- 지금 콤보

| ID | 이름 | 필요 | 보너스 |
|---|---|---|---|
| `GreenNook` | 초록 쉼터 | Plant | +20% |
| `LanternGlow` | 등불 아래 | Light | +20% |
| `GardenLantern` | 정원 등불석 | Plant + Light | +30% |
| `SauceMaster` | 소스 장인석 | Sauce | +25% |
| `WaterView` | 물멍석 | Water | +20% |
| `ArtCorner` | 풍류석 | Art + Light | +25% |
| `ForestTable` | 숲속 식탁 | Plant 3개 | +30% |
| `PlanetScent` | 행성 향기 | Planet | +15% |

- 예: 대나무 화분 + 홍등 + 야광버섯 화분을 한 식탁에 붙이면 초록 쉼터, 등불 아래, 정원 등불석, 행성 향기가 함께 성립해 +85%다.
- UTF-8 BOM으로 저장한다.

### Content/HG/Data/DT_Furniture - 가구 DataTable
- `ensure_data_tables()`가 `Data/Furniture.csv`로 만든다. 가구 액터(생성 스크립트)와 배치 컴포넌트가 경로로 불러온다 (`HGFurnitureData::LoadTable`).
### Content/HG/Data/DT_Combos - 콤보 DataTable
- `ensure_data_tables()`가 `Data/Combos.csv`로 만든다. GameInstance가 소프트 참조로 불러온다 (`GetComboTable`).

### Content/HG/Data/DT_Ingredients - 재료 DataTable
- `Tools/GenerateGreyboxMaps.py`의 `ensure_data_tables()`가 `Data/Ingredients.csv`로 만들고 채운다. CSV와 내용이 같으면 저장하지 않는다.
- GameInstance가 소프트 참조로 불러온다. 행성 생성기는 행 순서대로 재료 표식 종류를 만든다.

## 흐름 / 규칙

- 값을 바꾸거나 재료/가구/콤보를 늘릴 때: CSV를 고친다 → 에디터를 닫고 `Tools/Invoke-HGMapGen.ps1` → 실행.
- DataTable을 에디터에서 직접 고치지 않는다. 다음 실행 때 CSV 내용으로 덮어써진다.
- 행 이름(ID)은 가방과 캔 기록이 가리키므로 함부로 바꾸지 않는다. 행 순서를 바꾸면 같은 시드 행성의 재료 배치가 바뀐다.

## 알려진 문제와 다음 할 일

- 손님 종족, 요리법, 업그레이드 표는 아직 없다.
- 가구 값과 여는 값은 처음 정한 값이다. 사람이 놀아 보고 맞춘다 (손님 한 명이 기본 꼬치로 15~30을 낸다).
- 재료 `Price`는 손님이 꼬치 하나를 먹을 때 내는 값이다 (가방이 비면 기본 꼬치 5).
