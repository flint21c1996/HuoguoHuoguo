# Blueprints - 기획/아트가 직접 고치는 클래스 설정

> 경로: `Content/HG/BluePrint/Game/`, `Content/HG/Player/`, `Content/ArtSource/` · 마지막 갱신: 2026-11-27

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

- 맵: `L_Station` → `BP_HG_ShopGameMode`, `L_PlanetSurface`·`Legacy/L_Asteroid_Krak7` → `BP_HG_ExploreGameMode`. 프로젝트 기본값도 `BP_HG_ExploreGameMode`.
- 컨트롤러의 사장님 클래스가 비어 있으면 C++ 기본 클래스로 나타난다 (모델 없이 자리 표시).
- 가게와 탐험은 사장님 BP가 따로라서, 나중에 가게 차림(앞치마)을 다른 모델로 둘 수 있다.

## 주인공 모델 에셋 (`Content/ArtSource/`: 메시 `Fbx/Characters/Player`, 머티리얼 `Material/Character`, 텍스처 `Texture/Character`)

- `SK_HG_Boss` (+ `_Skeleton`, `_PhysicsAsset`): Tripo 모델을 블렌더에서 정리해 FBX로 가져왔다. 키 1m(100cm), 원점 = 발, 앞 = +Y, 본 64개 (언리얼 마네킹 이름: root, pelvis, spine_01~05, …).
  - 폴더 정리(2026-11-27) 뒤 `SK_HG_Boss_PhysicsAsset`의 미리보기 메시가 비었다 (파이썬에서 못 고침). 피직스 에셋을 열 때 Preview Mesh를 `SK_HG_Boss`로 한 번 골라 저장한다. 게임에는 영향 없다.
  - FBX 내보내기 규칙 (중요): 뼈 길이까지 cm 값이어야 한다. 블렌더에서 아마추어+메시를 100배 하고 배율 적용(Apply Scale),
    씬 Unit Scale 0.01, Apply Unit 켬, Apply Scalings = All Local로 내보낸다 (작업 .blend는 저장하지 않고 내보내기만 한다).
    예전 방식(미터 데이터 + 단위 변환)은 100배가 뿌리 뼈 배율로 들어가서, 리타게팅 애니메이션이 그 배율을 1로 덮어써 캐릭터가 1/100로 줄어 안 보였다.
- `M_HG_Boss`: 베이스 컬러 + ORM(R=AO, G=러프니스, B=메탈릭). "Used with Skeletal Mesh"를 켜 두어야 게임에서 회색 기본 머티리얼로 바뀌지 않는다.
- `T_HG_Boss_BaseColor` (4096, sRGB), `T_HG_Boss_ORM` (4096, sRGB 끔, Masks 압축).
  - 베이스 컬러는 면(face)마다 한 가지 색인 단색 텍스처다 (2026-11-27). 바이저(헬멧 유리)만 그라데이션을 남겼다. 원본 PNG = `T_HG_Boss_BaseColor_Face.png`.
  - 단색이라 에셋 크기가 약 1.5MB로 줄었다 (손으로 칠한 텍스처일 때 약 12.6MB). ORM은 4096 그대로 약 8.5MB.
  - 두 텍스처 에셋은 저장소에 올린다 (게임이 쓰는 에셋. 원본 PNG/블렌더 파일/FBX는 저장소 밖 리소스 폴더). 노멀맵은 없다.
- 원본은 저장소 밖 `훠궈훠궈 리소스` 폴더(저장소 폴더 옆)의 `Characters/Boss/`에 둔다: `HG_Boss.blend`(작업 파일, 그림은 같은 폴더 `Textures/`를 상대 경로로 씀), `HG_Boss.fbx`, `Textures/T_HG_Boss_BaseColor.png`, `Textures/T_HG_Boss_ORM.png`.
  언리얼 텍스처 에셋의 원본 경로도 이 폴더를 가리킨다. 다시 칠하면 `HG_Boss.blend`에서 저장한 뒤 텍스처 에셋에서 Reimport.
- 그림체 규칙: 캐릭터는 "면별 단색 + 셀 셰이딩"으로 통일한다 (배경의 단색 복셀과 맞춘다). 새 캐릭터도 같은 방식으로 칠한다.

## 주인공 애니메이션 (애니메이션 `Content/ArtSource/Fbx/Animation/Character`, ABP/블렌드 스페이스/리그 `Content/HG/Player/Animations`) (2026-11-27)

- 언리얼 3인칭 템플릿의 마네킹 맨손 동작을 사장님 뼈대로 리타게팅했다. 원본 = 엔진 `Templates/TemplateResources/High/Characters`를 `Content/Characters/Mannequins`로 복사한 것.
- `Rigs/IK_Mannequin`, `Rigs/IK_HG_Boss`: 뼈 이름이 같아서 체인 29개가 자동으로 맞았다. `Rigs/RTG_Mannequin_To_HGBoss`: 사장님 T자세를 마네킹 A자세에 자동 정렬.
- `Anims/ABP_HG_Boss` (템플릿 `ABP_Unarmed`를 복사), `BS_Idle_Walk_Run`, 서기/걷기 8방향/뛰기 8방향/점프/떨어지기/착지 21개.
- `BP_HG_Explorer`, `BP_HG_ShopBoss`의 Mesh > Anim Class = `ABP_HG_Boss`. 다른 애니메이션 블루프린트로 바꾸려면 여기만 바꾼다.
- 리타겟 자세 조정 (2~3등신 몸): 척추/목/머리는 자동 정렬을 되돌려 사장님 원래 자세(똑바로)를 쓴다 (마네킹의 굽은 자세에 맞추면 큰 머리 때문에 구부정해 보임).
  팔은 자동 정렬 각의 60%만 내린다 (덜 내려야 동작할 때 팔이 두꺼운 배를 뚫지 않고 바깥으로 벌어진다).
- 발 IK 컨트롤 리그 `Rigs/CR_HG_Boss_FootIK`: 템플릿 `CR_Mannequin_FootIK`를 옮겨 와 고친 것. ABP가 하드 참조한다.
  - 원래 리그는 발 목표를 마네킹 전용 IK 뼈(`IK_foot_L/R`, `ik_foot_l/r`)에서 읽는다. 사장님 뼈대에는 그 뼈가 없어서 리그 안의 마네킹 기준 자세 값으로
    Full Body IK가 발을 끌어당겨 발이 꺾였다. 목표 뼈를 사장님 발(`foot_l/r`, 뿌리는 `root`)로 바꿔서, 발은 애니메이션 자세를 쓰고 바닥 높이 보정만 더한다.
  - 확인됨: 고친 뒤 서기/걷기에서 발이 바닥에 평평하게, 앞을 향해 놓인다.
- 게임에서 필요한 것은 `Anims/` + `Rigs/CR_HG_Boss_FootIK`뿐이다. 마네킹 메시/원본 애니메이션(`Content/Characters/Mannequins`, 저장소 밖)은 리타게팅을 다시 할 때만 쓴다.
- 확인됨: 행성 착륙 지점에서 서기, 달리기, 점프 동작이 나온다 (셀 셰이딩, 엔진 스크린샷).

## 알려진 문제와 다음 할 일

- 다음 정리 순서: 도구가 머티리얼/맵/데이터를 덮어쓰지 않게 하기, 게임 데이터(재료, 가구 등)를 기획자가 고치는 원본으로 정리.
- 애니메이션 블루프린트는 템플릿 그대로라 걷기/뛰기 판단에 월드 기준 수평 속도를 쓴다. 행성 옆면처럼 "위"가 월드 Z가 아닌 곳에서는
  속도 계산이 틀릴 수 있다 (착륙 지점에서만 확인함). 틀리면 중력 기준 속도를 주는 C++ 애님 인스턴스가 필요하다.
- 가게(정거장)에서의 동작은 화면으로 자세히 보지 않았다. 팔이 배를 살짝 스치는지 더 벌릴지는 보고 정한다 (`ARM_KEEP` 비율).
- 발 IK의 바닥 추적은 월드 아래(-Z)로 쏜다. 행성 옆면에서는 높이 보정이 틀릴 수 있다 (착륙 지점에서만 확인).
- 나중에: 우주복 이미시브(띠, 부츠 밑창 줄이 은은하게 빛나게). 빛날 면만 흰색인 마스크를 `HG_FacePaint.py`로 칠해 만들고
  (따로 한 장, 또는 ORM의 A 채널), 머티리얼에서 마스크 x 색 x 밝기를 Emissive에 잇는다. 색과 밝기는 머티리얼 인스턴스 파라미터로 뺀다.
