# Blueprints - 기획/아트가 직접 고치는 클래스 설정

> 경로: `Content/HG/Blueprints/`, `Content/HG/Characters/` · 마지막 갱신: 2026-11-27

## 역할

C++는 기능과 규칙을 맡고, "어떤 모델/값을 쓸지"는 블루프린트 하위 클래스의 디테일 패널에서 정한다.
코드를 고치거나 빌드하지 않고 에디터에서 바꿀 수 있게 하려는 것이다.
도구(`Tools/GenerateGreyboxMaps.py`)는 이 블루프린트를 만들거나 고치지 않고, 맵이 가리키게만 한다.

## 연결 순서

맵 World Settings의 GameMode Override → 게임 모드 BP → 컨트롤러 BP → 사장님 BP.

| 블루프린트 | 부모 (C++) | 여기서 정하는 것 |
|---|---|---|
| `BP_HG_ShopGameMode` | `AHGShopGameMode` | Player Controller Class = `BP_HG_ShopPlayerController` |
| `BP_HG_ExploreGameMode` | `AHGExploreGameMode` | Player Controller Class = `BP_HG_ExplorePlayerController` |
| `BP_HG_ShopPlayerController` | `AHGShopPlayerController` | Player > Shop Boss Class = `BP_HG_ShopBoss` |
| `BP_HG_ExplorePlayerController` | `AHGExplorePlayerController` | Player > Explorer Class = `BP_HG_Explorer` |
| `BP_HG_ShopBoss` | `AHGShopBossCharacter` | Mesh = `SK_HG_Boss`, 크기 1.8. 걷기 속도 등은 Character Movement, 아이소 카메라 값은 Camera\|Iso |
| `BP_HG_Explorer` | `AHGExplorerCharacter` | Mesh = `SK_HG_Boss`, 크기 1.8. 점프/카메라/소행성 뛰기 값은 Jump, Camera, Hop, Float |

- 맵: `L_Station` → `BP_HG_ShopGameMode`, `L_PlanetSurface`·`L_Asteroid_Krak7` → `BP_HG_ExploreGameMode`. 프로젝트 기본값도 `BP_HG_ExploreGameMode`.
- 컨트롤러의 사장님 클래스가 비어 있으면 C++ 기본 클래스로 나타난다 (모델 없이 자리 표시).
- 가게와 탐험은 사장님 BP가 따로라서, 나중에 가게 차림(앞치마)을 다른 모델로 둘 수 있다.

## 주인공 모델 에셋 (`Content/HG/Characters/Boss/`)

- `SK_HG_Boss` (+ `_Skeleton`, `_PhysicsAsset`): Tripo 모델을 블렌더에서 정리해 FBX로 가져왔다. 키 1m, 원점 = 발, 앞 = +Y, 본 64개 (언리얼 마네킹 이름: root, pelvis, spine_01~05, …). 애니메이션 없음.
- `M_HG_Boss`: 베이스 컬러 + ORM(R=AO, G=러프니스, B=메탈릭). "Used with Skeletal Mesh"를 켜 두어야 게임에서 회색 기본 머티리얼로 바뀌지 않는다.
- `T_HG_Boss_BaseColor` (4096, sRGB), `T_HG_Boss_ORM` (4096, sRGB 끔, Masks 압축).
  - 베이스 컬러는 면(face)마다 한 가지 색인 단색 텍스처다 (2026-11-27). 바이저(헬멧 유리)만 그라데이션을 남겼다. 원본 PNG = `T_HG_Boss_BaseColor_Face.png`.
  - 단색이라 에셋 크기가 약 1.5MB로 줄었다 (손으로 칠한 텍스처일 때 약 12.6MB). ORM은 4096 그대로 약 8.5MB.
  - 두 텍스처 에셋은 저장소에 올린다 (게임이 쓰는 에셋. 원본 PNG/블렌더 파일/FBX는 저장소 밖 리소스 폴더). 노멀맵은 없다.
- 원본은 저장소 밖 `훠궈훠궈 리소스` 폴더(저장소 폴더 옆)의 `Characters/Boss/`에 둔다: `HG_Boss.blend`(작업 파일, 그림은 같은 폴더 `Textures/`를 상대 경로로 씀), `HG_Boss.fbx`, `Textures/T_HG_Boss_BaseColor.png`, `Textures/T_HG_Boss_ORM.png`.
  언리얼 텍스처 에셋의 원본 경로도 이 폴더를 가리킨다. 다시 칠하면 `HG_Boss.blend`에서 저장한 뒤 텍스처 에셋에서 Reimport.
- 그림체 규칙: 캐릭터는 "면별 단색 + 셀 셰이딩"으로 통일한다 (배경의 단색 복셀과 맞춘다). 새 캐릭터도 같은 방식으로 칠한다.

## 알려진 문제와 다음 할 일

- 다음 정리 순서: 도구가 머티리얼/맵/데이터를 덮어쓰지 않게 하기, 게임 데이터(재료, 가구 등)를 기획자가 고치는 원본으로 정리.
- 사장님 애니메이션 없음 (마네킹 애니메이션 리타깃 후보).
