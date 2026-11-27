# Blueprints - 기획/아트가 직접 고치는 클래스 설정

> 경로: `Content/HG/BluePrint/Game/`, `Content/HG/Player/`, `Content/ArtSource/` · 마지막 갱신: 2026-11-28

## 역할

C++는 기능과 규칙을 맡고, "어떤 모델/값을 쓸지"는 블루프린트 하위 클래스의 디테일 패널에서 정한다.
코드를 고치거나 빌드하지 않고 에디터에서 바꿀 수 있게 하려는 것이다.
도구(`Tools/GenerateGreyboxMaps.py`)는 이 블루프린트를 만들거나 고치지 않고, 맵이 가리키게만 한다.

## 연결 순서 (2026-11-28, Under One Umbrella와 같은 방식)

맵 World Settings의 GameMode Override → 게임 모드 BP(Player Controller Class, **Default Pawn Class**) → 사장님 BP(Mesh의 Anim Class) → 애님 BP.
사장님은 엔진 표준 흐름으로 게임 모드가 PlayerStart(행성 맵은 착륙 지점)에 만든다 (`Docs/Source/Game.md`). 예전에는 컨트롤러 BP의 Explorer Class / Shop Boss Class가 정했다 (지움).

| 블루프린트 | 부모 | 여기서 정하는 것 |
|---|---|---|
| `BP_HG_ShopGameMode` | `AHGShopGameMode` | Player Controller = `BP_HG_ShopPlayerController`, Default Pawn = `BP_HG_ShopBoss` |
| `BP_HG_ExploreGameMode` | `AHGExploreGameMode` | Player Controller = `BP_HG_ExplorePlayerController`, Default Pawn = `BP_HG_Explorer` (바탕) |
| `BP_HG_ExploreGameMode_<환경>` (5개) | `BP_HG_ExploreGameMode` | Default Pawn = `BP_HG_Explorer_<환경>`. 환경: PlanetSmall, PlanetMedium, PlanetLarge, AsteroidField, Derelict |
| `BP_HG_ShopPlayerController` | `AHGShopPlayerController` | 손님(Customer Director): Customer Class = `BP_HG_Customer`, Money Popup Class = `BP_HG_MoneyPopup`. 가구(Furniture Placer): Default Furniture Class = `BP_HG_Furniture` |
| `BP_HG_ExplorePlayerController` | `AHGExplorePlayerController` | Scan > Scan Wave Class = `BP_HG_ScanWave` |
| `BP_HG_ShopBoss` | `AHGShopBossCharacter` | Mesh = `SK_HG_Boss`, 크기 1.8, Anim Class = `ABP_HG_Boss_Shop`. 걷기 속도 등은 Character Movement, 아이소 카메라 값은 Camera\|Iso |
| `BP_HG_Explorer` | `AHGExplorerCharacter` | Mesh = `SK_HG_Boss`, 크기 1.8. 점프/카메라/소행성 뛰기 값은 Jump, Camera, Hop, Float. 우주복 차림(Look) |
| `BP_HG_Explorer_<환경>` (5개) | `BP_HG_Explorer` | Mesh의 Anim Class = `ABP_HG_Boss_<환경>`. 나머지는 바탕 값을 물려받는다. 환경마다 다르게 하고 싶은 값(점프, 카메라 등)만 여기서 바꾼다 |

- 경로: 게임 모드 `Content/HG/BluePrint/Game/`, 사장님과 컨트롤러 `Content/HG/Player/Blueprints/`, 애님 BP `Content/HG/Player/Animations/AnimationBlueprints/`.
- 맵: `L_Station` → `BP_HG_ShopGameMode`. 환경 맵 → 각자의 `BP_HG_ExploreGameMode_<환경>`. `L_PlanetMaker`·`Legacy/` → `BP_HG_ExploreGameMode`. 프로젝트 기본값도 `BP_HG_ExploreGameMode`.
- 게임 모드의 Default Pawn이 비어 있으면 C++ 기본 클래스로 나타난다 (모델 없이 자리 표시).
- 가게와 탐험은 사장님 BP가 따로라서, 나중에 가게 차림(앞치마)을 다른 모델로 둘 수 있다.
- 만든 방법: 파이썬 커맨드렛 (BP 하위 클래스 만들기, CDO 값 넣기, 맵 World Settings 바꾸기). 확인됨: 정거장 → 중형/평지/소행성 지대 다이빙과 귀환에서 로그에 환경별 게임 모드와 캐릭터(`BP_HG_Explorer_PlanetMedium_C` 등)가 찍히고, 모델이 달리는 동작과 함께 보인다.

## 월드/가게 액터 BP (2026-11-28, UOU의 `BluePrint/World/<종류>/BP_*` 방식)

맵에 놓거나 게임이 스폰하는 C++ 액터마다 BP 하위 클래스를 둔다. 맵에는 BP를 놓고, 스폰하는 쪽은 BP 클래스를 속성으로 받는다 (비어 있으면 C++ 클래스).
모델, 색, 크기 같은 "어떻게 보일지"는 앞으로 이 BP에서 바꾼다.

| 블루프린트 | 부모 (C++) | 놓는 곳 / 스폰하는 곳 |
|---|---|---|
| `BluePrint/World/Travel/BP_HG_TravelPad` | `AHGTravelPad` | 정거장 에어록, Krak7 귀환 발판(맵). 행성/폐우주선 귀환 발판(생성기 `ReturnPadClass`) |
| `BluePrint/World/Planet/BP_HG_PlanetGenerator` | `AHGPlanetGenerator` | 행성 맵 8개의 생성기. Return Pad Class = `BP_HG_TravelPad` |
| `BluePrint/World/Camera/BP_HG_ShopCameraAnchor` | `AHGShopCameraAnchor` | 정거장 카메라 기준점 |
| `BluePrint/World/Scan/BP_HG_ScanWave` | `AHGScanWave` | 스캐너 파동 (탐험 컨트롤러 `ScanWaveClass`) |
| `BluePrint/Shop/Customer/BP_HG_Customer` | `AHGCustomer` | 손님 (Customer Director `CustomerClass`) |
| `BluePrint/Shop/Customer/BP_HG_MoneyPopup` | `AHGMoneyPopup` | 계산 "+금액" 글자 (`MoneyPopupClass`) |
| `BluePrint/Shop/Furniture/BP_HG_Furniture` | `AHGFurniture` | 정거장 처음 가구 8개(맵), CSV `ActorClass`가 빈 가구 (Furniture Placer `DefaultFurnitureClass`) |

- 맵의 C++ 액터는 엔진 Convert Actors로 BP로 바꿨다 (이름표, 폴더, 위치, 값 그대로). 원래 클래스의 기본값과 같은 값은 옮겨지지 않는다 (가구 ID가 비어서 다시 넣었다).
- 맵 생성 도구(`Tools/GenerateGreyboxMaps.py`)도 새 맵을 만들 때 이 BP를 놓는다 (BP가 없으면 C++ 클래스).
- 확인됨: 정거장에서 손님 2명이 들어와 앉고, 평지 행성에서 스캔(재료 5개 감지)과 귀환 발판(`BP_HG_TravelPad_C_0`)으로 정거장 복귀.

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
- 환경별 애님 BP (2026-11-28): `ABP_HG_Boss`를 복제한 `ABP_HG_Boss_Shop`, `_PlanetSmall`, `_PlanetMedium`, `_PlanetLarge`, `_AsteroidField`, `_Derelict`.
  지금은 모두 원본과 같은 그래프다. 환경마다 다른 동작(무중력에서 떠 있는 자세, 가게 걷기 등)은 각자의 그래프에서 고친다.
  `ABP_HG_Boss`는 쓰지 않는 바탕(새 환경 애님 BP를 만들 때 복제할 원본)으로 남긴다.
- 모든 애님 BP의 부모 = C++ `UHGBossAnimInstance` (`Docs/Source/Player.md`): `Speed`, `VerticalSpeed`, `IsMoving`, `IsInAir`, `IsFloating`을 캐릭터의 위(중력) 기준으로 준다.
  그래프는 아직 템플릿 변수를 읽는다. 행성 옆면에서도 맞게 하려면 그래프 노드를 이 변수로 바꾼다.
- 어떤 애님 BP를 쓸지는 사장님 BP의 Mesh > Anim Class (`BP_HG_ShopBoss` = `_Shop`, `BP_HG_Explorer_<환경>` = `_<환경>`).
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
