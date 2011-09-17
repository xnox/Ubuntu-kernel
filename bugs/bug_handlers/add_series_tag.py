#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler

class AddSeriesTag(BugHandler):
    """
    Add a tag to the given bug. That tag is the name of the series the bug was
    filed against.
    """

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        BugHandler.__init__(self, cfg, lp, vout)

    # run
    #
    def run(self, bug, task, package_name):
        retval = True

        (series_name, series_version) = bug.series
        if series_name is not None and series_name is not '':
            if series_name not in bug.tags:
                bug.tags.append(series_name)

        return retval

# vi:set ts=4 sw=4 expandtab:
