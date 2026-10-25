"""Effect materials for the abilities, in /DowntownCore/Effects:
- M_DH_Cloak: what a cloaked hunter looks like to themselves and their team: almost see-through, a cyan rim (fresnel)
  that shimmers. Enemies do not see them at all (DHAbilities::UpdateCloak).
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/make_fx_materials.py" -nullrhi"""
import unreal as u

FOLDER = '/DowntownCore/Effects'
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary


def material(name):
    path = FOLDER + '/' + name
    mat = u.load_asset(path)  # (the asset registry may not have scanned the plugin yet: load, do not ask it)
    if mat:
        lib.delete_all_material_expressions(mat)
        return mat
    return tools.create_asset(name, FOLDER, u.Material, u.MaterialFactoryNew())


def node(mat, cls, x, y):
    return lib.create_material_expression(mat, cls, x, y)


cloak = material('M_DH_Cloak')
cloak.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
cloak.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
cloak.set_editor_property('two_sided', False)
for usage in ('used_with_skeletal_mesh', 'used_with_static_lighting'):
    try:
        cloak.set_editor_property(usage, True)
    except Exception:
        pass
lib.set_material_usage(cloak, u.MaterialUsage.MATUSAGE_SKELETAL_MESH)

fresnel = node(cloak, u.MaterialExpressionFresnel, -800, 0)
fresnel.set_editor_property('exponent', 2.5)
fresnel.set_editor_property('base_reflect_fraction', 0.02)
# shimmer: a fast sine on time, 0.7 .. 1.3
time = node(cloak, u.MaterialExpressionTime, -1000, 300)
speed = node(cloak, u.MaterialExpressionMultiply, -850, 300)
speed.set_editor_property('const_b', 7.0)
lib.connect_material_expressions(time, '', speed, 'A')
sine = node(cloak, u.MaterialExpressionSine, -700, 300)
lib.connect_material_expressions(speed, '', sine, '')
shimmer = node(cloak, u.MaterialExpressionMultiply, -550, 300)
shimmer.set_editor_property('const_b', 0.3)
lib.connect_material_expressions(sine, '', shimmer, 'A')
pulse = node(cloak, u.MaterialExpressionAdd, -400, 300)
pulse.set_editor_property('const_b', 1.0)
lib.connect_material_expressions(shimmer, '', pulse, 'A')
lifted = node(cloak, u.MaterialExpressionAdd, -600, 0)
lifted.set_editor_property('const_b', 0.45)  # also faces turned to the camera glow a little
lib.connect_material_expressions(fresnel, '', lifted, 'A')
rim = node(cloak, u.MaterialExpressionMultiply, -400, 0)
lib.connect_material_expressions(lifted, '', rim, 'A')
lib.connect_material_expressions(pulse, '', rim, 'B')

colour = node(cloak, u.MaterialExpressionConstant3Vector, -400, -200)
colour.set_editor_property('constant', u.LinearColor(0.4, 3.0, 5.0, 1.0))
emissive = node(cloak, u.MaterialExpressionMultiply, -200, -100)
lib.connect_material_expressions(colour, '', emissive, 'A')
lib.connect_material_expressions(rim, '', emissive, 'B')
lib.connect_material_property(emissive, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

opacity = node(cloak, u.MaterialExpressionMultiply, -200, 150)
opacity.set_editor_property('const_b', 0.6)
lib.connect_material_expressions(rim, '', opacity, 'A')
floor = node(cloak, u.MaterialExpressionAdd, -60, 150)
floor.set_editor_property('const_b', 0.14)  # a faint body even face-on
lib.connect_material_expressions(opacity, '', floor, 'A')
lib.connect_material_property(floor, '', u.MaterialProperty.MP_OPACITY)

lib.recompile_material(cloak)
u.EditorAssetLibrary.save_loaded_asset(cloak, False)
print('DH fx: M_DH_Cloak')

# M_DH_Boundary: the invisible walls at the edge of the play area, shown as a warning field that fades in within about
# twenty metres of the camera: orange-red diagonal stripes in world space, glowing (values high enough for Lyra's daylight exposure). Placed on every blocking
# volume by DowntownCore (DHBoundaries).
wall = material('M_DH_Boundary')
wall.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
wall.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
wall.set_editor_property('two_sided', True)

world = node(wall, u.MaterialExpressionWorldPosition, -1400, 0)
diagonal = node(wall, u.MaterialExpressionDotProduct, -1200, 0)
weights = node(wall, u.MaterialExpressionConstant3Vector, -1400, 150)
weights.set_editor_property('constant', u.LinearColor(1.0 / 90, 1.0 / 90, 1.0 / 90, 1.0))
lib.connect_material_expressions(world, '', diagonal, 'A')
lib.connect_material_expressions(weights, '', diagonal, 'B')
frac = node(wall, u.MaterialExpressionFrac, -1050, 0)
lib.connect_material_expressions(diagonal, '', frac, '')
stripes = node(wall, u.MaterialExpressionRound, -900, 0)
lib.connect_material_expressions(frac, '', stripes, '')

distance = node(wall, u.MaterialExpressionPixelDepth, -1200, 350)  # world distance against the camera position did not fade
near = node(wall, u.MaterialExpressionSubtract, -1050, 350)
near.set_editor_property('const_b', 300.0)
lib.connect_material_expressions(distance, '', near, 'A')
scaled = node(wall, u.MaterialExpressionDivide, -900, 350)
scaled.set_editor_property('const_b', 1600.0)
lib.connect_material_expressions(near, '', scaled, 'A')
fade = node(wall, u.MaterialExpressionOneMinus, -750, 350)
lib.connect_material_expressions(scaled, '', fade, '')
clamped = node(wall, u.MaterialExpressionSaturate, -600, 350)
lib.connect_material_expressions(fade, '', clamped, '')

strength = node(wall, u.MaterialExpressionMultiply, -700, 0)
strength.set_editor_property('const_b', 0.45)
lib.connect_material_expressions(stripes, '', strength, 'A')
base = node(wall, u.MaterialExpressionAdd, -550, 0)
base.set_editor_property('const_b', 0.08)
lib.connect_material_expressions(strength, '', base, 'A')
alpha = node(wall, u.MaterialExpressionMultiply, -380, 150)
lib.connect_material_expressions(base, '', alpha, 'A')
lib.connect_material_expressions(clamped, '', alpha, 'B')
lib.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY)

glow = node(wall, u.MaterialExpressionConstant3Vector, -550, -200)
glow.set_editor_property('constant', u.LinearColor(140.0, 40.0, 3.0, 1.0))  # bright against daylight exposure
lit = node(wall, u.MaterialExpressionMultiply, -380, -100)
lib.connect_material_expressions(glow, '', lit, 'A')
lib.connect_material_expressions(base, '', lit, 'B')
lib.connect_material_property(lit, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

lib.recompile_material(wall)
u.EditorAssetLibrary.save_loaded_asset(wall, False)
print('DH fx: M_DH_Boundary')
