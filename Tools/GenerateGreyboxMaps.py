"""회색 박스(블록아웃) 맵과 단색 머티리얼 인스턴스를 생성한다.

실행: Tools/Invoke-HGMapGen.ps1 (에디터를 닫은 상태에서 커맨드렛으로 실행)

- 만드는 것
  /Game/HG/Materials/Greybox/MI_GB_*   엔진 BasicShapeMaterial을 부모로 한 단색 머티리얼 인스턴스
  /Game/HG/Maps/L_Station             허브 정거장 훠궈집 (경영 + 탐험 시작 지점)
  /Game/HG/Maps/L_Asteroid_Krak7      (이전 단계의 손배치 소행성. 지금은 어느 발판도 이리로 보내지 않는다)
  /Game/HG/Maps/L_PlanetSurface       절차적 행성 표면. 조명과 AHGPlanetGenerator만 있고 지형은 실행할 때 시드로 만든다.
- 이미 있는 맵과 머티리얼은 건드리지 않는다. 사람이 에디터에서 고친 배치를 덮어쓰지 않기 위해서이다.
  다시 만들고 싶으면 환경 변수 HG_MAPGEN_FORCE=1을 주고 실행한다 (기존 맵을 지우고 새로 만든다).
- 예외: upgrade_station()/upgrade_existing_map()은 이미 있는 맵에서 "값"만 새 규칙으로 바꾼다
  (게임 모드 지정, 정거장 에어록 발판과 귀환 시작점). 이미 맞으면 저장하지 않는다.
  유일하게 액터를 지우는 경우: 정거장 홀 식탁이 예전 도형 묶음(Table_N_Top/Leg/Pot/Seat)이고 가구 액터가 하나도 없으면
  그 도형들을 지우고 HGTable/HGChair 가구 액터로 바꾼다. 가구 액터가 하나라도 있으면 건드리지 않는다.
- 이 스크립트는 처음 배치를 잡아 주는 용도이다. 이후의 배치 수정은 에디터에서 맵을 직접 고치는 것이 기준이다.

좌표 단위는 언리얼 유닛(1 = 1cm)이다. 엔진 기본 도형(Cube, Cylinder, Sphere)은 Scale 1일 때 1m 크기이고 중심이 원점이다.
"""
import os
import re
import unreal

FORCE = os.environ.get("HG_MAPGEN_FORCE") == "1"

MAP_DIR = "/Game/HG/Maps"
MAT_DIR = "/Game/HG/Materials/Greybox"
STATION_MAP = MAP_DIR + "/L_Station"
ASTEROID_MAP = MAP_DIR + "/L_Asteroid_Krak7"
PLANET_MAP = MAP_DIR + "/L_PlanetSurface"

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


def set_travel(pad, dest_map, tag, display_name, color):
    pad.set_editor_property("destination_level", unreal.load_asset(dest_map))
    pad.set_editor_property("arrival_tag", tag)
    pad.set_editor_property("destination_name", display_name)
    pad.set_editor_property("pad_color", unreal.LinearColor(*PALETTE[color], 1.0))


def explorer_start(name, loc, yaw, tag, space_suit):
    # 캡슐 반높이가 92이므로 중심을 바닥 + 93에 두면 발이 바닥에 닿는다.
    start = spawn(unreal.HGExplorerStart, name, "Gameplay", (loc[0], loc[1], 93), rot(yaw=yaw))
    start.set_editor_property("start_tag", tag)
    start.set_editor_property("space_suit", space_suit)
    return start


# ---------------------------------------------------------------------------
# L_Station: 허브 정거장 훠궈집 (12m x 10m, 바닥 윗면 Z = 0, 가게 중심 = 원점)
# ---------------------------------------------------------------------------
HX, HY, WALL_H = 600.0, 500.0, 340.0


def build_station():
    add_lighting()
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
    spawn(unreal.HGShopCameraAnchor, "ShopCameraAnchor", "Gameplay", (0, 0, 0), rot(yaw=45))
    explorer_start("ExplorerStart_Default", (-100, 220), 90, "Default", False)
    explorer_start("ExplorerStart_FromPlanet", (-450, 250), -90, FROM_PLANET, False)

    pad = spawn(unreal.HGTravelPad, "TravelPad_Airlock", "Gameplay", (-450, 400, 60))
    set_dive(pad)


