"""회색 박스(블록아웃) 맵과 단색 머티리얼 인스턴스를 생성한다.

실행: Tools/Invoke-HGMapGen.ps1 (에디터를 닫은 상태에서 커맨드렛으로 실행)

- 만드는 것
  /Game/HG/DebugMaterials/Greybox/MI_GB_*   엔진 BasicShapeMaterial을 부모로 한 단색 머티리얼 인스턴스
  /Game/HG/Maps/L_Station             허브 정거장 훠궈집 (경영 + 탐험 시작 지점)
  /Game/HG/Maps/Legacy/L_Asteroid_Krak7  (이전 단계의 손배치 소행성. 지금은 어느 발판도 이리로 보내지 않는다)
  /Game/HG/Maps/Legacy/L_PlanetSurface  예전의 "모든 행성을 받는" 표면 맵 (환경별 맵으로 나뉘어 은퇴. 환경별 맵이 설정에 없을 때의 기본 목적지)
- 이미 있는 맵은 열지도 고치지도 않는다 (2026-11-27부터 upgrade_station/upgrade_existing_map을 부르지 않는다).
  맵이 없을 때만 처음 배치를 만들어 준다. 환경별 맵(L_Planet_Small 등)은 에디터에서 사람이 만들고 고친다.
  다시 만들고 싶으면 환경 변수 HG_MAPGEN_FORCE=1을 주고 실행한다 (기존 맵을 지우고 새로 만든다).
- 이 스크립트는 처음 배치를 잡아 주는 용도이다. 이후의 배치 수정은 에디터에서 맵을 직접 고치는 것이 기준이다.

좌표 단위는 언리얼 유닛(1 = 1cm)이다. 엔진 기본 도형(Cube, Cylinder, Sphere)은 Scale 1일 때 1m 크기이고 중심이 원점이다.
"""
import os
import re
import unreal

FORCE = os.environ.get("HG_MAPGEN_FORCE") == "1"

MAP_DIR = "/Game/HG/Maps"
MAT_DIR = "/Game/HG/DebugMaterials/Greybox"
STATION_MAP = MAP_DIR + "/L_Station"
ASTEROID_MAP = MAP_DIR + "/Legacy/L_Asteroid_Krak7"
PLANET_MAP = MAP_DIR + "/Legacy/L_PlanetSurface"

# 행성에서 정거장으로 돌아올 때 쓰는 도착 태그. AHGPlanetGenerator의 ReturnArrivalTag 기본값과 같다.
FROM_PLANET = "FromPlanet"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

MESH = {
    "cube": unreal.load_asset("/Engine/BasicShapes/Cube.Cube"),
    "cylinder": unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder"),
    "sphere": unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere"),
}

# 색상 팔레트. 웹 프로토타입의 색(마라 빨강, 등불 금색, 산소 청록, 정거장 남색)을 그대로 가져왔다.
PALETTE = {
    "Floor": (0.70, 0.58, 0.45),
    "Wall": (0.10, 0.14, 0.24),
    "WallDark": (0.05, 0.07, 0.13),
    "Broth": (0.80, 0.12, 0.06),
    "BrothWhite": (0.92, 0.86, 0.72),
    "Lantern": (0.95, 0.62, 0.15),
    "Oxygen": (0.25, 0.80, 0.88),
    "Steel": (0.55, 0.58, 0.62),
    "Wood": (0.38, 0.14, 0.09),
    "Rock": (0.36, 0.33, 0.30),
    "RockDark": (0.22, 0.20, 0.19),
    "Mushroom": (0.55, 0.40, 0.95),
    "Kelp": (0.30, 0.85, 0.65),
    "Chili": (0.95, 0.25, 0.15),
    "GasPlanet": (0.75, 0.38, 0.22),
}


# ---------------------------------------------------------------------------
# 머티리얼
# ---------------------------------------------------------------------------
def ensure_materials():
    """색마다 MI_GB_<이름> 머티리얼 인스턴스를 만든다. 부모 셰이더를 공유하므로 셰이더를 새로 컴파일하지 않는다."""
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
    materials = {}
    for name, (r, g, b) in PALETTE.items():
        asset_name = "MI_GB_" + name
        path = MAT_DIR + "/" + asset_name
        if asset_lib.does_asset_exist(path):
            materials[name] = unreal.load_asset(path)
            continue
        mi = asset_tools.create_asset(asset_name, MAT_DIR, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            mi, "Color", unreal.LinearColor(r, g, b, 1.0))
        asset_lib.save_loaded_asset(mi)
        materials[name] = mi
        unreal.log("[MapGen] 머티리얼 생성: " + path)
    return materials


GLOW_MATERIAL = "/Game/HG/Materials/M_HG_Glow"


