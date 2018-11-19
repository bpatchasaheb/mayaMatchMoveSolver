"""
Attribute nodes for the mmSolver Window UI.
"""

import mmSolver.ui.uimodels as uimodels
import mmSolver.ui.nodes as nodes


class PlugNode(nodes.Node):
    def __init__(self, name,
                 parent=None,
                 data=None,
                 icon=None,
                 enabled=True,
                 editable=False,
                 selectable=True,
                 checkable=False,
                 neverHasChildren=False):
        if icon is None:
            icon = ':/plug.png'
        super(PlugNode, self).__init__(
            name,
            data=data,
            parent=parent,
            icon=icon,
            enabled=enabled,
            selectable=selectable,
            editable=editable,
            checkable=checkable,
            neverHasChildren=neverHasChildren)
        self.typeInfo = 'plug'

    def state(self):
        # TODO: Get the state.
        return ''

    def minValue(self):
        # TODO: Get the min value.
        return ''

    def maxValue(self):
        # TODO: Get the max value.
        return ''


class AttrNode(PlugNode):
    def __init__(self, name,
                 data=None,
                 parent=None):
        icon = ':/attr.png'
        super(AttrNode, self).__init__(
            name,
            data=data,
            parent=parent,
            icon=icon,
            selectable=True,
            editable=False)
        self.typeInfo = 'attr'

    def state(self):
        d = self.data().get('data')
        state = 'Invalid'
        if d is None:
            return invalid_state
        if d.is_static() is True:
            return 'Static'
        if d.is_animated() is True:
            return 'Animated'
        if d.is_locked() is True:
            return 'Locked'
        return invalid_state

    def minValue(self):
        d = self.data().get('data')
        if d is None:
            return ''
        v = d.get_min_value()
        if v is None:
            return ''
        return str(v)

    def maxValue(self):
        d = self.data().get('data')
        if d is None:
            return ''
        v = d.get_max_value()
        if v is None:
            return ''
        return str(v)

    def mayaNodeName(self):
        return 'node'

    def mayaAttrName(self):
        return 'attr'

    def mayaPlug(self):
        return None


class MayaNode(PlugNode):
    def __init__(self, name,
                 data=None,
                 parent=None):
        icon = ':/node.png'
        super(MayaNode, self).__init__(
            name,
            data=data,
            parent=parent,
            icon=icon,
            selectable=True,
            editable=False)
        self.typeInfo = 'node'

    def mayaNodeName(self):
        return 'node'

    def mayaAttrName(self):
        return 'attr'

    def mayaPlug(self):
        return None


class AttrModel(uimodels.ItemModel):
    def __init__(self, root, font=None):
        super(AttrModel, self).__init__(root, font=font)
        self._column_names = {
            0: 'Attr',
            1: 'State',
            2: 'Min',
            3: 'Max',
        }
        self._node_attr_key = {
            'Attr': 'name',
            'State': 'state',
            'Min': 'minValue',
            'Max': 'maxValue',
        }


