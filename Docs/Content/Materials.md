# Materials - 머티리얼 에셋

> 경로: `Content/HG/Materials/` (회색 박스 단색 `MI_GB_*`는 `Content/HG/DebugMaterials/Greybox/`) · 마지막 갱신: 2026-11-27

## Greybox

### MI_GB_* (15개)
- 부모: `/Engine/BasicShapes/BasicShapeMaterial`, `Color` 파라미터만 다르다 (셰이더 새로 컴파일 없음).
- 팔레트: Floor, Wall, WallDark, Broth, BrothWhite, Lantern, Oxygen, Steel, Wood, Rock, RockDark, Mushroom, Kelp, Chili, GasPlanet.
- 색은 웹 프로토타입(마라 빨강, 등불 금색, 산소 청록, 정거장 남색)에서 가져왔다. 값은 `Tools/GenerateGreyboxMaps.py`의 `PALETTE`.
- 맵에 배치된 블록아웃 도형이 쓴다. 런타임 도형(캐릭터, 행성)은 `HGPlaceholder::Paint` 동적 인스턴스를 쓴다.

## HG

### M_HG_Glow
- 밤에 빛나는 등(등불, 홍등)의 발광 머티리얼. `Tools/GenerateGreyboxMaps.py`의 `ensure_glow_material`이 만든다.
- 파라미터 `Color`(바탕색 → Base Color), `Glow`(스칼라, 기본 0, `Color × Glow` → Emissive). 코드가 동적 인스턴스로 밤에 Glow 6, 낮에 0.
- 사용 표시 `Used with Instanced Static Meshes`가 켜져 있어야 한다 (복셀은 인스턴스 메시라서, 꺼져 있으면 엔진이 기본 머티리얼로 그려 검게 보였다).

- 행성 구름도 이 머티리얼로 그린다 (Glow 0.6, 아래에서 봐도 검지 않게).

### M_HG_PlanetSky
- 행성 하늘 돔 머티리얼. `ensure_planet_materials`가 만든다. 빛 없음(Unlit), 양면, 하늘(Is Sky) 표시.
- Custom 노드(HLSL) 하나: 카메라 위치와 방향, 행성 중심(`Center`)으로 "위"를 구해 지평선 → 꼭대기 색(`Horizon`, `Zenith`)을 섞고, 해 방향(`SunDir`)에 해 원반과 번짐을 더한 뒤 `Brightness`를 곱한다.
- 하늘(Is Sky) 표시가 있어야 실시간 캡처 스카이 라이트가 이 돔을 하늘로 보고 그늘에 하늘빛을 비춘다. 이미 있는 에셋이면 스크립트가 이 표시만 켠다.

### M_HG_VertexColor
- 정점 색(Vertex Color) → Base Color, 거칠기 0.9. 각진 행성 지형이 면마다 넣은 색을 그대로 그린다. `ensure_planet_materials`가 만든다.

### M_HG_LandscapeBasin (2026-11-27)
- 평지 행성 랜드스케이프 머티리얼. `ensure_landscape_material`이 없을 때만 만든다 (있으면 에디터에서 고친 것을 존중).
- Custom 노드(HLSL `LANDSCAPE_BASIN_HLSL`) 하나: 월드 법선(`VertexNormalWS`)이 누운 비탈은 `Rock`, 평평한 곳은 `Ground`와 `GroundB`를 큰 얼룩(월드 위치 사인 무늬)으로 섞는다. 거칠기 0.9.
- 게임에서 행성 생성기가 세 색 값을 군계 색으로 바꾼다 (`AHGPlanetGenerator::PaintFlatGround`, `SetLandscapeMaterialVectorParameterValue`). 기본값은 바위 군계 색.
  랜드스케이프 디테일의 **Use Dynamic Material Instance**가 켜져 있어야 한다 (꺼져 있으면 엔진이 색 바꾸기를 무시해서 늘 바위 색이었다. Planet Maker가 만들 때 켜고, 꺼진 랜드스케이프는 로그 `[Flat]` 경고).
  확인됨: 버섯숲 평지 행성에 다이빙하면 땅이 보라색.
- 레이어 칠하기(풀/흙 페인트)는 아직 없다. 필요하면 이 머티리얼에 랜드스케이프 레이어를 더한다.

### M_HG_ScanPulse
- 스캐너 파동 화면 효과 (포스트 프로세스, 톤 매핑 뒤). `ensure_scan_material`이 만든다. Custom 노드(HLSL `SCAN_HLSL`) 하나: 화면 색(`PostProcessInput0`) + 월드 위치로 고리 띠(`Origin`, `Radius`, `Width`)와 1m 격자 선(`Grid`), 천체 안쪽만(`Center`, `Limit`), 세기 `Fade`, 색 `ScanColor`. 이웃 천체 `Neighbor0` ~ 7(xyz 중심, w 반지름)과 `BodyRadius`로 "이 천체에 더 붙은 점"만 받는다. 판 번호 `ScanVersion`(도구의 `SCAN_VERSION`과 다르면 도구가 지우고 다시 만든다, 지금 3). `AHGScanWave`가 동적 인스턴스로 매 프레임 바꾼다. 새로 만든 직후 처음 쓸 때는 셰이더 컴파일 동안 잠깐 기본 머티리얼(돌 무늬)이 보인다.