def ensure_glow_material():
    """밤에 빛나는 등에 쓰는 발광 머티리얼 M_HG_Glow. 없을 때만 만든다 (있으면 에디터에서 고친 것을 존중).

    파라미터: Color(벡터, 바탕색) -> Base Color, Color x Glow(스칼라, 기본 0) -> Emissive Color.
    코드(HGVoxel::PaintGlow)가 동적 인스턴스를 만들어 밤에는 Glow를 올리고 낮에는 0으로 둔다.
    """
    mel = unreal.MaterialEditingLibrary
    if asset_lib.does_asset_exist(GLOW_MATERIAL):
        # 복셀은 인스턴스 메시로 그리므로 이 사용 표시가 꺼져 있으면 엔진이 기본 머티리얼로 바꿔 그린다 (검게 보였다).
        mat = unreal.load_asset(GLOW_MATERIAL)
        if not mat.get_editor_property("used_with_instanced_static_meshes"):
            mat.set_editor_property("used_with_instanced_static_meshes", True)
            mel.recompile_material(mat)
            asset_lib.save_loaded_asset(mat)
            unreal.log("[MapGen] 발광 머티리얼: 인스턴스 메시 사용 표시를 켬")
        return
    folder, name = GLOW_MATERIAL.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 300)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 0.0)
    multiply = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 200)
    mel.connect_material_expressions(color, "", multiply, "A")
    mel.connect_material_expressions(glow, "", multiply, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    asset_lib.save_loaded_asset(mat)
    unreal.log("[MapGen] 발광 머티리얼 생성: " + GLOW_MATERIAL)


SKY_DOME_MATERIAL = "/Game/HG/Materials/M_HG_PlanetSky"
VERTEX_COLOR_MATERIAL = "/Game/HG/Materials/M_HG_VertexColor"

# 행성 하늘 돔: "위"를 행성 중심에서 카메라 쪽으로 계산해서, 둥근 행성 어디에 서 있어도 지평선이 맞게 보인다.
# (엔진의 하늘 대기는 땅이 평평하다고 가정해서 북극에서만 맞았다.)
SKY_HLSL = """
float3 up = normalize(CamPos - Center);
float3 v = normalize(-CamVec);
float h = dot(v, up);
float t = saturate(h * 1.2 + 0.05);
float3 col = lerp(Horizon, Zenith, pow(t, 0.6));
col = lerp(col, Horizon * 0.55, saturate(-h * 3.0));
float s = saturate(dot(v, normalize(SunDir)));
col += SunColor * (pow(s, 600.0) * 10.0 + pow(s, 8.0) * 0.3);
return col * Brightness;
"""


def ensure_planet_materials():
    """행성 그림체용 머티리얼 둘. 없을 때만 만든다 (있으면 에디터에서 고친 것을 존중).

    M_HG_PlanetSky: 빛을 받지 않는(Unlit) 양면 하늘 돔. 파라미터 Center(행성 중심), Zenith(천정 색), Horizon(지평선 색),
      SunDir(해 쪽 방향), SunColor, Brightness. 행성을 감싼 큰 구에 칠한다 (AHGPlanetGenerator).
    M_HG_VertexColor: 꼭짓점 색을 바탕색으로 쓴다. 행성 지형이 면마다 다른 색(높이, 노이즈)을 꼭짓점 색으로 넣는다.
    """
    mel = unreal.MaterialEditingLibrary

    if asset_lib.does_asset_exist(SKY_DOME_MATERIAL):
        # 하늘(Is Sky) 표시: 실시간 캡처 스카이 라이트가 이 돔을 하늘로 보고 그늘에 하늘빛을 비춘다.
        sky = unreal.load_asset(SKY_DOME_MATERIAL)
        if not sky.get_editor_property("is_sky"):
            sky.set_editor_property("is_sky", True)
            mel.recompile_material(sky)
            asset_lib.save_loaded_asset(sky)
            unreal.log("[MapGen] 행성 하늘 머티리얼: 하늘(Is Sky) 표시를 켬")
    else:
        folder, name = SKY_DOME_MATERIAL.rsplit("/", 1)
        mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("is_sky", True)
        custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
        custom.set_editor_property("code", SKY_HLSL)
        custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        names = ["CamPos", "CamVec", "Center", "Zenith", "Horizon", "SunDir", "SunColor", "Brightness"]
        inputs = []
        for n in names:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", n)
            inputs.append(ci)
        custom.set_editor_property("inputs", inputs)
        cam_pos = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -800, -200)
        cam_vec = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -800, -100)
        mel.connect_material_expressions(cam_pos, "", custom, "CamPos")
        mel.connect_material_expressions(cam_vec, "", custom, "CamVec")
        defaults = {
            "Center": unreal.LinearColor(0, 0, 0, 0),
            "Zenith": unreal.LinearColor(0.2, 0.45, 1.0, 1),
            "Horizon": unreal.LinearColor(0.8, 0.9, 1.0, 1),
            "SunDir": unreal.LinearColor(0, 0, 1, 0),
            "SunColor": unreal.LinearColor(1.0, 0.95, 0.85, 1),
        }
        y = 0
        for n, value in defaults.items():
            p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, y)
            p.set_editor_property("parameter_name", n)
            p.set_editor_property("default_value", value)
            mel.connect_material_expressions(p, "RGB", custom, n)
            y += 120
        bright = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, y)
        bright.set_editor_property("parameter_name", "Brightness")
        bright.set_editor_property("default_value", 1.0)
        mel.connect_material_expressions(bright, "", custom, "Brightness")
        mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        asset_lib.save_loaded_asset(mat)
        unreal.log("[MapGen] 행성 하늘 머티리얼 생성: " + SKY_DOME_MATERIAL)

    if not asset_lib.does_asset_exist(VERTEX_COLOR_MATERIAL):
        folder, name = VERTEX_COLOR_MATERIAL.rsplit("/", 1)
        mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)
        mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
        rough.set_editor_property("r", 0.9)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(mat)
        asset_lib.save_loaded_asset(mat)
        unreal.log("[MapGen] 꼭짓점 색 머티리얼 생성: " + VERTEX_COLOR_MATERIAL)


LANDSCAPE_BASIN_MATERIAL = "/Game/HG/Materials/M_HG_LandscapeBasin"

# 평지 행성 땅(랜드스케이프): 평평한 곳은 땅 색 두 가지를 큰 얼룩으로 섞고, 가파른 비탈(법선이 누운 곳)은 바위 색.
# 색 값(Ground, GroundB, Rock)은 게임에서 행성 생성기가 군계 색으로 바꾼다 (AHGPlanetGenerator::PaintFlatGround).
LANDSCAPE_BASIN_HLSL = """
float flatness = saturate((N.z - 0.72) * 6.0);
float patch = saturate(0.5 + 0.5 * sin(P.x / 1700.0 + sin(P.y / 2900.0) * 2.0) * sin(P.y / 2300.0 + sin(P.x / 3700.0) * 2.0));
float3 ground = lerp(Ground, GroundB, smoothstep(0.3, 0.7, patch));
return lerp(Rock, ground, flatness);
"""


def ensure_landscape_material():
    """평지 행성 랜드스케이프 머티리얼 M_HG_LandscapeBasin. 없을 때만 만든다 (있으면 에디터에서 고친 것을 존중).

    파라미터 Ground, GroundB(땅 얼룩), Rock(비탈). Planet Maker 평지 탭이 랜드스케이프를 만들 때 이 머티리얼을 붙인다.
    """
    if asset_lib.does_asset_exist(LANDSCAPE_BASIN_MATERIAL):
        return
    mel = unreal.MaterialEditingLibrary
    folder, name = LANDSCAPE_BASIN_MATERIAL.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", LANDSCAPE_BASIN_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["N", "P", "Ground", "GroundB", "Rock"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    normal = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -800, -200)
    world_pos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -800, -100)
    mel.connect_material_expressions(normal, "", custom, "N")
    mel.connect_material_expressions(world_pos, "", custom, "P")
    defaults = {
        "Ground": unreal.LinearColor(0.80, 0.40, 0.20, 1),
        "GroundB": unreal.LinearColor(0.62, 0.26, 0.15, 1),
        "Rock": unreal.LinearColor(0.42, 0.28, 0.22, 1),
    }
    y = 0
    for n, value in defaults.items():
        p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", value)
        mel.connect_material_expressions(p, "RGB", custom, n)
        y += 120
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    asset_lib.save_loaded_asset(mat)
    unreal.log("[MapGen] 평지 랜드스케이프 머티리얼 생성: " + LANDSCAPE_BASIN_MATERIAL)


SCAN_MATERIAL = "/Game/HG/Materials/M_HG_ScanPulse"
# 스캐너 머티리얼 판 번호. 계산식이나 연결을 바꾸면 올린다 (옛 판을 지우고 다시 만든다).
SCAN_VERSION = 3.0

# 스캐너 파동 (화면 효과, 포스트 프로세스). 화면의 각 점을 월드 위치로 되돌려 계산한다:
#   - 고리: 스캔 시작점에서 거리 Radius 근처(폭 Width)에 빛나는 띠. 데칼처럼 땅, 바위, 식물 어디에나 얹힌다.
#   - 분석 격자: 띠 위와 띠가 지나간 바로 뒤의 표면에 Grid(cm) 간격 격자 선 (와이어프레임처럼 "분석 중" 느낌).
#   - 지금 서 있는 천체 안쪽(중심 Center에서 Limit 안)만 받는다. 하늘(무한히 먼 점), 하늘의 섬은 빛나지 않는다.
#   - 이웃 천체(Neighbor0 ~ 7: xyz = 중심, w = 반지름, w = 0이면 없음)보다 이 천체에 "더 붙어 있는" 점만 받는다.
#     붙어 있는 정도 = 중심까지 거리 / 반지름 (작을수록 그 천체 표면 쪽). 소행성 지대에서 바로 옆 소행성이 같이 빛나던 문제.
#   - Fade로 전체 세기를 줄여 끝에 사라지게 한다.
SCAN_HLSL = """
float3 P = WorldPos;
float dSelf = distance(P, Center);
float rSelf = dSelf / max(BodyRadius, 1.0);
float4 N[8] = { Neighbor0, Neighbor1, Neighbor2, Neighbor3, Neighbor4, Neighbor5, Neighbor6, Neighbor7 };
float closest = 1.0e9;
for (int i = 0; i < 8; i++)
{
    if (N[i].w > 0.0)
    {
        closest = min(closest, distance(P, N[i].xyz) / N[i].w);
    }
}
float onBody = (1.0 - step(Limit, dSelf)) * step(rSelf, closest);
float d = distance(P, Origin);
float band = saturate(1.0 - abs(d - Radius) / Width);
band = band * band;
float behind = saturate(1.0 - (Radius - d) / (Width * 8.0)) * step(d, Radius);
float3 g = abs(frac(P / Grid) - 0.5);
float gridLine = smoothstep(0.44, 0.49, max(max(g.x, g.y), g.z));
float glow = (band * 0.9 + gridLine * (band * 1.4 + behind * 0.35)) * onBody * Fade;
return Scene.rgb + ScanColor * glow;
"""


def ensure_scan_material():
    """스캐너 파동 포스트 프로세스 머티리얼. 없을 때만 만든다 (있으면 에디터에서 고친 것을 존중).

    파라미터: Origin, Center, ScanColor, Neighbor0 ~ 7(벡터), Radius, Width, Limit, Grid, Fade, BodyRadius(스칼라).
    AHGScanWave가 동적 인스턴스로 매 프레임 바꾼다.
    이 머티리얼은 이 도구만 만드는 효과라, 판 번호 파라미터 ScanVersion이 SCAN_VERSION과 다르면(옛 판) 지우고 다시 만든다.
    """
    mel = unreal.MaterialEditingLibrary
    if asset_lib.does_asset_exist(SCAN_MATERIAL):
        old = unreal.load_asset(SCAN_MATERIAL)
        if "ScanVersion" in [str(n) for n in mel.get_scalar_parameter_names(old)] and \
                mel.get_material_default_scalar_parameter_value(old, "ScanVersion") == SCAN_VERSION:
            return
        asset_lib.delete_asset(SCAN_MATERIAL)
        unreal.log("[MapGen] 스캐너 파동 머티리얼: 옛 판이라 다시 만든다 (이웃 천체 빼기)")
    folder, name = SCAN_MATERIAL.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    # 톤 매핑 뒤에 얹는다 (화면 밝기 보정과 상관없이 같은 색으로 빛난다). 엔진 판마다 이름이 달라 둘 다 시도한다.
    for location in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
        if hasattr(unreal.BlendableLocation, location):
            mat.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, location))
            break
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", SCAN_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["Scene", "WorldPos", "Origin", "Center", "ScanColor", "Radius", "Width", "Limit", "Grid", "Fade", "BodyRadius"]
    names += ["Neighbor%d" % i for i in range(8)]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    scene = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, -300)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(scene, "Color", custom, "Scene")
    world_pos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, -180)
    mel.connect_material_expressions(world_pos, "", custom, "WorldPos")
    y = -60
    for n, value in (("Origin", unreal.LinearColor(0, 0, 0, 0)), ("Center", unreal.LinearColor(0, 0, 0, 0)),
                     ("ScanColor", unreal.LinearColor(0.3, 1.0, 0.9, 1))):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", value)
        mel.connect_material_expressions(p, "RGB", custom, n)
        y += 120
    for i in range(8):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1300, -300 + i * 120)
        p.set_editor_property("parameter_name", "Neighbor%d" % i)
        p.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
        # 4칸(xyz = 중심, w = 반지름)이 필요하므로 RGBA 출력에 잇는다 (기본 출력은 RGB 3칸이라 컴파일이 실패했다).
        mel.connect_material_expressions(p, "RGBA", custom, "Neighbor%d" % i)
    for n, value in (("Radius", 0.0), ("Width", 250.0), ("Limit", 100000.0), ("Grid", 100.0), ("Fade", 0.0), ("BodyRadius", 1000.0),
                     ("ScanVersion", SCAN_VERSION)):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", value)
        mel.connect_material_expressions(p, "", custom, n)
        y += 100
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    asset_lib.save_loaded_asset(mat)
    unreal.log("[MapGen] 스캐너 파동 머티리얼 생성: " + SCAN_MATERIAL)


LOOK_TOON_MATERIAL = "/Game/HG/Materials/M_HG_LookToon"
# 그림체 비교 머티리얼 판 번호. 계산식이나 연결을 바꾸면 올린다 (옛 판을 지우고 다시 만든다).
LOOK_VERSION = 8.0

