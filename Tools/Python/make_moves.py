"""Sprint and slide animations derived from Lyra's own weapon animations (Lyra has neither):
- A_DH_Sprint_<Rifle|Pistol>: Lyra's jog with the body leaning into the run, chest and weapon tipped down, head up.
- A_DH_Slide_<Rifle|Pistol>: Lyra's crouch idle turned into a slide: hips near the ground, the left leg stretched out in
  front, the right one folded under, the body leaning back with the weapon still level.
Poses are edited in component space (mannequin: +Y forward, +Z up, +X its left) with directions, not bone axes.
Written to /DowntownCore/Animations; played as dynamic montages by UDHPlayerRulesComponent (slot FullBody).
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/make_moves.py" -nullrhi"""
import math
import unreal as u

FOLDER = '/DowntownCore/Animations'
LOCO = '/Game/Characters/Heroes/Mannequin/Animations/Locomotion/'


# --- quaternions (x, y, z, w), Hamilton product; q1 * q2 = rotate by q2, then by q1 (FQuat convention)
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    x, y, z, w = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return (x, y, z)


def axis_angle(axis, degrees):
    s = math.sin(math.radians(degrees) / 2)
    n = math.sqrt(sum(c * c for c in axis))
    return (axis[0] / n * s, axis[1] / n * s, axis[2] / n * s, math.cos(math.radians(degrees) / 2))


def between(a, b):
    """Shortest rotation turning direction a into direction b."""
    na, nb = norm(a), norm(b)
    c = (na[1] * nb[2] - na[2] * nb[1], na[2] * nb[0] - na[0] * nb[2], na[0] * nb[1] - na[1] * nb[0])
    d = sum(x * y for x, y in zip(na, nb))
    q = (c[0], c[1], c[2], 1.0 + d)
    n = math.sqrt(sum(x * x for x in q))
    return tuple(x / n for x in q)


def norm(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / n for c in v)


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


class Pose:
    """Local bone transforms of one frame, world (component) space computed on demand."""

    def __init__(self, seq, bones, parents, frame):
        self.bones, self.parents = bones, parents
        self.rot, self.pos, self.scale = {}, {}, {}
        for bone in bones:
            t = u.AnimationLibrary.get_bone_pose_for_frame(seq, bone, frame, False)
            r = t.rotation
            self.rot[bone] = (r.x, r.y, r.z, r.w)
            self.pos[bone] = (t.translation.x, t.translation.y, t.translation.z)
            self.scale[bone] = t.scale3d

    def world(self, bone):
        parent = self.parents[bone]
        if parent is None:
            return self.rot[bone], self.pos[bone]
        prot, ppos = self.world(parent)
        return qmul(prot, self.rot[bone]), tuple(p + d for p, d in zip(ppos, qrot(prot, self.pos[bone])))

    def set_world(self, bone, rot=None, pos=None):
        wrot, wpos = self.world(bone)
        rot = rot or wrot
        pos = pos or wpos
        parent = self.parents[bone]
        if parent is None:
            self.rot[bone], self.pos[bone] = rot, pos
            return
        prot, ppos = self.world(parent)
        self.rot[bone] = qmul(qinv(prot), rot)
        self.pos[bone] = qrot(qinv(prot), sub(pos, ppos))

    def turn(self, bone, delta):
        """Rotates a bone (and so its children) by a component-space rotation about its own position."""
        wrot, _ = self.world(bone)
        self.set_world(bone, rot=qmul(delta, wrot))

    def aim(self, bone, child, direction):
        """Turns a bone so the line to its child points along a component-space direction."""
        current = sub(self.world(child)[1], self.world(bone)[1])
        self.turn(bone, between(current, direction))


def skeleton_of(seq):
    bones = [str(b) for b in u.AnimPoseExtensions.get_bone_names(u.AnimPoseExtensions.get_anim_pose_at_frame(seq, 0, u.AnimPoseEvaluationOptions()))]
    parents = {}
    for bone in bones:
        path = [str(b) for b in u.AnimationLibrary.find_bone_path_to_root(seq, bone)]
        parents[bone] = path[1] if len(path) > 1 else None
    return bones, parents


