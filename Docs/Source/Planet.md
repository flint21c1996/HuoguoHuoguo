# Planet - 행성 시드와 절차적 행성 생성기

> 경로: `Source/HuoguoHuoguo/Planet/` · 마지막 갱신: 2026-11-23

## 역할

행성은 맵 에셋이 아니라 시드 값으로 존재한다. 같은 시드면 언제나 같은 행성. 행성 표면 맵의 생성기가 시드로 구형 지형과 배치를 만든다.

## 파일

### HGPlanetTypes (.h/.cpp)
- `EHGPlanetBiome`: `Rock`(바위, 고추), `Ice`(얼음, 해초), `Fungal`(버섯숲, 버섯)
- `EHGPlanetSize`: `Small`(반지름 40~120m), `Medium`(300~600m)
- `FHGPlanetSeed`: `Seed`, `Name`, `Biome`, `SizeClass`, `Radius`(cm), `TerrainAmplitude`(반지름 대비 높낮이 비율), `TerrainFrequency`, `GravityScale`, `Richness`, `HoursLeft`(표시용). `Describe()`는 한 줄 설명.
- `HGPlanet::MakePlanet(Seed)`: `FRandomStream`으로 값을 뽑는다. 뽑는 순서를 바꾸면 기존 행성이 바뀌므로 새 값은 맨 뒤에서 뽑는다.
  - 소형: 진폭 4~9%, 중력 0.35~0.7(반지름에 비례). 중형: 진폭 1.2~2.5%, 촘촘한 굴곡, 중력 0.8~1.0.
- `HGPlanet::MakeApproachingPlanets(UniverseSeed, Cycle, Count)`: 주기별 시드 해시로 행성 목록. 첫째는 소형, 둘째는 중형이 나올 때까지 다시 뽑아 둘 다 고를 수 있게 보장한 뒤 이탈 시간순 정렬.
- 생물 군계별 색 `GroundColor`, `RockColor`.

### HGPlanetGenerator (.h/.cpp)
`L_PlanetSurface`에 하나 배치. 행성 중심 = 액터 위치.
- `ChoosePlanet()`: GameInstance의 `GetDivePlanet()`이 있으면 그것, 없으면(맵을 바로 열면) `PreviewPlanet`(`bPreviewFromSeedOnly`면 Seed만 쓰고 재생성). 시드로 노이즈 오프셋을 정한다.
- `SurfaceRadiusAt(Dir)`: 지형의 유일한 높이 함수. 5옥타브 fBm(Perlin3D), 생물 군계 보정(얼음 완만, 버섯숲 둔덕 강조), 북극 `LandingFlatRadius`(10m) 평탄화 후 2배 거리까지 부드럽게 복귀.
- `BuildTerrain()`: 큐브스피어. 면당 격자 = 반지름 * pi/2 / `TargetEdgeLength`(150), 상한 `MaxGridPerFace`(128).
  - 정육면체 점 → 구 사상(Nowell 식)으로 격자 크기를 고르게.
  - 법선은 접선 두 방향 유한 차분으로 계산 (면 이음매 명암 없음).
  - 삼각형 감기 순서는 면마다 첫 칸을 계산해 바깥을 향하도록 자동 결정 (`(P1-P2)x(P0-P2)`).
  - `UProceduralMeshComponent`, 복합 충돌 = 단순 충돌, 동기 쿠킹(사장님이 떨어지지 않게).
- `ScatterProps()`: 지형과 다른 시드 수열. 표면적 비례 개수 (바위 2000m²당 1, 20~1500 / 재료 3500m²당 1, 12~400) × Richness. 착륙 지점 근처 제외. 인스턴스 메시(`RoundRocks`, `BlockRocks` 충돌 있음, 재료 3종 충돌 없음).
- `SpawnReturnPad()`: 착륙 지점에서 4m 옆에 금색 `AHGTravelPad`를 지연 생성 (`ReturnLevel`, `ReturnArrivalTag` 기본 `FromPlanet`, `ReturnName`).
- `GetLandingTransform()`: 북극 지표 + 95. `GetGravityCenter()`: 액터 위치.

## 흐름 / 규칙

- 저장해야 할 것은 시드와 플레이어가 바꾼 것(캔 자원 등)뿐이다.
- 높이를 쓰는 모든 곳(메시, 배치, 착륙, 발판)은 `SurfaceRadiusAt` 하나를 쓴다.
- 측정값 (스모크 테스트): 소형 반지름 51m 지형 정점 1.8만, 생성 50ms. 중형 반지름 409m 정점 10만 / 삼각형 19.7만, 생성 233ms.

## 알려진 문제와 다음 할 일

- 에디터 뷰포트에서는 지형이 보이지 않는다 (실행할 때만 생성). 에디터 미리보기가 필요하면 별도 버튼으로 만든다.
- 재료는 표식뿐, 채집 기능 없음. 캔 자원 저장 없음.
- 밤 쪽이 꽤 어둡다 (보조광 1.2). 하늘에 별 없음.
- 중형 행성은 격자 상한 때문에 격자 한 칸이 약 5m로 거칠다. 더 크게 하려면 LOD/청크 분할이 필요하다.
- 다이빙 연출, 정거장 창밖 행성 표시는 아직 없다.