HALL_TABLES = ((-300, -250), (100, -250), (-300, 50), (100, 50))


def place_hall_furniture():
    """홀의 처음 가구 배치: 식탁 4개와 식탁마다 의자 1개.

    가구 액터(HGTable, HGChair)의 원점은 바닥 위 가구 중심이라 Z = 0에 놓는다.
    의자는 앞쪽(+X)이 손님이 보는 방향이다. 식탁 남쪽(-Y)에 두고 Yaw 90으로 돌려 식탁(북쪽)을 보게 한다.
    """
    f = "Shop/Hall"
    for i, (x, y) in enumerate(HALL_TABLES):
        spawn(unreal.HGTable, "Table_%d" % i, f, (x, y, 0))
        spawn(unreal.HGChair, "Chair_%d" % i, f, (x, y - 105, 0), rot(yaw=90))


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

    explorer_start("ExplorerStart_FromStation", (-1300, 0), 0, "FromStation", True)
    pad = spawn(unreal.HGTravelPad, "TravelPad_Return", "Gameplay", (-1600, 0, 60))
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

    gen = spawn(unreal.HGPlanetGenerator, "PlanetGenerator", "Gameplay", (0, 0, 0))
    gen.set_editor_property("return_level", unreal.load_asset(STATION_MAP))
    gen.set_editor_property("return_arrival_tag", FROM_PLANET)
    gen.set_editor_property("return_name", "훠궈훠궈 (허브 정거장)")

    # 멀리 떠 있는 가스 행성. 검은 하늘에서 거리감과 "우주에 있다"는 느낌을 준다.
    shape("sphere", "GasPlanet", "Backdrop", (900000, 600000, 300000), (4000, 4000, 4000), "GasPlanet", collide=False)


# 맵마다 쓰는 파트(게임 모드). 맵의 World Settings > GameMode Override에 넣는다.
MAP_GAME_MODES = {
    STATION_MAP: "HGShopGameMode",
    PLANET_MAP: "HGExploreGameMode",
    ASTEROID_MAP: "HGExploreGameMode",
}


def apply_game_mode(map_path):
    """지금 열린 맵의 게임 모드 지정을 MAP_GAME_MODES 값으로 맞춘다. 바꿨으면 True."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    wanted = getattr(unreal, MAP_GAME_MODES[map_path]).static_class()
    current = settings.get_editor_property("default_game_mode")
    if current == wanted:
        return False
    settings.set_editor_property("default_game_mode", wanted)
    unreal.log("[MapGen] %s 게임 모드 지정: %s" % (map_path, MAP_GAME_MODES[map_path]))
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
    for actor in actor_sub.get_all_level_actors():
        if isinstance(actor, unreal.HGTravelPad):
            if actor.get_actor_label() == "TravelPad_Airlock" and not actor.get_editor_property("dive_to_selected_planet"):
                set_dive(actor)
                changed = True
                unreal.log("[MapGen] L_Station 에어록 발판을 다이빙 발판으로 바꿈")
        elif isinstance(actor, unreal.HGExplorerStart):
            if str(actor.get_editor_property("start_tag")) == "FromAsteroid":
                actor.set_editor_property("start_tag", FROM_PLANET)
                actor.set_actor_label("ExplorerStart_FromPlanet")
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
DATA_DIR = "/Game/HG/Data"

# (에셋 이름, CSV 파일, 행 구조체). CSV가 원본이고 DataTable은 매번 CSV 내용으로 다시 채운다.
# 그래서 재료 값을 바꿀 때는 CSV만 고치고 이 스크립트를 다시 돌린다. 에디터에서 DataTable을 직접 고치지 않는다.
DATA_TABLES = (
    ("DT_Ingredients", "Data/Ingredients.csv", "HGIngredientRow"),
    ("DT_Combos", "Data/Combos.csv", "HGComboRow"),
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

# 이미 있던 맵은 값만 새 규칙으로 맞춘다 (예외: 정거장 홀 식탁 도형을 가구 액터로 바꾸는 것, 위 설명 참고).
new_paths = [path for path, _ in targets]
if STATION_MAP not in new_paths:
    upgrade_station()
for map_path in (PLANET_MAP, ASTEROID_MAP):
    if map_path not in new_paths:
        upgrade_existing_map(map_path)

unreal.log("[MapGen] 완료. 새로 만든 맵 %d개" % len(targets))