LATERAL = (1.0, 0.0, 0.0)  # body left; a positive turn about it swings things forward-down -> forward-up


def sprint(pose):
    pose.turn('spine_01', axis_angle(LATERAL, -9))   # lean into the run
    pose.turn('spine_03', axis_angle(LATERAL, -16))  # chest and weapon tipped down
    pose.turn('neck_01', axis_angle(LATERAL, 15))    # eyes back on the street
    pose.turn('head', axis_angle(LATERAL, 8))


def slide(pose, chest=-32):
    _, pelvis = pose.world('pelvis')
    pose.set_world('pelvis', pos=(pelvis[0], pelvis[1], 34.0))  # hips just above the ground
    pose.aim('spine_01', 'neck_01', (0.0, -0.3, 0.95))         # lean back
    pose.turn('spine_03', axis_angle(LATERAL, chest))           # chest and weapon back to level
    pose.turn('neck_01', axis_angle(LATERAL, 10))
    # left leg stretched out in front, heel on the ground
    pose.aim('thigh_l', 'calf_l', (0.08, 0.93, -0.36))
    pose.aim('calf_l', 'foot_l', (0.05, 0.95, -0.3))
    # right leg folded: knee down and out, shin back under the left knee
    pose.aim('thigh_r', 'calf_r', (-0.4, 0.7, -0.45))
    pose.aim('calf_r', 'foot_r', (0.2, -0.97, 0.05))


def build(source, name, edit):
    seq = u.load_asset(LOCO + source)
    target = FOLDER + '/' + name
    # Every bone track is written below, so an existing asset is simply overwritten.
    new = u.load_asset(target) if u.EditorAssetLibrary.does_asset_exist(target) else u.EditorAssetLibrary.duplicate_asset(LOCO + source, target)
    bones, parents = skeleton_of(seq)
    frames = u.AnimationLibrary.get_num_frames(seq)
    keys = {b: ([], [], []) for b in bones}
    first = None
    for frame in range(frames):
        pose = Pose(seq, bones, parents, frame)
        edit(pose)
        first = first or pose
        for b in bones:
            keys[b][0].append(u.Vector(*pose.pos[b]))
            keys[b][1].append(u.Quat(*pose.rot[b]))
            keys[b][2].append(pose.scale[b])
    controller = new.controller
    controller.open_bracket(u.Text('DH moves'), True)
    for b in bones:
        controller.set_bone_track_keys(b, keys[b][0], keys[b][1], keys[b][2], True)
    controller.close_bracket(True)
    new.set_editor_property('enable_root_motion', False)
    u.EditorAssetLibrary.save_loaded_asset(new, False)
    print('DH moves: %s from %s, %d frames' % (name, source, frames))
    # Bone positions of the first frame for a quick look without the editor (Tools/Python/plot_pose.py). Not evaluated
    # from the new asset: that starts its compression while it is being saved (assertion on exit).
    DUMP[name] = {b: first.world(b)[1] for b in bones}
    source_pose = Pose(seq, bones, parents, 0)
    DUMP[name + ' (source)'] = {b: source_pose.world(b)[1] for b in bones}
    DUMP['parents'] = parents


DUMP = {}


build('Rifle/MM_Rifle_Jog_Fwd', 'A_DH_Sprint_Rifle', sprint)
build('Pistol/MM_Pistol_Jog_Fwd', 'A_DH_Sprint_Pistol', sprint)
build('Rifle/MM_Rifle_Crouch_Idle', 'A_DH_Slide_Rifle', lambda pose: slide(pose, chest=-52))  # the rifle crouch aims higher
build('Pistol/MM_Pistol_Crouch_Idle', 'A_DH_Slide_Pistol', slide)

import json, os
json.dump(DUMP, open(os.path.join(u.Paths.project_saved_dir(), 'moves_pose.json'), 'w'))
