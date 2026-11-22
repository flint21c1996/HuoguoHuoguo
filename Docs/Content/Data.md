# Data - 밸런스 데이터 (CSV 원본과 DataTable 에셋)

> 경로: `Data/` (CSV 원본), `Content/HG/Data/` (DataTable 에셋) · 마지막 갱신: 2026-11-23

## 역할

재료 같은 밸런스 데이터를 텍스트(CSV)로 관리한다. 원본은 저장소의 `Data/*.csv`이고, 게임은 그것을 읽어 들인 DataTable 에셋을 쓴다.
텍스트라서 diff로 바뀐 값을 보고, 사람과 AI가 쉽게 고칠 수 있다.

## 파일

### Data/Ingredients.csv - 재료
- 열: `Name`(행 이름 = 재료 ID), `DisplayName`, `Color`(`"(R=,G=,B=,A=)"`), `Price`, `HomeBiome`(`Rock`/`Ice`/`Fungal`). 행 구조체는 `FHGIngredientRow` (`Docs/Source/Game.md`).
- 지금 재료: `SporeMushroom` 포자버섯(보라, 버섯숲), `FrostKelp` 서리다시마(청록, 얼음), `EmberChili` 불씨고추(빨강, 바위).
- UTF-8 BOM으로 저장한다 (한국어).

### Content/HG/Data/DT_Ingredients - 재료 DataTable
- `Tools/GenerateGreyboxMaps.py`의 `ensure_data_tables()`가 `Data/Ingredients.csv`로 만들고 채운다. CSV와 내용이 같으면 저장하지 않는다.
- GameInstance가 소프트 참조로 불러온다. 행성 생성기는 행 순서대로 재료 표식 종류를 만든다.

## 흐름 / 규칙

- 값을 바꾸거나 재료를 늘릴 때: CSV를 고친다 → 에디터를 닫고 `Tools/Invoke-HGMapGen.ps1` → 실행.
- DataTable을 에디터에서 직접 고치지 않는다. 다음 실행 때 CSV 내용으로 덮어써진다.
- 행 이름(ID)은 가방과 캔 기록이 가리키므로 함부로 바꾸지 않는다. 행 순서를 바꾸면 같은 시드 행성의 재료 배치가 바뀐다.

## 알려진 문제와 다음 할 일

- 손님 종족, 요리법, 업그레이드 표는 아직 없다.
- `Price`는 아직 쓰지 않는다 (요리/판매가 생기면 쓴다).
