"""Main menu backgrounds, step 1: one slow camera move through DH_Block per menu entry, all in the level sequence
/Game/DH/Cinematics/LS_DH_MenuBackgrounds (one camera and camera cut each), rendered with Movie Render Queue to
Saved/MenuFrames (PNG). Runs in the GUI editor and quits when done (same setup as make_intro.py):
  UnrealEditor.exe DowntownHunting.uproject -ExecCmds="py <repo>/Tools/Python/make_menu_backgrounds.py"
Step 2 (frames -> looping Content/Movies/Menu/DH_Menu_<Name>.mp4): Tools/Branding/make_menu_movies.py.
The menu sits on the right, so each shot keeps its subject left of centre."""
import os

import unreal

MAP = "/Game/DH/Maps/DH_Block"
SEQ_DIR, SEQ_NAME = "/Game/DH/Cinematics", "LS_DH_MenuBackgrounds"
FPS, SECONDS = 30, 13.5  # make_menu_movies.py loops 12 s and cross-fades the last 1.5 s into the start
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "MenuFrames")

# name: (start location, (pitch, yaw)), (end location, (pitch, yaw))
SHOTS = [
    ("Play", ((-56300, 13500, 180), (2, 121)), ((-56660, 14100, 180), (2, 121))),
    ("Character", ((-62000, 18200, 160), (2, 30)), ((-61740, 18350, 170), (2, 31))),
    ("Loadout", ((-59400, 19300, 160), (3, 300)), ((-59150, 18867, 160), (3, 303))),
    ("Options", ((-61200, 19500, 3500), (-17, 55)), ((-61937, 20016, 3500), (-17, 60))),
    ("Credits", ((-63500, 16500, 7000), (-27, 32)), ((-63500, 16500, 7300), (-27, 42))),
    ("Quit", ((-61000, 21500, 1600), (-10, 260)), ((-61000, 21500, 1200), (-4, 260))),
]

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = SEQ_DIR + "/" + SEQ_NAME
shot_frames = int(FPS * SECONDS)
end = shot_frames * len(SHOTS)
if unreal.EditorAssetLibrary.does_asset_exist(path):
    seq = unreal.load_asset(path)  # delete by hand to rebuild (recreating in one run crashed the editor, see make_intro.py)
else:
    seq = tools.create_asset(SEQ_NAME, SEQ_DIR, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(end)
    cuts = seq.add_track(unreal.MovieSceneCameraCutTrack)
    for index, (name, (loc0, rot0), (loc1, rot1)) in enumerate(SHOTS):
        first, last = index * shot_frames, (index + 1) * shot_frames
        cam = seq.add_spawnable_from_class(unreal.CineCameraActor)
        cam.set_display_name("Cam_" + name)
        camera = cam.get_object_template().get_editor_property("camera_component")
        camera.set_editor_property("current_focal_length", 24.0)
        focus = camera.get_editor_property("focus_settings")
        focus.focus_method = unreal.CameraFocusMethod.DISABLE
        camera.set_editor_property("focus_settings", focus)

        section = cam.add_track(unreal.MovieScene3DTransformTrack).add_section()
        section.set_range(first, last)
        ch = section.get_all_channels()  # x, y, z, roll, pitch, yaw, sx, sy, sz
        for frame, loc, (pitch, yaw) in ((first, loc0, rot0), (last, loc1, rot1)):
            for i, v in enumerate(loc):
                ch[i].add_key(unreal.FrameNumber(frame), float(v), interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
            ch[4].add_key(unreal.FrameNumber(frame), float(pitch), interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
            ch[5].add_key(unreal.FrameNumber(frame), float(yaw), interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)

        cut = cuts.add_section()
        cut.set_range(first, last)
        cut.set_camera_binding_id(seq.get_binding_id(cam))
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("MENUBG sequence saved, %d frames" % end)

queue_subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
queue = queue_subsystem.get_queue()
queue.delete_all_jobs()
job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
job.sequence = unreal.SoftObjectPath(path + "." + SEQ_NAME)
job.map = unreal.SoftObjectPath(MAP + "." + MAP.split("/")[-1])
config = job.get_configuration()
out = config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
out.output_directory = unreal.DirectoryPath(OUT_DIR)
out.output_resolution = unreal.IntPoint(1920, 1080)
out.file_name_format = "{frame_number}"
out.zero_pad_frame_numbers = 4
config.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
config.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
dev_settings = unreal.get_default_object(unreal.LyraDeveloperSettings)
dev_settings.set_editor_property("experience_override", unreal.PrimaryAssetId(
    unreal.PrimaryAssetType("LyraExperienceDefinition"), "B_LyraFrontEnd_Experience"))
game = config.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
game.soft_game_mode_override = unreal.LyraGameMode.static_class()
aa = config.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
aa.temporal_sample_count = 4
aa.engine_warm_up_count = 90  # per shot: every camera cut is a shot, Lumen and streaming settle at each
aa.render_warm_up_count = 32


def finished(executor, success):
    unreal.log("MENUBG render finished success=%s" % success)
    dev_settings.set_editor_property("experience_override", unreal.PrimaryAssetId())
    unreal.SystemLibrary.quit_editor()


executor = queue_subsystem.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
executor.on_executor_finished_delegate.add_callable(finished)
unreal.log("MENUBG render started -> %s" % OUT_DIR)
