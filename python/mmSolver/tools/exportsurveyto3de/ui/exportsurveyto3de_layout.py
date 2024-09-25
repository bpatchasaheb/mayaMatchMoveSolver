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
The main component of the user interface for the import/export survey to 3de
window.
"""
import os
import re

import mmSolver.ui.Qt.QtWidgets as QtWidgets
import mmSolver.ui.Qt.QtCore as QtCore
import mmSolver.ui.qtpyutils as qtpyutils

qtpyutils.override_binding_order()

import \
    mmSolver.tools.exportsurveyto3de.ui.ui_exportsurveyto3de_layout as ui_layout
import mmSolver.tools.exportsurveyto3de.lib as lib
import mmSolver.tools.exportsurveyto3de.constant as const
import mmSolver.logger

import maya.cmds as cmds

LOG = mmSolver.logger.get_logger()

current_file_path = cmds.file(query=True, sceneName=True)
file_dir = os.path.dirname(current_file_path)


# noinspection PyMethodMayBeStatic
class ExportSurveyTo3deLayout(QtWidgets.QWidget, ui_layout.Ui_Form):
    def __init__(self, parent=None, *args, **kwargs):
        super(ExportSurveyTo3deLayout, self).__init__(*args, **kwargs)
        self.setupUi(self)

        self.create_connections()
        self.populate_ui()

    def create_connections(self):
        self.exportBrowseBtnWdgt.clicked.connect(self.export_browse_btn_clicked)
        self.modifyNamesBtnWdgt.clicked.connect(
            self.modify_names_btn_clicked)
        self.addLocatorsBtnWdgt.clicked.connect(self.add_locators_btn_clicked)
        self.selectAllBtnWdgt.clicked.connect(self.select_all_btn_clicked)
        self.exportBtnWdgt.clicked.connect(self.export_btn_clicked)
        self.importBrowseBtnWdgt.clicked.connect(self.import_browse_btn_clicked)

        self.importOptionsComboWdgt.currentIndexChanged.connect(
            self.import_options_index_changed)

    def populate_ui(self):
        # Disable combo box empty item
        self.importOptionsComboWdgt.model().item(0).setEnabled(False)
        self.importOptionsComboWdgt.model().item(0).setSelectable(False)
        # Set current tab
        self.importExportSurveyTabWdgt.setCurrentIndex(1)

    def ui_get_current_tab_name(self, tab_widget):
        current_index = tab_widget.currentIndex()
        return tab_widget.tabText(current_index)

    def ui_construct_export_name(self, name):
        """ Return list widget item label """
        name_output = name.split('|')[-1]
        name_final = name + const.ARROW + name_output
        return name_final

    def ui_get_maya_names(self, list_widget, import_tab=True, export_tab=False,
                          selected_items=True):
        """ Return selected/all maya names list from list widget """
        items = list_widget.selectedItems() or []
        if not selected_items:
            items = self.get_all_list_widget_items(list_widget)
        if export_tab:
            return [item.text().split(const.ARROW)[0] for item in items]
        elif import_tab:
            return [item.text().split(const.ARROW)[-1] for item in items]

    def ui_get_3de_names(self, list_widget, import_tab=True, export_tab=False,
                         selected_items=True):
        """ Return selected/all 3de names list from list widget """
        items = list_widget.selectedItems() or []
        if not selected_items:
            items = self.get_all_list_widget_items(list_widget)
        if export_tab:
            return [item.text().split(const.ARROW)[-1] for item in items]
        elif import_tab:
            return [item.text().split(const.ARROW)[0] for item in items]

    def locator_exists_in_global_space(self, locator_name):
        """ Return True if locator exists in global space without parent """
        locator_name = '|' + locator_name
        if cmds.objExists(locator_name):
            parent = cmds.listRelatives(locator_name, parent=True)
            if parent is None:
                return True
        return False

    def starts_with_number(self, name):
        """ Return True if name starts with digit """
        return name[0].isdigit() if name else False

    def export_browse_btn_clicked(self):
        file_path = cmds.fileDialog2(caption=const.WINDOW_TITLE_EXPORT,
                                     dir=file_dir,
                                     fileMode=0,
                                     fileFilter='Text Files (*.txt);;All Files(*.*)')
        if not file_path:
            return
        self.exportFilePathLineEditWdgt.setText(file_path[0])

    def import_browse_btn_clicked(self):
        file_path = cmds.fileDialog2(caption=const.WINDOW_TITLE_IMPORT,
                                     dir=file_dir,
                                     fileMode=1,
                                     fileFilter='Text Files (*.txt);;All Files(*.*)')
        if not file_path:
            return
        # Set file path text
        self.importFilePathLineEditWdgt.setText(file_path[0])
        # Clear all items
        self.importListWdgt.clear()
        # Read file
        with open(file_path[0], "r") as file:
            for line in file:
                line_split = line.strip().split()
                if len(line_split) != 4:
                    continue
                name, pos_x, pos_y, pos_z = line_split
                # Create item data
                pos_data = {'position': [pos_x, pos_y, pos_z]}
                # Create item
                self.create_list_widget_item(self.importListWdgt,
                                             name + const.ARROW + name,
                                             pos_data)
        # Set options index to 1
        self.importOptionsComboWdgt.setCurrentIndex(
            const.INDEX_NEW_LOCATORS)

    def add_locators_btn_clicked(self):
        locator_tfms = lib.filter_locators_from_selection()
        if not locator_tfms:
            LOG.warn('Locator selection not found.')
            return
        self.exportListWdgt.clear()
        for locator_name in locator_tfms:
            item_name = self.ui_construct_export_name(locator_name)
            # Create item data
            data = {}
            # Create item
            self.create_list_widget_item(self.exportListWdgt, item_name, data)

    def create_list_widget_item(self, list_widget, name, data):
        item = QtWidgets.QListWidgetItem(name)
        list_widget.addItem(item)
        item.setSelected(True)
        item.setData(QtCore.Qt.UserRole, data)

    def get_all_list_widget_items(self, list_widget):
        items = []
        for index in range(list_widget.count()):
            items.append(list_widget.item(index))
        return items

    def export_btn_clicked(self):
        file_path = self.exportFilePathLineEditWdgt.text()
        list_widget = self.get_list_widget_from_current_tab()
        items = list_widget.selectedItems() or []
        if not file_path:
            LOG.warn('Export file path not found.')
            return
        if not items:
            LOG.warn('Selection not found in UI.')
            return
        locator_list = self.ui_get_maya_names(self.exportListWdgt, False, True)
        names_list = self.ui_get_3de_names(self.exportListWdgt, False, True)
        lib.export_survey(file_path, locator_list, names_list)

    def get_list_widget_from_current_tab(self):
        """ Return list widget from current tab """
        if self.ui_get_current_tab_name(
                self.importExportSurveyTabWdgt) == const.TAB_NAME_EXPORT:
            return self.exportListWdgt
        elif self.ui_get_current_tab_name(
                self.importExportSurveyTabWdgt) == const.TAB_NAME_IMPORT:
            return self.importListWdgt

    def select_all_btn_clicked(self):
        list_widget = self.get_list_widget_from_current_tab()
        list_widget.selectAll()

    def ui_import_need_new_locator(self, name):
        """ Return True if maya name needs to be new locator """
        if self.starts_with_number(
                name) or self.locator_exists_in_global_space(name):
            return True
        return False

    def import_options_index_changed(self):
        if self.importOptionsComboWdgt.currentIndex() == const.INDEX_NEW_LOCATORS:
            # Get selected items
            all_items = self.get_all_list_widget_items(
                self.importListWdgt) or []
            # Get 3de names
            names_list = self.ui_get_3de_names(self.importListWdgt,
                                               import_tab=True,
                                               export_tab=False,
                                               selected_items=False)
            # Set new items names
            for idx, item in enumerate(all_items):
                # If name starts with digit or exists already globally then
                # maya name is new locator
                if self.ui_import_need_new_locator(names_list[idx]):
                    new_item_name = names_list[
                                    idx] + const.ARROW + const.NEW_LOCATOR_NAME
                # Else keep current name
                else:
                    new_item_name = names_list[idx] + const.ARROW + names_list[
                        idx]
                item.setText(new_item_name)

        elif self.importOptionsComboWdgt.currentIndex() == const.INDEX_REPLACE_LOCATORS:
            print('Replace Selected Locators')

    def modify_names_btn_clicked(self):
        search_value = self.searchLineEditWdgt.text()
        replace_value = self.replaceLineEditWdgt.text()
        list_widget = self.get_list_widget_from_current_tab()
        selected_items = list_widget.selectedItems() or []
        if not selected_items:
            LOG.warn('Selection not found in UI.')
            return

        locator_list = self.ui_get_maya_names(self.exportListWdgt, False, True)
        names_list = self.ui_get_3de_names(self.exportListWdgt, False, True)

        # Export list widget
        # if list_widget == self.exportListWdgt:
        for idx, item in enumerate(selected_items):
            output_name = names_list[idx].replace(search_value,
                                                  replace_value, 1)
            new_item_name = locator_list[idx] + const.ARROW + output_name
            item.setText(new_item_name)

        # Import list widget
        # elif list_widget == self.importListWdgt:
        #     pass