"""Imports weapons of the Classic Weapons Pack (Stein Games, CC0, see callOfShooter THIRD_PARTY_ASSETS.md) from the
Blacksite repo's source files into /Game/Weapons/Classic/<Name>, with a material from the PBR maps. Taken from Blacksite's
Scripts/import_weapons013.py (AK47, MP5 and 1911 came over as assets); used for the M700 sniper.
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/import_classic_weapons.py M700" -nullrhi"""
import sys
from pathlib import Path
import unreal as u

root = Path(r'C:/dev/privat/github/callOfShooter/SourceAssets/Weapons013/Classic/WeaponsPack')
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary

def import_file(source, folder, name, options=None):
    task = u.AssetImportTask()
    task.filename = str(source)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options: task.options = options
    tools.import_asset_tasks([task])
    assert task.imported_object_paths, str(source)
    return [u.load_asset(path) for path in task.imported_object_paths]

for weapon in sys.argv[1:]:
    folder = '/Game/Weapons/Classic/' + weapon
    options = u.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    options.skeletal_mesh_import_data.convert_scene = True
    imported = import_file(root / weapon / ('SKM_' + weapon + '.fbx'), folder, 'SKM_' + weapon, options)
    meshes = [asset for asset in imported if isinstance(asset, u.SkeletalMesh)]
    assert len(meshes) == 1, str(imported)
    mesh = meshes[0]
    textures = {}
    for suffix in ('C', 'N', 'RMAO'):
        source = root / weapon / ('T_' + ('1991' if weapon == '1911' and suffix == 'N' else weapon) + '_' + suffix + '.png')
        texture = import_file(source, folder, 'T_' + weapon + '_' + suffix)[0]
        texture.set_editor_property('srgb', suffix == 'C')
        texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP if suffix == 'N' else u.TextureCompressionSettings.TC_MASKS if suffix == 'RMAO' else u.TextureCompressionSettings.TC_DEFAULT)
        assert u.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        textures[suffix] = texture
    name = 'M_' + weapon
    mat = u.load_asset(folder + '/' + name) if u.EditorAssetLibrary.does_asset_exist(folder + '/' + name) else tools.create_asset(name, folder, u.Material, u.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    lib.set_base_material_usage(mat, u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    for suffix, outputs in (
        ('C', [('RGB', u.MaterialProperty.MP_BASE_COLOR)]),
        ('N', [('RGB', u.MaterialProperty.MP_NORMAL)]),
        ('RMAO', [('R', u.MaterialProperty.MP_ROUGHNESS), ('G', u.MaterialProperty.MP_METALLIC), ('B', u.MaterialProperty.MP_AMBIENT_OCCLUSION)])):
        node = lib.create_material_expression(mat, u.MaterialExpressionTextureSample)
        node.texture = textures[suffix]
        node.sampler_type = u.MaterialSamplerType.SAMPLERTYPE_NORMAL if suffix == 'N' else u.MaterialSamplerType.SAMPLERTYPE_MASKS if suffix == 'RMAO' else u.MaterialSamplerType.SAMPLERTYPE_COLOR
        for channel, prop in outputs: assert lib.connect_material_property(node, channel, prop)
    lib.recompile_material(mat)
    assert u.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    materials = mesh.get_editor_property('materials')
    for index, material in enumerate(materials):
        material.material_interface = mat
        materials[index] = material
    mesh.set_editor_property('materials', materials)
    assert u.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    assert all(slot.material_interface == mat for slot in mesh.get_editor_property('materials'))
    # Inspect the imported coordinate system and rig; never assume FBX axes.
    actor = u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.SkeletalMeshActor, u.Vector())
    component = actor.skeletal_mesh_component
    component.set_skeletal_mesh(mesh)
    for index in range(component.get_num_bones()):
        bone = component.get_bone_name(index)
        u.log('DH_WEAPON_BONE ' + weapon + ' ' + str(bone) + ' ' + str(component.get_socket_transform(bone, u.RelativeTransformSpace.RTS_COMPONENT)))
    u.log('DH_WEAPON_MESH ' + weapon + ' ' + mesh.get_path_name() + ' ' + str(actor.get_actor_bounds(False)))
    u.get_editor_subsystem(u.EditorActorSubsystem).destroy_actor(actor)
u.log('DH_WEAPON_IMPORT_READY')
