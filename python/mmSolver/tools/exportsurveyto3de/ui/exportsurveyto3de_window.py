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
"""
Window for the Export Survey To 3de tool.

Usage::

   import mmSolver.tools.exportsurveyto3de.ui.exportsurveyto3de_window as exportsurveyto3de_window
   exportsurveyto3de_window.main()

"""

import mmSolver.ui.Qt.QtCore as QtCore
import mmSolver.ui.qtpyutils as qtpyutils

qtpyutils.override_binding_order()

import mmSolver.ui.uiutils as uiutils
import mmSolver.tools.exportsurveyto3de.ui.exportsurveyto3de_layout as exportsurveyto3de_layout
import mmSolver.tools.exportsurveyto3de.lib as lib
import mmSolver.tools.exportsurveyto3de.constant as const

import mmSolver.logger

LOG = mmSolver.logger.get_logger()
baseModule, BaseWindow = uiutils.getBaseWindow()


class ExportSurveyTo3deWindow(BaseWindow):
    name = 'ExportSurveyTo3deWindow'

    def __init__(self, parent=None, name=None):
        super(ExportSurveyTo3deWindow, self).__init__(parent, name=name)
        self.setupUi(self)
        self.addSubForm(exportsurveyto3de_layout.ExportSurveyTo3deLayout)

        self.setWindowTitle(const.WINDOW_TITLE)
        self.setWindowFlags(QtCore.Qt.Tool)

        # Hide irrelevant stuff
        self.baseHideStandardButtons()
        self.baseHideProgressBar()


def main(show=True, auto_raise=True, delete=False):
    """
    Open the Export Survey To 3DE UI.

    :param show: Show the UI.
    :type show: bool

    :param auto_raise: If the UI is open, raise it to the front?
    :type auto_raise: bool

    :param delete: Delete the existing UI and rebuild it? Helpful when
                   developing the UI in Maya script editor.
    :type delete: bool

    :returns: A new ui window, or None if the window cannot be
              opened.
    :rtype: ExportSurveyTo3DEWindow or None
    """
    win = ExportSurveyTo3deWindow.open_window(
        show=show, auto_raise=auto_raise, delete=delete
    )
    return win
