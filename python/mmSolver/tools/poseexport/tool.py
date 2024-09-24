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

import mmSolver.tools.poseexport.lib as lib


def main():
    selection = cmds.ls(selection=True, transforms=True)
    if not selection:
        LOG.warn("No object selected.")
        return
    if len(selection) != 1:
        LOG.warn("Select one object only.")
        return
    file_path = cmds.fileDialog2(fileMode=0,
                                 caption="Export Pose File",
                                 fileFilter="*.pose",
                                 dialogStyle=2)
    if not file_path:
        return
    lib.export_pose(file_path[0], selection[0])
    LOG.warn("Pose file exported.")