# 셀 셰이딩 (톤 매핑 전, HDR 화면 색). 화면의 각 점에서
#   빛 = 화면 색 / 바탕색(GBuffer BaseColor). 빛의 밝기(휘도)를 로그 눈금으로 Stops(단) 간격 계단으로 끊고,
#   화면 색 전체에 "계단 밝기 / 원래 밝기"를 곱한다. 빛의 색(등불의 주황, 달빛의 파랑)은 그대로 남고 밝기만 계단이 된다.
#   로그 눈금이라 노출(밝기 보정)과 상관없이 어느 장면에서나 비슷한 계단이 생긴다. Soft는 계단 경계의 부드러움.
# 윤곽선: 깊이의 역수(1/깊이)를 가로, 세로로 이계 미분한다. 평평한 면은 1/깊이가 화면에서 직선으로 변해서 0이 되고,
#   물체 테두리(뒤의 먼 것과 겹친 곳)에서만 커진다. 복셀 계단 하나하나에는 선이 생기지 않게 깊이만 쓴다.
#   Line(선 세기)이 0이면 선을 그리지 않는다.
# 하늘(아주 먼 곳)과 바탕색이 거의 검은 곳(빛을 받지 않는 하늘 돔, 우주)은 계단을 넣지 않는다.
TOON_HLSL = """
float3 c = Scene.rgb;
float3 b = Base.rgb;
// 가운데 깊이도 이웃과 같은 함수로 읽는다 (같은 좌표 변환을 거치게).
float d0 = max(SceneTextureFetch(PPI_SceneDepth, float2(0, 0)).r, 1.0) + Depth.r * 0.0;
float3 outc = c;
// Stops가 0이면 계단을 끄고 외곽선만 남긴다 (hg.LookBands 0).
// Soft는 계단 경계의 부드러움 (작을수록 딱 끊긴다). 값은 프로젝트 설정(HG 그림체)이 덮어쓴다.
if (Stops > 0.001 && max(max(b.r, b.g), b.b) > 0.02 && d0 < SkyDepth)
{
    float3 lightRgb = c / max(b, 0.02);
    float L = max(dot(lightRgb, float3(0.2126, 0.7152, 0.0722)), 1.0e-4);
    float x = log2(L) / Stops;
    float q = floor(x) + smoothstep(0.5 - Soft, 0.5 + Soft, frac(x));
    outc = c * (exp2(q * Stops) / L);
    // 카툰처럼 색을 조금 더 쨍하게 (밝기는 두고 채도만 Saturation배)
    float y = dot(outc, float3(0.2126, 0.7152, 0.0722));
    outc = max(lerp(y.xxx, outc, Saturation), 0.0);
}
// 깊이 텍스처는 보간 없이 픽셀 단위로 읽힌다. 반 픽셀 옆을 읽으면 한쪽은 자기 자신, 다른 쪽은 이웃이 읽혀서
// 비스듬한 땅이 "한쪽만 멀어지는 테두리"로 보여 땅 전체가 칠해졌다 (LineWidth 0.5일 때).
// 그래서 이웃은 늘 정수 픽셀(최소 1) 떨어진 곳을 읽고, 1보다 얇은 굵기는 1px 선의 진하기로 낸다.
float w = max(1.0, floor(LineWidth + 0.5));
float thin = saturate(LineWidth);
float iz0 = 1.0 / d0;
float izL = 1.0 / max(SceneTextureFetch(PPI_SceneDepth, float2(-w, 0)).r, 1.0);
float izR = 1.0 / max(SceneTextureFetch(PPI_SceneDepth, float2(w, 0)).r, 1.0);
float izU = 1.0 / max(SceneTextureFetch(PPI_SceneDepth, float2(0, -w)).r, 1.0);
float izD = 1.0 / max(SceneTextureFetch(PPI_SceneDepth, float2(0, w)).r, 1.0);
// 부호를 본다: 앞쪽 물체의 테두리 점은 옆이 먼 배경이라 값이 음수, 배경 쪽 점은 양수다.
// 음수 쪽(앞쪽 물체)에만 선을 그어서 테두리 안팎 두 겹이 아니라 한 겹만 생긴다 (선이 절반 굵기).
// 땅의 꺾임 빼기: 진짜 테두리는 한쪽 이웃만 갑자기 멀어지고 다른 쪽은 거의 그대로다.
//   둥근 행성 땅이나 삼각형 사이 꺾임은 양쪽 이웃이 비슷하게 변한다 (비스듬히 멀리 보면 이계 미분이 커서 줄무늬로 잡혔다).
//   그래서 이계 미분이 "작은 쪽 변화"의 Ratio배를 넘을 때만 선으로 본다.
float lx = -(izL + izR - 2.0 * iz0);
float ly = -(izU + izD - 2.0 * iz0);
float ex = smoothstep(LineDepth, LineDepth * 2.0, lx / iz0) * step(Ratio * min(abs(izL - iz0), abs(izR - iz0)), lx);
float ey = smoothstep(LineDepth, LineDepth * 2.0, ly / iz0) * step(Ratio * min(abs(izU - iz0), abs(izD - iz0)), ly);
// 옅은 선 (높낮이): 언덕 능선이 뒤쪽 땅을 가리는 곳처럼 깊이 차이가 작은 테두리에 FaintLine 세기로 긋는다.
//   능선 앞쪽은 비스듬한 비탈이라 이웃 변화가 커서 Ratio(4)로는 걸러지므로, 더 느슨한 FaintRatio를 쓴다.
//   둥근 땅이나 완만한 꺾임은 이계 미분이 이웃 변화보다 훨씬 작아서 여전히 걸러진다.
float fx = smoothstep(FaintLineDepth, FaintLineDepth * 2.0, lx / iz0) * step(FaintRatio * min(abs(izL - iz0), abs(izR - iz0)), lx);
float fy = smoothstep(FaintLineDepth, FaintLineDepth * 2.0, ly / iz0) * step(FaintRatio * min(abs(izU - iz0), abs(izD - iz0)), ly);
float edge = max(max(ex, ey), max(fx, fy) * FaintLine) * Line * thin;
// 커스텀 스텐실이 NoLineStencil(1)인 점(외계 식물)은 선을 긋지 않는다.
// 커스텀 깊이는 화면 깊이와 따로 그려져서, 언덕 뒤에 가려진 식물도 스텐실이 찍혀 있다.
// 식물이 실제로 보이는 점(커스텀 깊이가 화면 깊이와 거의 같음)에서만 뺀다.
float onFlora = step(abs(Stencil.r - NoLineStencil), 0.5) * step(CDepth.r, d0 * 1.002 + 2.0);
edge *= 1.0 - onFlora;
return lerp(outc, outc * LineColor, edge);
"""


def _custom_with_inputs(mel, mat, code, names):
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    return custom


def _scene_texture(mel, mat, custom, texture_id, pin, y):
    """화면 텍스처 노드를 만들어 Custom 노드 입력에 잇는다. 이 노드가 있어야 엔진이 그 텍스처(깊이, GBuffer)를 셰이더에 묶는다."""
    node = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, y)
    node.set_editor_property("scene_texture_id", texture_id)
    mel.connect_material_expressions(node, "Color", custom, pin)


def _params(mel, mat, custom, scalars, vectors, y):
    for n, value in vectors:
        p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", value)
        mel.connect_material_expressions(p, "RGB", custom, n)
        y += 120
    for n, value in scalars:
        p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", value)
        if custom is not None and n != "LookVersion":
            mel.connect_material_expressions(p, "", custom, n)
        y += 100


def _set_location(mat, names):
    """블렌더블 위치를 정한다. 엔진 판마다 이름이 달라 앞에서부터 있는 것을 쓴다."""
    for location in names:
        if hasattr(unreal.BlendableLocation, location):
            mat.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, location))
            return location
    return None


def _look_is_current(path):
    if not asset_lib.does_asset_exist(path):
        return False
    old = unreal.load_asset(path)
    mel = unreal.MaterialEditingLibrary
    if "LookVersion" in [str(n) for n in mel.get_scalar_parameter_names(old)] and \
            mel.get_material_default_scalar_parameter_value(old, "LookVersion") == LOOK_VERSION:
        return True
    asset_lib.delete_asset(path)
    unreal.log("[MapGen] 그림체 비교 머티리얼: 옛 판이라 다시 만든다 " + path)
    return False


def ensure_look_materials():
    """그림체 비교용 포스트 프로세스 머티리얼 (셀 셰이딩). 코드의 hg.Look 스위치가 화면에 얹는다.

    M_HG_LookToon (톤 매핑 전): 빛 밝기 계단 + 깊이 윤곽선(진한 선, 높낮이용 옅은 선). 파라미터 Stops, Soft, Saturation, SkyDepth,
      LineWidth, LineDepth, Ratio, FaintLineDepth, FaintRatio, FaintLine, Line, NoLineStencil, LineColor.
      조절 값은 코드가 프로젝트 설정(UHGLookSettings)으로 덮어쓴다.
    커스텀 스텐실이 NoLineStencil(1)인 점(외계 식물)에는 윤곽선을 긋지 않는다.
    (2026-11-26에 픽셀 그림체 M_HG_LookPixel도 만들었다가 사용자가 안 쓰기로 해서 뺐다.)
    이 도구만 만드는 효과라 판 번호 LookVersion이 LOOK_VERSION과 다르면 지우고 다시 만든다.
    """
    mel = unreal.MaterialEditingLibrary
    ids = unreal.SceneTextureId

    if not _look_is_current(LOOK_TOON_MATERIAL):
        folder, name = LOOK_TOON_MATERIAL.rsplit("/", 1)
        mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
        # 톤 매핑 전 (HDR 빛 값으로 계단을 끊는다). 5.4부터 이름이 BL_SCENE_COLOR_AFTER_DOF.
        _set_location(mat, ("BL_SCENE_COLOR_AFTER_DOF", "BL_BEFORE_TONEMAPPING"))
        names = ["Scene", "Base", "Depth", "Stencil", "CDepth", "Stops", "Soft", "Saturation", "SkyDepth", "LineWidth", "LineDepth", "Ratio", "FaintLineDepth", "FaintRatio", "FaintLine", "Line",
                 "NoLineStencil", "LineColor"]
        custom = _custom_with_inputs(mel, mat, TOON_HLSL, names)
        _scene_texture(mel, mat, custom, ids.PPI_POST_PROCESS_INPUT0, "Scene", -620)
        _scene_texture(mel, mat, custom, ids.PPI_BASE_COLOR, "Base", -500)
        _scene_texture(mel, mat, custom, ids.PPI_SCENE_DEPTH, "Depth", -380)
        _scene_texture(mel, mat, custom, ids.PPI_CUSTOM_STENCIL, "Stencil", -260)
        _scene_texture(mel, mat, custom, ids.PPI_CUSTOM_DEPTH, "CDepth", -140)
        _params(mel, mat, custom,
                (("Stops", 1.0), ("Soft", 0.08), ("Saturation", 1.2), ("SkyDepth", 2000000.0), ("LineWidth", 0.5), ("LineDepth", 0.004),
                 ("Ratio", 4.0), ("FaintLineDepth", 0.002), ("FaintRatio", 1.5), ("FaintLine", 0.35), ("Line", 1.0), ("NoLineStencil", 1.0), ("LookVersion", LOOK_VERSION)),
                (("LineColor", unreal.LinearColor(0.12, 0.09, 0.11, 1)),), -120)
        mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        asset_lib.save_loaded_asset(mat)
        unreal.log("[MapGen] 셀 셰이딩 머티리얼 생성: " + LOOK_TOON_MATERIAL)


# ---------------------------------------------------------------------------
# 맵 생성과 배치 도우미
# ---------------------------------------------------------------------------
def create_empty_map(map_path):
    """빈 맵 에셋을 만든다. 이미 있으면 FORCE일 때만 지우고 새로 만든다. 새로 만들었으면 True."""
    if asset_lib.does_asset_exist(map_path):
        if not FORCE:
            unreal.log("[MapGen] 이미 있는 맵이라 건너뜀: " + map_path)
            return False
        asset_lib.delete_asset(map_path)
    package_path, asset_name = map_path.rsplit("/", 1)
    world = asset_tools.create_asset(asset_name, package_path, unreal.World, unreal.WorldFactory())
    asset_lib.save_loaded_asset(world)
    unreal.log("[MapGen] 빈 맵 생성: " + map_path)
    return True


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    # unreal.Rotator의 위치 인자 순서는 (roll, pitch, yaw)라서 헷갈리지 않게 이름으로 넘긴다.
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def label(actor, name, folder):
    actor.set_actor_label(name)
    actor.set_folder_path(folder)
    return actor


def shape(kind, name, folder, loc, scale, mat, rotation=None, collide=True):
    """기본 도형 하나를 StaticMeshActor로 배치한다. scale 1 = 1m."""
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), rotation or rot())
    comp = actor.static_mesh_component
    comp.set_static_mesh(MESH[kind])
    comp.set_material(0, MATS[mat])
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if not collide:
        comp.set_collision_profile_name("NoCollision")
    return label(actor, name, folder)


def spawn(cls, name, folder, loc, rotation=None):
    actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), rotation or rot())
    return label(actor, name, folder)


# 맵에 놓는 게임 액터는 C++ 클래스가 아니라 그 BP 하위 클래스를 놓는다 (2026-11-28, Under One Umbrella와 같은 방식).
# BP가 아직 없으면(처음 만드는 프로젝트) C++ 클래스로 대신한다. BP 자체는 이 도구가 만들거나 고치지 않는다.
ACTOR_BPS = {
    "TravelPad": "/Game/HG/BluePrint/World/Travel/BP_HG_TravelPad",
    "PlanetGenerator": "/Game/HG/BluePrint/World/Planet/BP_HG_PlanetGenerator",
    "ShopCameraAnchor": "/Game/HG/BluePrint/World/Camera/BP_HG_ShopCameraAnchor",
    "Furniture": "/Game/HG/BluePrint/Shop/Furniture/BP_HG_Furniture",
}


def actor_class(key, fallback):
    path = ACTOR_BPS[key]
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path).generated_class()
    return fallback


def add_lighting():
    """해(방향광), 하늘 대기, 실시간 캡처 스카이 라이트. 우주 배경으로 바꾸는 것은 아트 단계에서 한다."""
    sun = spawn(unreal.DirectionalLight, "Sun", "Lighting", (0, 0, 800), rot(pitch=-48, yaw=35))
    sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_comp.set_intensity(8.0)
    sun_comp.set_editor_property("atmosphere_sun_light", True)

    spawn(unreal.SkyAtmosphere, "SkyAtmosphere", "Lighting", (0, 0, 0))

    # 스카이 라이트: 하늘 전체에서 오는 간접광. 이것이 없으면 해가 비추지 않는 면이 완전히 검게 나온다.
    sky = spawn(unreal.SkyLight, "SkyLight", "Lighting", (0, 0, 600))
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_comp.set_editor_property("real_time_capture", True)
    sky_comp.set_intensity(1.2)


# 정거장 노출(EV100, 프로젝트의 "확장 휘도 범위" 기준, 클수록 어둡다). 자동 노출을 두면 멀리 줌아웃했을 때
# 화면 대부분이 어두운 우주가 되어 카메라가 밝기를 끌어올리고 가게가 하얗게 날아간다. 그래서 고정한다.
STATION_EXPOSURE_EV = 2.0


