# Copyright (C) 2024 Patcha Saheb Binginapalli.
#
# This file is part of mmSolver.
#
# mmSolver is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# mmSolver is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with mmSolver.  If not, see <https://www.gnu.org/licenses/>.
#

import maya.api.OpenMaya as om
import maya.cmds as cmds


def export_pose(file_path, obj):
    """
    Export pose file.

    :param file_path: File path to export.
    :type file_path: str

    :param obj: Selected object.
    :type obj: str
    """
    position = cmds.xform(obj, query=True, translation=True, worldSpace=True)
    dag_path = om.MSelectionList().add(obj).getDagPath(0)
    mfn_transform = om.MFnTransform(dag_path)
    rotation = mfn_transform.rotation(om.MSpace.kTransform)
    rotation = rotation.reorder(om.MEulerRotation.kZXY)
    rotation_zxy = [om.MAngle(angle).asDegrees() for angle in
                    (rotation.x, rotation.y, rotation.z)]
    pose_data = position + rotation_zxy
    with open(file_path, "w") as pose_file:
        pose_file.write(" ".join(map(str, pose_data)) + '\n')
