"""Per-class clip overrides for the foe batch (#150): the archer's bow draw and the brute's two-handed maul swing.

The foe batch uses the batch 2 override machinery unchanged: overrides.py is not edited (the batch 2 asset records hash it), this module only
names the two foe overrides and builds their pose functions from overrides.py's.

* `Ranged_Archer` (36 frames, replaces the shared Ranged): the same bow draw the ranger has (the left arm raises the bow level at the target, the
  right hand goes to the string, draws it to the cheek, holds, releases and drops), so the release frame and the clip timing are the ranger's and
  everything keyed on them (the lightless arrow that leaves at the release frame, #198) works for the archer unchanged. The archer's shortbow is
  authored upright at the hold (frame 23) with its nock vertex skinned to hand.R, and the hunting knife on weapon_socket is scaled to 1/1000 from
  frame 2 to frame 31, exactly as the ranger's blade is.
* `Melee_Brute` (30 frames, replaces the shared Melee): the barbarian's two-handed swing (coil over the shoulder, chop, follow-through, the left hand
  solved onto the haft every frame) for the brute's stone maul, which is authored along the same 60 degree haft axis as the barbarian's greataxe.
"""
import overrides as OV

# class key -> (shared clip name, action name, frames)
OVERRIDES = {'archer': ('Ranged', 'Ranged_Archer', 36), 'brute': ('Melee', 'Melee_Brute', 30)}
AX_IDLE = OV.AX_IDLE                 # the haft axis in Idle (60 degrees up), shared with the barbarian
HAFT_FROM_GRIP = OV.HAFT_FROM_GRIP   # where the left hand goes on the haft, metres from the right grip towards the butt


def archer_pose_fn(rig):
    """(pose_at(f), scale_at(f), info) for the archer's Ranged_Archer: the ranger's draw."""
    return OV.ranger_pose_fn(rig)


def archer_hold_pose(rig):
    """The archer's pose at full draw (frame 23): the shortbow is authored upright here."""
    return OV.ranger_hold_pose(rig)


def brute_pose_fn(rig, haft_point_idle):
    """(pose_at(f), info) for the brute's Melee_Brute: both hands on the maul's haft."""
    return OV.barbarian_pose_fn(rig, haft_point_idle)
