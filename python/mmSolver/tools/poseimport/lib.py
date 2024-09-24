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

import maya.cmds as cmds

import mmSolver.logger

LOG = mmSolver.logger.get_logger()

TEMP_LOCATOR_NAME = 'pose_temp_locator'

def is_obj_settable(obj):
    attrs_to_check = ['translateX', 'translateY', 'translateZ',
                      'rotateX', 'rotateY', 'rotateZ']
    for attr in attrs_to_check:
        full_attr = '{}.{}'.format(obj, attr)
        is_locked = cmds.getAttr(full_attr, lock=True)
        is_keyable = cmds.getAttr(full_attr, keyable=True)
        is_channel_box = cmds.getAttr(full_attr, channelBox=True)
        is_settable = not is_locked and (is_keyable or is_channel_box)
        if not is_settable:
            return False
    return True


def import_pose(file_path, obj):
    """
    Import pose file.

    :param file_path: File path to import.
    :type file_path: str

    :param obj: Selected object.
    :type obj: str
    """

    with open(file_path, "r") as pose_file:
        pose_data = pose_file.readline().strip().split()
    if len(pose_data) != 6:
        LOG.warn('Invalid Pose.')
        return
    position = list(map(float, pose_data[:3]))
    rotation = list(map(float, pose_data[3:]))
    if not is_obj_settable(obj):
        LOG.warn('Some of translation or rotation attributes are not settable.')
        return
    if cmds.objExists(TEMP_LOCATOR_NAME):
        cmds.delete(TEMP_LOCATOR_NAME)
    temp_loc = cmds.spaceLocator(name=TEMP_LOCATOR_NAME)[0]
    cmds.setAttr(temp_loc + '.rotateOrder', 2)
    cmds.xform(temp_loc, translation=position, worldSpace=True)
    cmds.xform(temp_loc, rotation=rotation, worldSpace=True)
    con = cmds.parentConstraint(temp_loc, obj, maintainOffset=False)
    cmds.setKeyframe(obj, attribute='translate')
    cmds.setKeyframe(obj, attribute='rotate')
    cmds.delete(con)
    cmds.delete(temp_loc)
    cmds.select(obj, replace=True)