### M_HG_LookToon
- 그림체 비교 1번(셀 셰이딩) 화면 효과. 포스트 프로세스, 톤 매핑 전(HDR). `ensure_look_materials`가 만든다. 코드의 `hg.Look` 스위치가 얹는다 (`Docs/Source/Game.md`).
- 빛 = 화면 색 / 바탕색(GBuffer BaseColor). 빛 밝기를 로그 눈금으로 `Stops`(1단) 간격 계단으로 끊고 화면 색에 "계단 밝기 / 원래 밝기"를 곱한다. 빛 색은 남고 밝기만 계단이 된다. 경계 부드러움 `Soft`(0.08), 채도 `Saturation`(1.2). `Stops` 0이면 계단과 채도를 끄고 외곽선만 남긴다. 모든 조절 값은 코드가 프로젝트 설정(HG 그림체, `Docs/Source/Game.md`)으로 덮어쓴다.
- 윤곽선: 1/깊이를 가로, 세로로 이계 미분(평평한 면은 0)해서 `LineDepth`(0.004)를 넘으면 `LineColor`를 곱한다. 세기 `Line`(0이면 끔). 깊이만 써서 복셀 계단마다 선이 생기지는 않는다.
  - 옅은 선(높낮이, 판 8, 사용자 요청): 언덕 능선이 뒤쪽 땅을 가리는 곳처럼 깊이 차이가 작은 테두리에 `FaintLine`(0.35) 세기로 긋는다. 기준 `FaintLineDepth`(0.002), 거르기 `FaintRatio`(1.5, 능선 앞 비탈 때문에 진한 선보다 느슨).
  - 굵기 `LineWidth`(0.5, 사용자 요청 "아주 얇게"): 이웃 깊이는 늘 정수 픽셀(최소 1) 떨어진 곳을 읽고, 1보다 작은 값은 1px 선의 진하기(0.5 = 반투명)로 낸다.
  - 앞쪽 물체 쪽에만 선을 긋는다 (이계 미분이 음수인 쪽). 테두리 안팎 두 겹이 아니라 한 겹.
  - 땅의 꺾임 빼기: 이계 미분이 "작은 쪽 이웃 변화"의 `Ratio`(4)배를 넘을 때만 선. 진짜 테두리는 한쪽 이웃만 갑자기 멀어진다.
  - 커스텀 스텐실이 `NoLineStencil`(1)이고 커스텀 깊이가 화면 깊이와 거의 같은(식물이 실제로 보이는) 점은 선을 긋지 않는다. 행성의 외계 식물이 스텐실 1을 찍는다 (`Docs/Source/Planet.md`).
  - 고친 버그 (판 7, 2026-11-26): 굵기를 0.5px로 낮춘 판 4 ~ 6에서 행성 땅에 가로 줄무늬와 꽃 모양 얼룩이 생겼다 (사용자 발견).
    깊이 텍스처는 보간 없이 픽셀 단위로 읽혀서, 반 픽셀 옆을 읽으면 한쪽은 자기 자신이 읽힌다. 그러면 비스듬한 땅이 "한쪽만 멀어지는 테두리"로 판정되어 넓게 칠해졌다.
    꽃 모양 구멍은 언덕 뒤에 가려진 식물의 스텐실 때문이었다 (커스텀 깊이는 화면 깊이와 따로 그려진다). SSAO나 빛 계단 때문이 아니었다 (계단을 꺼도 그대로였다).
  - 확인됨(2026-11-26, 소형 버섯숲 행성, 같은 자리 효과 끔/켬 비교): 땅 줄무늬 없음, 버섯과 캐릭터에 얇은 선, 외계 식물에는 선 없음.
  - 판 8 확인(2026-11-26, 버섯숲 + 중형 얼음 행성): 땅 줄무늬 없음. 옅은 능선 선은 찍은 화면에 겹친 언덕이 없어서 확인하지 못했다.
    계단 경계를 0.08로 다시 또렷하게 하자 꽃 밑동 둘레에 밝은 테두리(옅은 그늘이 한 단 바뀌는 경계)가 보인다. 거슬리면 경계 부드러움을 0.2 정도로 올린다.
- 하늘(`SkyDepth`보다 먼 곳)과 바탕색이 거의 검은 곳은 계단을 넣지 않는다.

- 2026-11-26에 픽셀 그림체 `M_HG_LookPixel`(큰 픽셀 + 색 단계)도 만들었다가 사용자가 안 쓰기로 해서 뺐다 (에셋과 도구 코드 모두 지움).

## 알려진 문제와 다음 할 일

- 그림체 비교 머티리얼은 시제품이다. 셀 셰이딩은 빛이 고른 낮 가게처럼 그늘 변화가 적은 곳에서는 윤곽선 말고는 차이가 작다. 머티리얼에서 굵은 테두리를 그리거나 직접광만 따로 계단으로 끊으려면 엔진 셰이딩 모델(툰) 수준의 작업이 필요하다.
- UI(화면 글자, 미니맵)는 효과를 받지 않는다.

- 행성 지형은 버텍스 컬러뿐이라 가까이서 질감이 없다 (복셀 느낌이면 오히려 맞을 수 있다).
