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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import maya.cmds as cmds

import mmSolver.api as mmapi
import mmSolver.logger

LOG = mmSolver.logger.get_logger()


def filter_locators_from_selection():
    """
    Return locator/bundle transforms full path from selection.
    """
    selected_objects = cmds.ls(selection=True, transforms=True, long=True) or []
    locators = []
    for obj in selected_objects:
        if mmapi.get_object_type(obj) == 'bundle':
            locators.append(obj)
            continue
        shapes = cmds.listRelatives(obj, shapes=True, fullPath=True) or []
        if shapes and cmds.objectType(shapes[0]) == 'locator':
            locators.append(obj)
    return locators


def export_survey(file_path, locators_list, names_list):
    with open(file_path, "w") as file:
        for idx, locator in enumerate(locators_list):
            if cmds.objExists(locator):
                position = cmds.xform(locator, query=True,
                                      worldSpace=True,
                                      translation=True)
                name = names_list[idx]
                file.write(
                    "{} {} {} {}\r\n".format(name, position[0],
                                             position[1],
                                             position[2]))
    LOG.warn('Survey text file saved.')
