# Materials - 머티리얼 에셋

> 경로: `Content/HG/Materials/` · 마지막 갱신: 2026-11-24

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

### M_HG_ScanPulse
- 스캐너 파동 화면 효과 (포스트 프로세스, 톤 매핑 뒤). `ensure_scan_material`이 만든다. Custom 노드(HLSL `SCAN_HLSL`) 하나: 화면 색(`PostProcessInput0`) + 월드 위치로 고리 띠(`Origin`, `Radius`, `Width`)와 1m 격자 선(`Grid`), 천체 안쪽만(`Center`, `Limit`), 세기 `Fade`, 색 `ScanColor`. 이웃 천체 `Neighbor0` ~ 7(xyz 중심, w 반지름)과 `BodyRadius`로 "이 천체에 더 붙은 점"만 받는다. 판 번호 `ScanVersion`(도구의 `SCAN_VERSION`과 다르면 도구가 지우고 다시 만든다, 지금 3). `AHGScanWave`가 동적 인스턴스로 매 프레임 바꾼다. 새로 만든 직후 처음 쓸 때는 셰이더 컴파일 동안 잠깐 기본 머티리얼(돌 무늬)이 보인다.

## 알려진 문제와 다음 할 일

- 행성 지형은 버텍스 컬러뿐이라 가까이서 질감이 없다 (복셀 느낌이면 오히려 맞을 수 있다).