def ensure_fixed_exposure(label_name, ev):
    """맵 전체(Unbound)에 노출을 고정하는 포스트 프로세스 볼륨을 둔다. 바뀐 것이 있으면 True.

    볼륨이 없을 때만 만들고 값을 넣는다. 이미 있으면 사람이 에디터에서 고친 값을 존중해 건드리지 않는다.
    예외: 환경 변수 HG_STATION_EXPOSURE가 있으면 그 값으로 덮어쓴다 (값을 맞춰 볼 때 쓰는 도구).
    """
    override = os.environ.get("HG_STATION_EXPOSURE", "")
    existing = [a for a in actor_sub.get_all_level_actors()
                if isinstance(a, unreal.PostProcessVolume) and a.get_actor_label() == label_name]
    if existing and not override:
        return False
    ppv = existing[0] if existing else spawn(unreal.PostProcessVolume, label_name, "Lighting", (0, 0, 0))
    value = float(override) if override else ev
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", value)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", value)
    ppv.set_editor_property("settings", settings)
    unreal.log("[MapGen] %s: 노출 고정 EV %.2f" % (label_name, value))
    return True


def set_travel(pad, dest_map, tag, display_name, color):
    pad.set_editor_property("destination_level", unreal.load_asset(dest_map))
    pad.set_editor_property("arrival_tag", tag)
    pad.set_editor_property("destination_name", display_name)
    pad.set_editor_property("pad_color", unreal.LinearColor(*PALETTE[color], 1.0))


def player_start(name, loc, yaw, tag):
    # 엔진 PlayerStart (2026-11-28, 예전 자체 시작점 HGExplorerStart 대신). 도착 태그는 Player Start Tag.
    # 캡슐 반높이가 92이므로 중심을 바닥 + 93에 두면 발이 바닥에 닿는다. 우주복 차림은 캐릭터 BP가 정한다.
    start = spawn(unreal.PlayerStart, name, "Gameplay", (loc[0], loc[1], 93), rot(yaw=yaw))
    start.set_editor_property("player_start_tag", tag)
    return start


# ---------------------------------------------------------------------------
# L_Station: 허브 정거장 훠궈집 (12m x 10m, 바닥 윗면 Z = 0, 가게 중심 = 원점)
# ---------------------------------------------------------------------------
HX, HY, WALL_H = 600.0, 500.0, 340.0


def build_station():
    add_lighting()
    ensure_fixed_exposure("PostProcess_Station", STATION_EXPOSURE_EV)
    w, d = HX * 2 / 100, HY * 2 / 100

    # 바닥판과 그 아래 받침대. 받침대는 정거장에서 잘라 낸 한 칸을 디오라마처럼 보여 주는 장치이다.
    f = "Shop/Structure"
    shape("cube", "Floor", f, (0, 0, -15), (w, d, 0.3), "Floor")
    shape("cube", "Plinth", f, (0, 0, -150), (w + 0.6, d + 0.6, 2.4), "WallDark")

    # 벽: 기본 카메라 방향(45도)에서 먼 쪽 두 면(+X, +Y)은 높게, 카메라 쪽 두 면은 발목 높이로 낮게 둔다.
    shape("cube", "Wall_East", f, (HX + 15, 0, WALL_H / 2), (0.3, d, WALL_H / 100), "Wall")
    shape("cube", "Wall_North", f, (0, HY + 15, WALL_H / 2), (w + 0.6, 0.3, WALL_H / 100), "Wall")
    shape("cube", "Curb_West", f, (-HX - 15, 0, 20), (0.3, d, 0.4), "Wall")
    shape("cube", "Curb_South", f, (0, -HY - 15, 20), (w + 0.6, 0.3, 0.4), "Wall")
    shape("cube", "Trim_East", f, (HX - 2, 0, 35), (0.05, d, 0.06), "Oxygen", collide=False)
    shape("cube", "Trim_North", f, (0, HY - 2, 35), (w, 0.05, 0.06), "Oxygen", collide=False)

    # 주방: 카운터와 화구 2개. 냄비 위에 마라탕(빨강)과 백탕(흰색) 반원판을 얹는다.
    f = "Shop/Kitchen"
    shape("cube", "Counter", f, (100, 400, 45), (4.8, 1.0, 0.9), "Broth")
    shape("cube", "Counter_Top", f, (100, 400, 93), (4.9, 1.1, 0.08), "Steel")
    for i, x in enumerate((0, 220)):
        shape("cylinder", "Burner_%d_Pot" % i, f, (x, 400, 111), (0.8, 0.8, 0.28), "Steel")
        shape("cylinder", "Burner_%d_Mala" % i, f, (x - 10, 400, 126), (0.36, 0.72, 0.03), "Broth", collide=False)
        shape("cylinder", "Burner_%d_Baitang" % i, f, (x + 10, 400, 126), (0.36, 0.72, 0.03), "BrothWhite", collide=False)

    # 홀: 손님 식탁 4개와 의자. 경영 파트에서 사장님이 옮기거나 치울 수 있는 가구 액터다.
    place_hall_furniture()

    # 재료 진열대와 도착 전광판 (+X 벽).
    f = "Shop/Props"
    shape("cube", "IngredientShelf", f, (510, -330, 75), (1.0, 1.3, 1.5), "Steel")
    for i, mat in enumerate(("Mushroom", "Kelp", "Chili", "Lantern")):
        shape("cube", "IngredientShelf_Slot_%d" % i, f, (455, -330, 40 + i * 28), (0.1, 0.9, 0.2), mat, collide=False)
    shape("cube", "ArrivalBoard", f, (HX - 4, 0, 235), (0.05, 3.2, 1.8), "WallDark", collide=False)
    shape("cube", "ArrivalBoard_Header", f, (HX - 6, 0, 310), (0.05, 3.0, 0.12), "Lantern", collide=False)
    for i in range(4):
        shape("sphere", "Lantern_%d" % i, f, (-360 + i * 240, 250, 300), (0.5, 0.5, 0.42), "Broth", collide=False)

    # 에어록 문 (+Y 벽 왼쪽 구석).
    shape("cube", "Airlock_Door", f, (-450, HY - 5, 125), (1.7, 0.2, 2.5), "Steel", collide=False)
    shape("cube", "Airlock_Light", f, (-450, HY - 18, 245), (1.4, 0.06, 0.1), "Oxygen", collide=False)

    # 게임플레이 표식
    # 경영 카메라의 처음 방향(Yaw 45). 카메라는 사장님을 따라가므로 위치는 쓰지 않는다.
    spawn(actor_class("ShopCameraAnchor", unreal.HGShopCameraAnchor), "ShopCameraAnchor", "Gameplay", (0, 0, 0), rot(yaw=45))
    player_start("PlayerStart_Default", (-100, 220), 90, "Default")
    player_start("PlayerStart_FromPlanet", (-450, 250), -90, FROM_PLANET)

    pad = spawn(actor_class("TravelPad", unreal.HGTravelPad), "TravelPad_Airlock", "Gameplay", (-450, 400, 60))
    set_dive(pad)


HALL_TABLES = ((-300, -250), (100, -250), (-300, 50), (100, 50))


