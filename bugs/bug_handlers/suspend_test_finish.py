#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler

class SuspendTestFinish(BugHandler):
    """
    Look st suspend-test-finish.py in the kernel team's arsenal scripts.

    Look at LP#s: 464552, 279035, 568264
    """

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        BugHandler.__init__(self, cfg, lp, vout)

    # run
    #
    def run(self, bug, task, package_name):
        retval = True
        return retval

# vi:set ts=4 sw=4 expandtab:
