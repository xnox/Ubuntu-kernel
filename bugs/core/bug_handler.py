#!/usr/bin/env python
#

from plugin_mount                       import PluginMount
from ktl.utils                          import stdo

class BugHandler:
    """
    This is the base class for BugEngine plugins. Just derive from this class
    and your plugin is automatically published.
    """
    __metaclass__ = PluginMount

    verbose = False

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        self.lp = lp
        self.show_buff = ''
        self.vout = vout
        self.cfg = cfg

    def show(self):
        stdo(self.show_buf)

# vi:set ts=4 sw=4 expandtab:
