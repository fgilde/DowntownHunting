"""Intro movie, step 1: camera flight through DH_Block as /Game/DH/Cinematics/LS_DH_Intro, rendered with Movie Render Queue
to Saved/IntroFrames (PNG). Runs in the GUI editor (rendering needs a real RHI) and quits when done:
  UnrealEditor.exe DowntownHunting.uproject -ExecCmds="py <repo>/Tools/Python/make_intro.py"
Step 2 (frames -> Content/Movies/DH_Intro.mp4 with title and sound): Tools/Branding/make_intro_movie.py."""
import os

import unreal

MAP = "/Game/DH/Maps/DH_Block"
SEQ_DIR, SEQ_NAME = "/Game/DH/Cinematics", "LS_DH_Intro"
FPS, SECONDS = 30, 20
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "IntroFrames")

# (second, location, (pitch, yaw)): low along the street, into the crossing, crane up over the block.
KEYS = [
    (0.0, (-56300, 13500, 190), (1, 121)),  # streets run at ~30/120 degrees, centre line traced on the ground
    (6.5, (-59600, 19100, 230), (0, 121)),
    (10.0, (-60300, 20300, 1000), (-6, 95)),
    (14.0, (-61200, 19500, 3500), (-17, 55)),
    (20.0, (-63500, 16500, 7000), (-27, 32)),
]

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = SEQ_DIR + "/" + SEQ_NAME
if unreal.EditorAssetLibrary.does_asset_exist(path):
    # Reuse it; delete the asset by hand to rebuild the flight (deleting and recreating it here crashed the editor).
    seq = unreal.load_asset(path)
else:
    seq = tools.create_asset(SEQ_NAME, SEQ_DIR, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    end = FPS * SECONDS
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(end)

    cam = seq.add_spawnable_from_class(unreal.CineCameraActor)
    template = cam.get_object_template()
    camera = template.get_editor_property("camera_component") if template else None
    if camera:
        camera.set_editor_property("current_focal_length", 22.0)
        focus = camera.get_editor_property("focus_settings")
        focus.focus_method = unreal.CameraFocusMethod.DISABLE
        camera.set_editor_property("focus_settings", focus)
    unreal.log("INTRO camera template %s" % camera)

    section = cam.add_track(unreal.MovieScene3DTransformTrack).add_section()
    section.set_range(0, end)
    ch = section.get_all_channels()  # x, y, z, roll, pitch, yaw, sx, sy, sz
    for sec, loc, (pitch, yaw) in KEYS:
        frame = unreal.FrameNumber(int(sec * FPS))
        for i, v in enumerate(loc):
            ch[i].add_key(frame, float(v), interpolation=unreal.MovieSceneKeyInterpolation.AUTO)
        ch[4].add_key(frame, float(pitch), interpolation=unreal.MovieSceneKeyInterpolation.AUTO)
        ch[5].add_key(frame, float(yaw), interpolation=unreal.MovieSceneKeyInterpolation.AUTO)

    cut = seq.add_track(unreal.MovieSceneCameraCutTrack).add_section()
    cut.set_range(0, end)
    cut.set_camera_binding_id(seq.get_binding_id(cam))
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("INTRO sequence saved")

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
# Lyra actors in the map (control points) need Lyra's game mode and an experience. The front-end experience spawns no pawn;
# PIE picks it up from the developer settings override, which is cleared again when the render ends.
dev_settings = unreal.get_default_object(unreal.LyraDeveloperSettings)
dev_settings.set_editor_property("experience_override", unreal.PrimaryAssetId(
    unreal.PrimaryAssetType("LyraExperienceDefinition"), "B_LyraFrontEnd_Experience"))
game = config.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
game.soft_game_mode_override = unreal.LyraGameMode.static_class()
unreal.log("INTRO game mode override %s" % game.soft_game_mode_override)
aa = config.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
aa.temporal_sample_count = 4
aa.engine_warm_up_count = 90  # Lumen and texture streaming settle before frame 0
aa.render_warm_up_count = 32


def finished(executor, success):
    unreal.log("INTRO render finished success=%s" % success)
    dev_settings.set_editor_property("experience_override", unreal.PrimaryAssetId())
    unreal.SystemLibrary.quit_editor()


executor = queue_subsystem.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
executor.on_executor_finished_delegate.add_callable(finished)
unreal.log("INTRO render started -> %s" % OUT_DIR)
