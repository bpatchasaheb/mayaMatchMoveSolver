# Copyright (C) 2018, 2019 David Cattermole.
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
Test functions for loadmarker tool module.
"""

import os
import unittest

import maya.cmds

import test.test_tools.toolsutils as test_tools_utils
import mmSolver.tools.loadmarker.lib.mayareadfile as marker_read
import mmSolver.tools.loadmarker.lib.interface as interface
import mmSolver.tools.loadmarker.lib.utils as lib_utils
import mmSolver.tools.loadmarker.lib.fileutils as lib_fileutils
import mmSolver.tools.createmarker.tool as create_marker


# @unittest.skip
class TestLoadMarker(test_tools_utils.ToolsTestCase):

    def test_get_cameras(self):
        """
        Get all cameras.
        """
        cameras_a = lib_utils.get_cameras()
        assert len(cameras_a) == 0
        tfm = maya.cmds.createNode('transform')
        shp = maya.cmds.createNode('camera', parent=tfm)
        cameras_b = lib_utils.get_cameras()
        assert len(cameras_b) == 1
        return

    def test_get_selected_cameras(self):
        tfm = maya.cmds.createNode('transform')
        shp = maya.cmds.createNode('camera', parent=tfm)
        maya.cmds.select(tfm, replace=True)
        cameras = lib_utils.get_selected_cameras()
        assert len(cameras) == 1
        return

    def test_file_path(self):
        values = (
            ('match_mover', 'loadmarker.rz2'),
            ('uvtrack', 'test_v1.uv'),
            ('uvtrack', 'test_v3.uv'),
            ('uvtrack', 'test_v4.uv'),
        )
        for dir_name, file_name in values:
            path = self.get_data_path(dir_name, file_name)

            valid = lib_fileutils.is_valid_file_path(path)
            assert valid is True

            fmt = lib_fileutils.get_file_path_format(path)
            assert fmt is not None

            file_info = lib_fileutils.get_file_info(path)
            assert isinstance(file_info, interface.FileInfo)

            file_info_data = lib_fileutils.get_file_info_strings(path)
            assert isinstance(file_info_data, dict)
            assert len(file_info_data) == 11
            keys = file_info_data.keys()
            assert 'fmt' in keys
            assert 'fmt_name' in keys
            assert 'num_points' in keys
            assert 'point_names' in keys
            assert 'start_frame' in keys
            assert 'end_frame' in keys
            assert 'frame_range' in keys
            assert 'lens_dist' in keys
            assert 'lens_undist' in keys
            assert 'positions' in keys
            assert 'has_camera_fov' in keys

            start_dir = lib_utils.get_start_directory(path)
            assert os.path.isdir(start_dir) is True
        return

    def test_create_new_camera(self):
        cam = lib_utils.create_new_camera()
        assert cam
        return

    def test_create_new_marker_group(self):
        cam = lib_utils.create_new_camera()
        mkr_grp = lib_utils.create_new_marker_group(cam)
        assert mkr_grp
        return

    def test_get_default_image_resolution(self):
        w, h = lib_utils.get_default_image_resolution()
        assert isinstance(w, (int, long))
        assert isinstance(h, (int, long))
        return

    def test_update_nodes(self):
        file_names = [
            'test_v1.uv',
            'test_v3.uv',  # only contains 1 point
            'test_v3_with_3d_point.uv',
            'test_v4.uv',  # only contains 1 point
        ]
        for file_name in file_names:
            path = self.get_data_path('uvtrack', file_name)
            create_marker.main()
            # mkr =
            # mkr_list = [mkr]
            _, mkr_data_list = marker_read.read(path)
            mkr_list = lib_utils.get_selected_markers()
            marker_read.update_nodes(mkr_list, mkr_data_list, load_bundle_position=False)
            marker_read.update_nodes(mkr_list, mkr_data_list, load_bundle_position=True)
        return

    def test_loadmarker_rz2_format(self):
        cam = lib_utils.create_new_camera()
        mkr_grp = lib_utils.create_new_marker_group(cam)

        mkr_data_list = []
        paths = [
            self.get_data_path('match_mover', 'loadmarker.rz2'),
            self.get_data_path('match_mover', 'loadmarker_corners.rz2'),
            # self.get_data_path('match_mover', '2dtracks.rz2'),
            # self.get_data_path('match_mover', 'cha_171_1020_atb_v001.rz2'),
            # self.get_data_path('match_mover', 'EP_1000_head_trackers_v002.rz2'),
            # self.get_data_path('match_mover', 'kipPointsMatchmover.rz2'),
            # self.get_data_path('match_mover', 'NonSequentialMatchmoverPoints.rz2'),
        ]
        for path in paths:
            _, tmp_list = marker_read.read(path)
            mkr_data_list += tmp_list

        # Create the markers
        num_nodes1 = len(maya.cmds.ls())
        marker_read.create_nodes(mkr_data_list, cam=cam, mkr_grp=mkr_grp)
        num_nodes2 = len(maya.cmds.ls())
        self.assertGreater(num_nodes2, num_nodes1)

        self.assertTrue(maya.cmds.objExists('TopLeft_MKR'))
        self.assertEqual(maya.cmds.getAttr('TopLeft_MKR.translateX'), -0.5)
        self.assertEqual(maya.cmds.getAttr('TopLeft_MKR.translateY'), 0.5)

        self.assertTrue(maya.cmds.objExists('TopRight_MKR'))
        self.assertEqual(maya.cmds.getAttr('TopRight_MKR.translateX'), 0.5)
        self.assertEqual(maya.cmds.getAttr('TopRight_MKR.translateY'), 0.5)

        self.assertTrue(maya.cmds.objExists('BottomLeft_MKR'))
        self.assertEqual(maya.cmds.getAttr('BottomLeft_MKR.translateX'), -0.5)
        self.assertEqual(maya.cmds.getAttr('BottomLeft_MKR.translateY'), -0.5)

        self.assertTrue(maya.cmds.objExists('BottomRight_MKR'))
        self.assertEqual(maya.cmds.getAttr('BottomRight_MKR.translateX'), 0.5)
        self.assertEqual(maya.cmds.getAttr('BottomRight_MKR.translateY'), -0.5)

    def test_loadmarker_uvtrack_format(self):
        """
        Test loading markers using the '.uv' format.
        """
        cam = lib_utils.create_new_camera()
        mkr_grp = lib_utils.create_new_marker_group(cam)

        mkr_data_list = []
        paths = [
            self.get_data_path('uvtrack', 'test_v1.uv'),
            self.get_data_path('uvtrack', 'test_v3.uv'),
            self.get_data_path('uvtrack', 'test_v4.uv'),
            self.get_data_path('uvtrack', 'loadmarker_corners.uv'),
            self.get_data_path('uvtrack', 'cameraTrackRnD.uv'),
            self.get_data_path('uvtrack', 'stA.uv'),
            self.get_data_path('uvtrack', 'stA_with_emptyMarker.uv'),
            self.get_data_path('uvtrack', 'eye_fmt1_v001.uv'),
            self.get_data_path('uvtrack', 'eye_fmt2_v001.uv'),
        ]
        for path in paths:
            print('Reading... %r' % path)
            _, tmp_list = marker_read.read(path)
            self.assertNotEqual(tmp_list, None)
            mkr_data_list += tmp_list

        # Create the markers
        num_nodes1 = len(maya.cmds.ls())
        marker_read.create_nodes(mkr_data_list, cam=cam, mkr_grp=mkr_grp)
        num_nodes2 = len(maya.cmds.ls())
        self.assertGreater(num_nodes2, num_nodes1)

        self.assertTrue(maya.cmds.objExists('TopLeft_MKR'))
        self.assertEqual(maya.cmds.getAttr('TopLeft_MKR.translateX'), -0.5)
        self.assertEqual(maya.cmds.getAttr('TopLeft_MKR.translateY'), 0.5)

        self.assertTrue(maya.cmds.objExists('TopRight_MKR'))
        self.assertEqual(maya.cmds.getAttr('TopRight_MKR.translateX'), 0.5)
        self.assertEqual(maya.cmds.getAttr('TopRight_MKR.translateY'), 0.5)

        self.assertTrue(maya.cmds.objExists('BottomLeft_MKR'))
        self.assertEqual(maya.cmds.getAttr('BottomLeft_MKR.translateX'), -0.5)
        self.assertEqual(maya.cmds.getAttr('BottomLeft_MKR.translateY'), -0.5)

        self.assertTrue(maya.cmds.objExists('BottomRight_MKR'))
        self.assertEqual(maya.cmds.getAttr('BottomRight_MKR.translateX'), 0.5)
        self.assertEqual(maya.cmds.getAttr('BottomRight_MKR.translateY'), -0.5)

    def test_loadmarker_tdetxt_format(self):
        """
        Test loading markers using the '.uv' format.
        """
        cam = lib_utils.create_new_camera()
        mkr_grp = lib_utils.create_new_marker_group(cam)

        mkr_data_list = []
        paths = [
            (self.get_data_path('3de_v4', 'loadmarker_corners.txt'), (1920.0, 1080.0)),
            # (self.get_data_path('3de_v4', '3de_export_cube.txt'), (1920.0, 1080.0)),
            # (self.get_data_path('3de_v4', 'FB1880_man_v05.txt'), (1920.0, 1080.0)),
        ]
        for path, res in paths:
            print('Reading... %r' % path)
            _, tmp_list = marker_read.read(
                path,
                image_width=res[0],
                image_height=res[1]
            )
            self.assertNotEqual(tmp_list, None)
            mkr_data_list += tmp_list

        # Create the markers
        num_nodes1 = len(maya.cmds.ls())
        marker_read.create_nodes(mkr_data_list, cam=cam, mkr_grp=mkr_grp)
        num_nodes2 = len(maya.cmds.ls())
        self.assertGreater(num_nodes2, num_nodes1)

        self.assertTrue(maya.cmds.objExists('TopLeft_MKR'))
        self.assertEqual(maya.cmds.getAttr('TopLeft_MKR.translateX'), -0.5)
        self.assertEqual(maya.cmds.getAttr('TopLeft_MKR.translateY'), 0.5)

        self.assertTrue(maya.cmds.objExists('TopRight_MKR'))
        self.assertEqual(maya.cmds.getAttr('TopRight_MKR.translateX'), 0.5)
        self.assertEqual(maya.cmds.getAttr('TopRight_MKR.translateY'), 0.5)

        self.assertTrue(maya.cmds.objExists('BottomLeft_MKR'))
        self.assertEqual(maya.cmds.getAttr('BottomLeft_MKR.translateX'), -0.5)
        self.assertEqual(maya.cmds.getAttr('BottomLeft_MKR.translateY'), -0.5)

        self.assertTrue(maya.cmds.objExists('BottomRight_MKR'))
        self.assertEqual(maya.cmds.getAttr('BottomRight_MKR.translateX'), 0.5)
        self.assertEqual(maya.cmds.getAttr('BottomRight_MKR.translateY'), -0.5)


if __name__ == '__main__':
    prog = unittest.main()