def place_hall_furniture():
    """홀의 처음 가구 배치: 식탁 4개와 식탁마다 의자 1개.

    가구 액터(BP_HG_Furniture, 가구 ID "Table"/"Chair")의 원점은 바닥 위 가구 중심이라 Z = 0에 놓는다.
    의자는 앞쪽(+X)이 손님이 보는 방향이다. 식탁 남쪽(-Y)에 두고 Yaw 90으로 돌려 식탁(북쪽)을 보게 한다.
    """
    f = "Shop/Hall"
    furniture = actor_class("Furniture", unreal.HGFurniture)
    for i, (x, y) in enumerate(HALL_TABLES):
        spawn(furniture, "Table_%d" % i, f, (x, y, 0)).set_editor_property("furniture_id", "Table")
        spawn(furniture, "Chair_%d" % i, f, (x, y - 105, 0), rot(yaw=90)).set_editor_property("furniture_id", "Chair")


def set_dive(pad):
    """에어록 발판을 다이빙 발판으로 만든다. 목적지는 행성 표면 맵, 어느 행성인지는 실행 중에 고른다."""
    set_travel(pad, PLANET_MAP, "Landing", "접근 행성으로 다이빙", "Oxygen")
    pad.set_editor_property("dive_to_selected_planet", True)


# ---------------------------------------------------------------------------
# L_Asteroid_Krak7: 첫 파밍지 (지름 40m 원판, 윗면 Z = 0)
# ---------------------------------------------------------------------------
def build_asteroid():
    add_lighting()

    # 지형: 구형 중력은 다음 단계에서 넣는다. 지금은 납작한 원판 위를 걷는다.
    f = "Asteroid/Ground"
    shape("cylinder", "Ground", f, (0, 0, -50), (40, 40, 1), "Rock")
    shape("cylinder", "Ground_Under", f, (0, 0, -350), (34, 34, 5), "RockDark", collide=False)

    # 바위: 난수 시드를 고정해서 다시 생성해도 같은 배치가 나오게 한다.
    import math
    import random
    rng = random.Random(7)
    f = "Asteroid/Rocks"
    for i in range(26):
        angle = rng.uniform(0, 2 * math.pi)
        radius = rng.uniform(500, 1850)
        s = rng.uniform(0.8, 3.2)
        loc = (math.cos(angle) * radius, math.sin(angle) * radius, s * 30)
        r = rot(pitch=rng.uniform(-25, 25), yaw=rng.uniform(0, 360), roll=rng.uniform(-25, 25))
        shape("cube" if i % 3 == 0 else "sphere", "Rock_%02d" % i, f, loc, (s, s * 0.9, s * 0.7), "Rock", r)

    # 재료 표식: 아직 채집 기능은 없고 위치만 표시한다.
    f = "Asteroid/Ingredients"
    kinds = ("Mushroom", "Kelp", "Chili")
    for i in range(9):
        angle = rng.uniform(0, 2 * math.pi)
        radius = rng.uniform(400, 1700)
        loc = (math.cos(angle) * radius, math.sin(angle) * radius, 25)
        shape("sphere", "Ingredient_%s_%d" % (kinds[i % 3], i), f, loc, (0.4, 0.4, 0.4), kinds[i % 3], collide=False)

    shape("sphere", "GasPlanet", "Backdrop", (90000, 60000, 15000), (600, 600, 600), "GasPlanet", collide=False)

    player_start("PlayerStart_FromStation", (-1300, 0), 0, "FromStation")
    pad = spawn(actor_class("TravelPad", unreal.HGTravelPad), "TravelPad_Return", "Gameplay", (-1600, 0, 60))
    set_travel(pad, STATION_MAP, "FromAsteroid", "훠궈훠궈 (허브 정거장)", "Lantern")


# ---------------------------------------------------------------------------
# L_PlanetSurface: 절차적 행성 표면 (행성 중심 = 원점)
# ---------------------------------------------------------------------------
def build_planet_surface():
    """우주 조명 + 행성 생성기. 지형, 바위, 재료, 귀환 발판은 게임을 시작할 때 생성기가 시드로 만든다."""
    # 대기가 없는 우주라서 하늘 대기는 두지 않는다. 하늘이 검으므로 스카이 라이트 대신 약한 보조광으로 그림자 면을 살린다.
    sun = spawn(unreal.DirectionalLight, "Sun", "Lighting", (0, 0, 80000), rot(pitch=-55, yaw=35))
    sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_comp.set_intensity(10.0)
    # 방향광이 둘이면 엔진이 어느 쪽을 주광원으로 쓸지 경고한다. 해를 우선순위로 지정한다.
    sun_comp.set_editor_property("forward_shading_priority", 1)

    fill =spawn(unreal.DirectionalLight, "FillLight", "Lighting", (0, 0, 80000), rot(pitch=40, yaw=215))
    fill_comp = fill.get_component_by_class(unreal.DirectionalLightComponent)
    fill_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill_comp.set_intensity(1.2)
    fill_comp.set_light_color(unreal.LinearColor(0.55, 0.65, 1.0, 1.0))
    fill_comp.set_cast_shadows(False)

    # 노출 고정: 화면 대부분이 검은 하늘이라 자동 노출을 두면 지면이 하얗게 날아간다.
    # 값은 EV100이다 (프로젝트 기본값인 "확장 휘도 범위"가 켜져 있을 때). 클수록 어둡다.
    ppv = spawn(unreal.PostProcessVolume, "PostProcess_Space", "Lighting", (0, 0, 0))
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 2.5)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 2.5)
    ppv.set_editor_property("settings", settings)

    gen = spawn(actor_class("PlanetGenerator", unreal.HGPlanetGenerator), "PlanetGenerator", "Gameplay", (0, 0, 0))
    gen.set_editor_property("return_level", unreal.load_asset(STATION_MAP))
    gen.set_editor_property("return_arrival_tag", FROM_PLANET)
    gen.set_editor_property("return_name", "훠궈훠궈 (허브 정거장)")

    # 멀리 떠 있는 가스 행성. 검은 하늘에서 거리감과 "우주에 있다"는 느낌을 준다.
    shape("sphere", "GasPlanet", "Backdrop", (900000, 600000, 300000), (4000, 4000, 4000), "GasPlanet", collide=False)


# 맵마다 쓰는 파트(게임 모드). 맵의 World Settings > GameMode Override에 넣는다.
# (C++ 클래스, 블루프린트 경로). 블루프린트가 있으면 블루프린트를 넣는다. 사장님 모델, 컨트롤러 같은 설정은
# 블루프린트에서 사람이 고치므로, 이 도구는 블루프린트를 만들거나 고치지 않고 가리키기만 한다.
BLUEPRINT_DIR = "/Game/HG/BluePrint/Game"
MAP_GAME_MODES = {
    STATION_MAP: ("HGShopGameMode", BLUEPRINT_DIR + "/BP_HG_ShopGameMode"),
    PLANET_MAP: ("HGExploreGameMode", BLUEPRINT_DIR + "/BP_HG_ExploreGameMode"),
    ASTEROID_MAP: ("HGExploreGameMode", BLUEPRINT_DIR + "/BP_HG_ExploreGameMode"),
}


