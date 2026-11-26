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
- 원본(블렌더 파일, PNG)은 저장소 밖 개인 폴더에 있다. 텍스처를 다시 칠하면 텍스처 에셋에서 Reimport.

## 알려진 문제와 다음 할 일

- 다음 정리 순서: 도구가 머티리얼/맵/데이터를 덮어쓰지 않게 하기, 게임 데이터(재료, 가구 등)를 기획자가 고치는 원본으로 정리.
- 사장님 애니메이션 없음 (마네킹 애니메이션 리타깃 후보).
