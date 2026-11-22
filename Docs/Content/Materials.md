# Materials - 머티리얼 에셋

> 경로: `Content/HG/Materials/` · 마지막 갱신: 2026-11-23

## Greybox

### MI_GB_* (15개)
- 부모: `/Engine/BasicShapes/BasicShapeMaterial`, `Color` 파라미터만 다르다 (셰이더 새로 컴파일 없음).
- 팔레트: Floor, Wall, WallDark, Broth, BrothWhite, Lantern, Oxygen, Steel, Wood, Rock, RockDark, Mushroom, Kelp, Chili, GasPlanet.
- 색은 웹 프로토타입(마라 빨강, 등불 금색, 산소 청록, 정거장 남색)에서 가져왔다. 값은 `Tools/GenerateGreyboxMaps.py`의 `PALETTE`.
- 맵에 배치된 블록아웃 도형이 쓴다. 런타임 도형(캐릭터, 행성)은 `HGPlaceholder::Paint` 동적 인스턴스를 쓴다.

## 알려진 문제와 다음 할 일

- 행성 지형 전용 머티리얼(높이/경사 색, 버텍스 컬러) 없음. 지금은 생물 군계별 단색이다.
