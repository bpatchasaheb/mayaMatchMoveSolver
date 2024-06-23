# Copyright (C) 2024 David Cattermole
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
The main component of the user interface for the image cache
window.
"""

import os
import collections

import mmSolver.ui.qtpyutils as qtpyutils

qtpyutils.override_binding_order()

import mmSolver.ui.Qt.QtWidgets as QtWidgets
import mmSolver.ui.Qt.QtCore as QtCore

import mmSolver.logger
import mmSolver.utils.config as config_utils
import mmSolver.utils.configmaya as configmaya
import mmSolver.utils.constant as const_utils
import mmSolver.utils.math as math_utils
import mmSolver.tools.imagecache.lib as lib
import mmSolver.tools.imagecacheprefs.constant as const
import mmSolver.tools.imagecacheprefs.ui.ui_imagecacheprefs_layout as ui_imagecacheprefs_layout

LOG = mmSolver.logger.get_logger()


CapacityValue = collections.namedtuple('CapacityValue', ['size_bytes', 'percent'])


def set_memory_total_label(
    label,
    size_bytes,
):
    assert isinstance(label, QtWidgets.QLabel)
    assert isinstance(size_bytes, int)
    gigabytes = 0.0
    if size_bytes > 0:
        gigabytes = size_bytes / const_utils.BYTES_TO_GIGABYTES

    text = '<b>{:0,.2f} GB</b>'.format(gigabytes)
    label.setText(text)


def set_memory_used_label(label, used_size_bytes, total_size_bytes):
    assert isinstance(label, QtWidgets.QLabel)
    assert isinstance(used_size_bytes, int)
    assert isinstance(total_size_bytes, int)
    used_gigabytes = 0.0
    if used_size_bytes > 0:
        used_gigabytes = used_size_bytes / const_utils.BYTES_TO_GIGABYTES

    used_percent = 0.0
    if used_size_bytes > 0 and total_size_bytes > 0:
        used_percent = (used_size_bytes / total_size_bytes) * 100.0

    text = '<b>{:0,.2f} GB ({:3.1f}%)</b>'.format(used_gigabytes, used_percent)
    label.setText(text)


def set_count_label(
    label,
    value,
):
    assert isinstance(label, QtWidgets.QLabel)
    assert isinstance(value, int)
    text = '<b>{:,}</b>'.format(value)
    label.setText(text)


def set_capacity_size_label(
    label,
    size_bytes,
):
    assert isinstance(label, QtWidgets.QLabel)
    assert isinstance(size_bytes, int)
    gigabytes = 0.0
    if size_bytes > 0:
        gigabytes = size_bytes / const_utils.BYTES_TO_GIGABYTES

    text = '<b>{:0,.2f} GB</b>'.format(gigabytes)
    label.setText(text)


def set_used_size_label(label, used_size_bytes, capacity_size_bytes):
    assert isinstance(label, QtWidgets.QLabel)
    assert isinstance(used_size_bytes, int)
    assert isinstance(capacity_size_bytes, int)
    used_gigabytes = 0.0
    if used_size_bytes > 0:
        used_gigabytes = used_size_bytes / const_utils.BYTES_TO_GIGABYTES

    used_percent = 0.0
    if used_size_bytes > 0 and capacity_size_bytes > 0:
        used_percent = (used_size_bytes / capacity_size_bytes) * 100.0

    text = '<b>{:0,.2f} GB ({:3.1f}%)</b>'.format(used_gigabytes, used_percent)
    label.setText(text)


def set_percent_slider(
    slider,
    percent,
):
    assert isinstance(slider, QtWidgets.QSlider)
    assert isinstance(percent, float)

    old_min = 0.0
    old_max = 100.0
    new_min = float(slider.minimum())
    new_max = float(slider.maximum())
    slider_value = math_utils.remap(old_min, old_max, new_min, new_max, percent)
    slider.setValue(int(slider_value))


def set_value_double_spin_box(
    doubleSpinBox,
    value,
):
    assert isinstance(doubleSpinBox, QtWidgets.QDoubleSpinBox)
    assert isinstance(value, float)
    doubleSpinBox.setValue(int(value))


def set_capacity_widgets(
    label, slider, double_spin_box, capacity_bytes, capacity_percent
):
    assert isinstance(capacity_bytes, int)
    assert isinstance(capacity_percent, float)
    set_capacity_size_label(label, capacity_bytes)
    set_percent_slider(slider, capacity_percent)
    set_value_double_spin_box(double_spin_box, capacity_percent)
    return


def get_config():
    """Get the Image Cache config object or None."""
    file_name = const.CONFIG_FILE_NAME
    config_path = config_utils.get_home_dir_path(file_name)
    config = config_utils.Config(config_path)
    config.set_autoread(False)
    config.set_autowrite(False)
    if os.path.isfile(config.file_path):
        config.read()
    return config


def get_config_value(config, key, fallback):
    """Query the attribute from the user's home directory. If the user's
    option is saved, use that value instead.
    """
    value = fallback
    if config is not None:
        value = config.get_value(key, fallback)
    return value


def get_update_every_n_seconds(config, default_value=None):
    if default_value is None:
        default_value = const.CONFIG_UPDATE_EVERY_N_SECONDS_DEFAULT_VALUE
    assert isinstance(default_value, int)
    name = const.CONFIG_UPDATE_EVERY_N_SECONDS_KEY
    seconds = get_config_value(config, name, default_value)
    assert isinstance(seconds, int)
    return seconds


def get_gpu_cache_capacity_percent_default(config, default_value=None):
    if default_value is None:
        default_value = 0.0
    assert isinstance(default_value, float)
    name = const.CONFIG_GPU_CAPACITY_PERCENT_KEY
    percent = get_config_value(config, name, default_value)
    assert isinstance(percent, float)
    ratio = percent / 100.0
    capacity_bytes = lib.get_gpu_memory_total_bytes()
    size_bytes = int(capacity_bytes * ratio)
    return CapacityValue(size_bytes, percent)


def get_gpu_cache_capacity_percent_default(config, default_value=None):
    if default_value is None:
        default_value = 0.0
    assert isinstance(default_value, float)
    name = const.CONFIG_GPU_CAPACITY_PERCENT_KEY
    percent = get_config_value(config, name, default_value)
    assert isinstance(percent, float)
    ratio = percent / 100.0
    capacity_bytes = lib.get_gpu_memory_total_bytes()
    size_bytes = int(capacity_bytes * ratio)
    return CapacityValue(size_bytes, percent)


def get_cpu_cache_capacity_percent_default(config, default_value=None):
    if default_value is None:
        default_value = 0.0
    assert isinstance(default_value, float)
    name = const.CONFIG_CPU_CAPACITY_PERCENT_KEY
    percent = get_config_value(config, name, default_value)
    assert isinstance(percent, float)
    ratio = percent / 100.0
    capacity_bytes = lib.get_cpu_memory_total_bytes()
    size_bytes = int(capacity_bytes * ratio)
    return CapacityValue(size_bytes, percent)


def get_cache_scene_override():
    name = const.CONFIG_SCENE_CAPACITY_OVERRIDE_KEY
    value = configmaya.get_scene_option(name, default=None)
    assert value is None or isinstance(value, bool)
    return value


def get_gpu_cache_capacity_percent_scene(default_value=None):
    if default_value is None:
        default_value = 0.0
    assert isinstance(default_value, float)
    name = const.CONFIG_SCENE_GPU_CAPACITY_PERCENT_KEY
    percent = configmaya.get_scene_option(name, default=default_value)
    assert isinstance(percent, float)
    ratio = percent / 100.0
    capacity_bytes = lib.get_gpu_memory_total_bytes()
    size_bytes = int(capacity_bytes * ratio)
    return CapacityValue(size_bytes, percent)


def get_cpu_cache_capacity_percent_scene(default_value=None):
    if default_value is None:
        default_value = 0.0
    assert isinstance(default_value, float)
    name = const.CONFIG_SCENE_CPU_CAPACITY_PERCENT_KEY
    percent = configmaya.get_scene_option(name, default=default_value)
    assert isinstance(percent, float)
    ratio = percent / 100.0
    capacity_bytes = lib.get_cpu_memory_total_bytes()
    size_bytes = int(capacity_bytes * ratio)
    return CapacityValue(size_bytes, percent)


def set_cache_scene_override(value):
    assert value is None or isinstance(value, bool)
    name = const.CONFIG_SCENE_CAPACITY_OVERRIDE_KEY
    configmaya.set_scene_option(name, value, add_attr=True)
    return


def set_gpu_cache_capacity_percent_scene(percent):
    assert isinstance(percent, float)
    name = const.CONFIG_SCENE_GPU_CAPACITY_PERCENT_KEY
    percent = configmaya.set_scene_option(name, percent, add_attr=True)
    return


def set_cpu_cache_capacity_percent_scene(percent):
    assert isinstance(percent, float)
    name = const.CONFIG_SCENE_CPU_CAPACITY_PERCENT_KEY
    percent = configmaya.set_scene_option(name, percent, add_attr=True)
    return


class ImageCachePrefsLayout(QtWidgets.QWidget, ui_imagecacheprefs_layout.Ui_Form):
    def __init__(self, parent=None, *args, **kwargs):
        super(ImageCachePrefsLayout, self).__init__(*args, **kwargs)
        self.setupUi(self)

        # Update timer.
        self.update_timer = QtCore.QTimer()
        milliseconds = int(const.CONFIG_UPDATE_EVERY_N_SECONDS_DEFAULT_VALUE * 1000)
        self.update_timer.setInterval(milliseconds)
        self.update_timer.timeout.connect(self.update_resource_values)
        self.update_timer.start()

        self._connect_connections()

        # Populate the UI with data
        self.reset_options()

    def _connect_connections(self):
        self.updateEvery_spinBox.valueChanged[int].connect(self._update_every_changed)

        self.gpuCacheDefaultCapacity_doubleSpinBox.valueChanged[float].connect(
            self._gpu_default_capacity_spin_box_changed
        )
        self.cpuCacheDefaultCapacity_doubleSpinBox.valueChanged[float].connect(
            self._cpu_default_capacity_spin_box_changed
        )
        self.gpuCacheSceneCapacity_doubleSpinBox.valueChanged[float].connect(
            self._gpu_scene_capacity_spin_box_changed
        )
        self.cpuCacheSceneCapacity_doubleSpinBox.valueChanged[float].connect(
            self._cpu_scene_capacity_spin_box_changed
        )

        self.gpuCacheDefaultCapacity_horizontalSlider.valueChanged[int].connect(
            self._gpu_default_capacity_slider_changed
        )
        self.cpuCacheDefaultCapacity_horizontalSlider.valueChanged[int].connect(
            self._cpu_default_capacity_slider_changed
        )
        self.gpuCacheSceneCapacity_horizontalSlider.valueChanged[int].connect(
            self._gpu_scene_capacity_slider_changed
        )
        self.cpuCacheSceneCapacity_horizontalSlider.valueChanged[int].connect(
            self._cpu_scene_capacity_slider_changed
        )

    def _gpu_default_capacity_spin_box_changed(self, percent):
        assert isinstance(percent, float)
        LOG.debug('_gpu_default_capacity_spin_box_changed: percent=%r', percent)

        label = self.gpuCacheDefaultCapacityValue_label
        slider = self.gpuCacheDefaultCapacity_horizontalSlider

        gpu_memory_total = lib.get_gpu_memory_total_bytes()
        ratio = percent / 100.0
        size_bytes = int(ratio * gpu_memory_total)
        LOG.debug('_gpu_default_capacity_spin_box_changed: ratio=%r', ratio)
        LOG.debug('_gpu_default_capacity_spin_box_changed: size_bytes=%r', size_bytes)

        set_capacity_size_label(label, size_bytes)

        block = slider.blockSignals(True)
        set_percent_slider(slider, percent)
        slider.blockSignals(block)

    def _cpu_default_capacity_spin_box_changed(self, percent):
        assert isinstance(percent, float)
        LOG.debug('_cpu_default_capacity_spin_box_changed: percent=%r', percent)

        label = self.cpuCacheDefaultCapacityValue_label
        slider = self.cpuCacheDefaultCapacity_horizontalSlider

        cpu_memory_total = lib.get_cpu_memory_total_bytes()
        ratio = percent / 100.0
        size_bytes = int(ratio * cpu_memory_total)
        LOG.debug('_cpu_default_capacity_spin_box_changed: ratio=%r', ratio)
        LOG.debug('_cpu_default_capacity_spin_box_changed: size_bytes=%r', size_bytes)

        set_capacity_size_label(label, size_bytes)

        block = slider.blockSignals(True)
        set_percent_slider(slider, percent)
        slider.blockSignals(block)

    def _gpu_scene_capacity_spin_box_changed(self, percent):
        assert isinstance(percent, float)
        LOG.debug('_gpu_scene_capacity_spin_box_changed: percent=%r', percent)

        label = self.gpuCacheSceneCapacityValue_label
        slider = self.gpuCacheSceneCapacity_horizontalSlider

        gpu_memory_total = lib.get_gpu_memory_total_bytes()
        ratio = percent / 100.0
        size_bytes = int(ratio * gpu_memory_total)
        LOG.debug('_gpu_scene_capacity_spin_box_changed: ratio=%r', ratio)
        LOG.debug('_gpu_scene_capacity_spin_box_changed: size_bytes=%r', size_bytes)

        set_capacity_size_label(label, size_bytes)

        block = slider.blockSignals(True)
        set_percent_slider(slider, percent)
        slider.blockSignals(block)

    def _cpu_scene_capacity_spin_box_changed(self, percent):
        assert isinstance(percent, float)
        LOG.debug('_cpu_scene_capacity_spin_box_changed: percent=%r', percent)

        label = self.cpuCacheSceneCapacityValue_label
        slider = self.cpuCacheSceneCapacity_horizontalSlider

        cpu_memory_total = lib.get_cpu_memory_total_bytes()
        ratio = percent / 100.0
        size_bytes = int(ratio * cpu_memory_total)
        LOG.debug('_cpu_scene_capacity_spin_box_changed: ratio=%r', ratio)
        LOG.debug('_cpu_scene_capacity_spin_box_changed: size_bytes=%r', size_bytes)

        set_capacity_size_label(label, size_bytes)

        block = slider.blockSignals(True)
        set_percent_slider(slider, percent)
        slider.blockSignals(block)

    def _gpu_default_capacity_slider_changed(self, value):
        assert isinstance(value, int)
        LOG.debug('_gpu_default_capacity_slider_changed: value=%r', value)

        label = self.gpuCacheDefaultCapacityValue_label
        spin_box = self.gpuCacheDefaultCapacity_doubleSpinBox
        slider = self.gpuCacheDefaultCapacity_horizontalSlider

        old_min = float(slider.minimum())
        old_max = float(slider.maximum())
        new_min = 0.0
        new_max = 100.0
        percent = math_utils.remap(old_min, old_max, new_min, new_max, float(value))
        ratio = percent / new_max
        LOG.debug('_gpu_default_capacity_slider_changed: percent=%r', percent)

        gpu_memory_total = lib.get_gpu_memory_total_bytes()
        size_bytes = int(ratio * gpu_memory_total)

        set_capacity_size_label(label, size_bytes)

        block = spin_box.blockSignals(True)
        set_value_double_spin_box(spin_box, percent)
        spin_box.blockSignals(block)

    def _cpu_default_capacity_slider_changed(self, value):
        assert isinstance(value, int)
        LOG.debug('_cpu_default_capacity_slider_changed: value=%r', value)

        label = self.cpuCacheDefaultCapacityValue_label
        spin_box = self.cpuCacheDefaultCapacity_doubleSpinBox
        slider = self.cpuCacheDefaultCapacity_horizontalSlider

        old_min = float(slider.minimum())
        old_max = float(slider.maximum())
        new_min = 0.0
        new_max = 100.0
        percent = math_utils.remap(old_min, old_max, new_min, new_max, float(value))
        ratio = percent / new_max
        LOG.debug('_cpu_default_capacity_slider_changed: percent=%r', percent)

        cpu_memory_total = lib.get_cpu_memory_total_bytes()
        size_bytes = int(ratio * cpu_memory_total)

        set_capacity_size_label(label, size_bytes)

        block = spin_box.blockSignals(True)
        set_value_double_spin_box(spin_box, percent)
        spin_box.blockSignals(block)

    def _gpu_scene_capacity_slider_changed(self, value):
        assert isinstance(value, int)
        LOG.debug('_gpu_scene_capacity_slider_changed: value=%r', value)

        label = self.gpuCacheSceneCapacityValue_label
        spin_box = self.gpuCacheSceneCapacity_doubleSpinBox
        slider = self.gpuCacheSceneCapacity_horizontalSlider

        old_min = float(slider.minimum())
        old_max = float(slider.maximum())
        new_min = 0.0
        new_max = 100.0
        percent = math_utils.remap(old_min, old_max, new_min, new_max, float(value))
        ratio = percent / new_max
        LOG.debug('_gpu_scene_capacity_slider_changed: percent=%r', percent)

        gpu_memory_total = lib.get_gpu_memory_total_bytes()
        size_bytes = int(ratio * gpu_memory_total)

        set_capacity_size_label(label, size_bytes)

        block = spin_box.blockSignals(True)
        set_value_double_spin_box(spin_box, percent)
        spin_box.blockSignals(block)

    def _cpu_scene_capacity_slider_changed(self, value):
        assert isinstance(value, int)
        LOG.debug('_cpu_scene_capacity_slider_changed: value=%r', value)

        label = self.cpuCacheSceneCapacityValue_label
        spin_box = self.cpuCacheSceneCapacity_doubleSpinBox
        slider = self.cpuCacheSceneCapacity_horizontalSlider

        old_min = float(slider.minimum())
        old_max = float(slider.maximum())
        new_min = 0.0
        new_max = 100.0
        percent = math_utils.remap(old_min, old_max, new_min, new_max, float(value))
        ratio = percent / new_max
        LOG.debug('_cpu_scene_capacity_slider_changed: percent=%r', percent)

        cpu_memory_total = lib.get_cpu_memory_total_bytes()
        size_bytes = int(ratio * cpu_memory_total)

        set_capacity_size_label(label, size_bytes)

        block = spin_box.blockSignals(True)
        set_value_double_spin_box(spin_box, percent)
        spin_box.blockSignals(block)

    def _update_every_changed(self, value):
        assert isinstance(value, int)
        milliseconds = int(value * 1000)
        self.update_timer.setInterval(milliseconds)

        config = get_config()
        if config is not None:
            config.set_value(const.CONFIG_UPDATE_EVERY_N_SECONDS_KEY, value)
            config.write()
        return

    def _get_capacity_bytes(
        self, scene_override, default_spin_box, scene_spin_box, memory_total
    ):
        percent = default_spin_box.value()
        if scene_override is True:
            percent = scene_spin_box.value()
        ratio = percent / 100.0

        size_bytes = int(ratio * memory_total)
        return size_bytes

    def get_gpu_capacity_bytes(self):
        scene_override = self.imageCacheSceneSettings_groupBox.isChecked()
        default_spin_box = self.gpuCacheDefaultCapacity_doubleSpinBox
        scene_spin_box = self.gpuCacheSceneCapacity_doubleSpinBox
        gpu_memory_total = lib.get_gpu_memory_total_bytes()
        return self._get_capacity_bytes(
            scene_override, default_spin_box, scene_spin_box, gpu_memory_total
        )

    def get_cpu_capacity_bytes(self):
        scene_override = self.imageCacheSceneSettings_groupBox.isChecked()
        default_spin_box = self.cpuCacheDefaultCapacity_doubleSpinBox
        scene_spin_box = self.cpuCacheSceneCapacity_doubleSpinBox
        cpu_memory_total = lib.get_cpu_memory_total_bytes()
        return self._get_capacity_bytes(
            scene_override, default_spin_box, scene_spin_box, cpu_memory_total
        )

    def update_resource_values(self):
        gpu_cache_item_count = lib.get_gpu_cache_item_count()
        cpu_cache_item_count = lib.get_cpu_cache_item_count()
        gpu_cache_group_count = lib.get_gpu_cache_group_count()
        cpu_cache_group_count = lib.get_cpu_cache_group_count()
        gpu_cache_used = lib.get_gpu_cache_used_bytes()
        cpu_cache_used = lib.get_cpu_cache_used_bytes()
        gpu_cache_capacity = lib.get_gpu_cache_capacity_bytes()
        cpu_cache_capacity = lib.get_cpu_cache_capacity_bytes()
        gpu_memory_total = lib.get_gpu_memory_total_bytes()
        cpu_memory_total = lib.get_cpu_memory_total_bytes()
        gpu_memory_used = lib.get_gpu_memory_used_bytes()
        cpu_memory_used = lib.get_cpu_memory_used_bytes()

        set_memory_total_label(self.gpuMemoryTotalValue_label, gpu_memory_total)
        set_memory_total_label(self.cpuMemoryTotalValue_label, cpu_memory_total)

        set_memory_used_label(
            self.gpuMemoryUsedValue_label, gpu_memory_used, gpu_memory_total
        )
        set_memory_used_label(
            self.cpuMemoryUsedValue_label, cpu_memory_used, cpu_memory_total
        )

        set_count_label(self.gpuCacheItemCountValue_label, gpu_cache_item_count)
        set_count_label(self.cpuCacheItemCountValue_label, cpu_cache_item_count)

        set_count_label(self.gpuCacheGroupCountValue_label, gpu_cache_group_count)
        set_count_label(self.cpuCacheGroupCountValue_label, cpu_cache_group_count)

        set_capacity_size_label(self.gpuCacheCapacityValue_label, gpu_cache_capacity)
        set_capacity_size_label(self.cpuCacheCapacityValue_label, cpu_cache_capacity)

        set_used_size_label(
            self.cpuCacheUsedValue_label, cpu_cache_used, cpu_cache_capacity
        )
        set_used_size_label(
            self.gpuCacheUsedValue_label, gpu_cache_used, gpu_cache_capacity
        )
        return

    def reset_options(self):
        """
        Reset the UI the values.

        If scene override is enabled, get the scene option values,
        otherwise use the default values saved in the config file.
        """
        self.update_resource_values()

        config = get_config()
        update_seconds = get_update_every_n_seconds(config)
        self.updateEvery_spinBox.setValue(update_seconds)

        default_gpu_capacity = get_gpu_cache_capacity_percent_default(config)
        default_cpu_capacity = get_cpu_cache_capacity_percent_default(config)

        scene_override = get_cache_scene_override()
        scene_gpu_capacity = default_gpu_capacity
        scene_cpu_capacity = default_cpu_capacity
        if scene_override is True:
            scene_gpu_capacity = get_gpu_cache_capacity_percent_scene()
            scene_cpu_capacity = get_cpu_cache_capacity_percent_scene()

        # Set default capacities.
        set_capacity_widgets(
            self.gpuCacheDefaultCapacityValue_label,
            self.gpuCacheDefaultCapacity_horizontalSlider,
            self.gpuCacheDefaultCapacity_doubleSpinBox,
            default_gpu_capacity.size_bytes,
            default_gpu_capacity.percent,
        )
        set_capacity_widgets(
            self.cpuCacheDefaultCapacityValue_label,
            self.cpuCacheDefaultCapacity_horizontalSlider,
            self.cpuCacheDefaultCapacity_doubleSpinBox,
            default_cpu_capacity.size_bytes,
            default_cpu_capacity.percent,
        )

        # Scene Override?
        self.imageCacheSceneSettings_groupBox.setChecked(scene_override is True)

        # Set scene capacities.
        set_capacity_widgets(
            self.gpuCacheSceneCapacityValue_label,
            self.gpuCacheSceneCapacity_horizontalSlider,
            self.gpuCacheSceneCapacity_doubleSpinBox,
            scene_gpu_capacity.size_bytes,
            scene_gpu_capacity.percent,
        )
        set_capacity_widgets(
            self.cpuCacheSceneCapacityValue_label,
            self.cpuCacheSceneCapacity_horizontalSlider,
            self.cpuCacheSceneCapacity_doubleSpinBox,
            scene_cpu_capacity.size_bytes,
            scene_cpu_capacity.percent,
        )
        return

    def save_options(self):
        # Save config values in config file.
        update_seconds = self.updateEvery_spinBox.value()
        gpu_percent_default = self.gpuCacheDefaultCapacity_doubleSpinBox.value()
        cpu_percent_default = self.cpuCacheDefaultCapacity_doubleSpinBox.value()
        config = get_config()
        if config is not None:
            config.set_value(const.CONFIG_UPDATE_EVERY_N_SECONDS_KEY, update_seconds)
            config.set_value(const.CONFIG_GPU_CAPACITY_PERCENT_KEY, gpu_percent_default)
            config.set_value(const.CONFIG_CPU_CAPACITY_PERCENT_KEY, cpu_percent_default)
            config.write()

        # Save config values in Maya Scene.
        scene_override = self.imageCacheSceneSettings_groupBox.isChecked()
        assert isinstance(scene_override, bool)
        gpu_percent_scene = self.gpuCacheSceneCapacity_doubleSpinBox.value()
        cpu_percent_scene = self.cpuCacheSceneCapacity_doubleSpinBox.value()
        set_cache_scene_override(scene_override)
        if scene_override is True:
            set_gpu_cache_capacity_percent_scene(gpu_percent_scene)
            set_cpu_cache_capacity_percent_scene(cpu_percent_scene)
        return