def wanted_game_mode(map_path):
    """맵에 넣을 게임 모드 클래스. 블루프린트가 있으면 그 클래스, 없으면 C++ 클래스."""
    native, blueprint = MAP_GAME_MODES[map_path]
    if unreal.EditorAssetLibrary.does_asset_exist(blueprint):
        return unreal.EditorAssetLibrary.load_blueprint_class(blueprint)
    return getattr(unreal, native).static_class()


def apply_game_mode(map_path):
    """지금 열린 맵의 게임 모드 지정을 MAP_GAME_MODES 값으로 맞춘다. 바꿨으면 True."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    wanted = wanted_game_mode(map_path)
    current = settings.get_editor_property("default_game_mode")
    if current == wanted:
        return False
    settings.set_editor_property("default_game_mode", wanted)
    unreal.log("[MapGen] %s 게임 모드 지정: %s" % (map_path, wanted.get_name()))
    return True


def upgrade_existing_map(map_path):
    """이미 있는 맵의 게임 모드 지정만 맞춘다. 바뀐 것이 있을 때만 저장한다."""
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if apply_game_mode(map_path):
        unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
        unreal.log("[MapGen] 맵 저장: " + map_path)


def upgrade_station():
    """이미 있는 L_Station을 행성 다이빙 규칙과 경영 게임 모드로 바꾼다. 값만 바꾸고, 바뀐 것이 있을 때만 저장한다."""
    world = unreal.EditorLoadingAndSavingUtils.load_map(STATION_MAP)
    changed = apply_game_mode(STATION_MAP)
    if ensure_fixed_exposure("PostProcess_Station", STATION_EXPOSURE_EV):
        changed = True
    for actor in actor_sub.get_all_level_actors():
        if isinstance(actor, unreal.HGTravelPad):
            if actor.get_actor_label() == "TravelPad_Airlock" and not actor.get_editor_property("dive_to_selected_planet"):
                set_dive(actor)
                changed = True
                unreal.log("[MapGen] L_Station 에어록 발판을 다이빙 발판으로 바꿈")
        elif isinstance(actor, unreal.PlayerStart):
            if str(actor.get_editor_property("player_start_tag")) == "FromAsteroid":
                actor.set_editor_property("player_start_tag", FROM_PLANET)
                actor.set_actor_label("PlayerStart_FromPlanet")
                changed = True
                unreal.log("[MapGen] L_Station 귀환 시작점 태그를 FromPlanet으로 바꿈")

    # 홀 식탁이 예전 방식(기본 도형 StaticMeshActor 묶음)이면 배치 가능한 가구 액터로 바꾼다.
    # 가구 액터가 하나라도 있으면 이미 바꾼 맵이거나 사람이 에디터에서 배치한 것이므로 건드리지 않는다.
    actors = actor_sub.get_all_level_actors()
    if not any(isinstance(a, unreal.HGFurniture) for a in actors):
        old_pattern = re.compile(r"^Table_\d+_(Top|Leg|Pot|Seat)$")
        old_parts = [a for a in actors if isinstance(a, unreal.StaticMeshActor) and old_pattern.match(a.get_actor_label())]
        for actor in old_parts:
            actor_sub.destroy_actor(actor)
        place_hall_furniture()
        changed = True
        unreal.log("[MapGen] L_Station 홀 식탁: 도형 %d개를 지우고 가구 액터 %d개로 바꿈" % (len(old_parts), len(HALL_TABLES) * 2))

    if changed:
        unreal.EditorLoadingAndSavingUtils.save_map(world, STATION_MAP)
        unreal.log("[MapGen] 맵 저장: " + STATION_MAP)


# ---------------------------------------------------------------------------
# 데이터 표: 저장소의 CSV(Data/*.csv)를 DataTable 에셋으로 읽어 들인다
# ---------------------------------------------------------------------------
DATA_DIR = "/Game/HG/DataTable"

# (에셋 이름, CSV 파일, 행 구조체). CSV가 원본이고 DataTable은 매번 CSV 내용으로 다시 채운다.
# 그래서 재료 값을 바꿀 때는 CSV만 고치고 이 스크립트를 다시 돌린다. 에디터에서 DataTable을 직접 고치지 않는다.
DATA_TABLES = (
    ("DT_Ingredients", "Data/Ingredients.csv", "HGIngredientRow"),
    ("DT_Combos", "Data/Combos.csv", "HGComboRow"),
    ("DT_Furniture", "Data/Furniture.csv", "HGFurnitureRow"),
)


def ensure_data_tables():
    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for asset_name, csv_rel, struct_name in DATA_TABLES:
        csv_path = os.path.join(project_dir, csv_rel)
        if not os.path.isfile(csv_path):
            unreal.log_error("[MapGen] CSV 없음: " + csv_path)
            continue
        asset_path = "%s/%s" % (DATA_DIR, asset_name)
        row_struct = getattr(unreal, struct_name).static_struct()
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            table = unreal.load_asset(asset_path)
        else:
            factory = unreal.DataTableFactory()
            factory.set_editor_property("struct", row_struct)
            table = asset_tools.create_asset(asset_name, DATA_DIR, unreal.DataTable, factory)
        # CSV 내용과 이미 같으면 저장하지 않는다 (바이너리 에셋이 괜히 바뀌지 않게).
        before = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(table)
        if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv_path, row_struct):
            unreal.log_error("[MapGen] CSV를 읽지 못했다: " + csv_path)
            continue
        after = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(table)
        rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
        if before != after:
            unreal.EditorAssetLibrary.save_loaded_asset(table, False)
            unreal.log("[MapGen] %s <- %s (%d행) 저장" % (asset_path, csv_rel, len(rows)))
        else:
            unreal.log("[MapGen] %s 는 CSV와 같아서 저장하지 않음 (%d행)" % (asset_path, len(rows)))


# ---------------------------------------------------------------------------
# 실행
# ---------------------------------------------------------------------------
ensure_data_tables()
MATS = ensure_materials()
ensure_glow_material()
ensure_planet_materials()
ensure_landscape_material()
ensure_scan_material()
ensure_look_materials()

# 맵끼리 서로 참조(이동 발판의 목적지)하므로 빈 맵을 먼저 모두 만들고 나서 채운다.
targets = []
for map_path, builder in ((STATION_MAP, build_station), (ASTEROID_MAP, build_asteroid), (PLANET_MAP, build_planet_surface)):
    if create_empty_map(map_path):
        targets.append((map_path, builder))

for map_path, builder in targets:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    builder()
    apply_game_mode(map_path)
    unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
    unreal.log("[MapGen] 맵 저장: " + map_path)

# 이미 있는 맵은 열지도 고치지도 않는다 (2026-11-27). 맵은 에디터에서 사람이 직접 만들고 고치는 것이 기준이다.
# 예전에는 여기서 upgrade_station()/upgrade_existing_map()으로 게임 모드, 노출, 에어록 발판, 시작점, 홀 가구를
# 새 규칙으로 바꿨는데, 손으로 고친 맵을 덮어쓸 수 있어서 부르지 않는다 (함수는 기록으로 남겨 둔다).

unreal.log("[MapGen] 완료. 새로 만든 맵 %d개" % len(targets))
