#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler

class NewHandsOff(BugHandler):
    """
    """

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        BugHandler.__init__(self, cfg, lp, vout)

    # run
    #
    def run(self, bug, task, package_name):
        """
        There are certain bugs that we shouldn't mess with. This method tries to
        know about those and returns True if they are supposed to be ignored.
        """
        retval = True

        while True:

            # A bug that is filed for a particular CVE. This is part of the CVE/SRU
            # process.
            #
            if 'kernel-cve-tracking-bug' in bug.tags:
                retval = False
                break

            # A bug that is used for workflow processes and is part of the kernel
            # stable teams workflow.
            #
            if 'kernel-release-tracking-bug' in bug.tags:
                retval = False
                break

            # The kernel stable team adds this tag onto bugs that get filed as part
            # of a commit revert. It's so the team doesn't forget about the revert.
            #
            if ('stable-next' in bug.tags) or ('kernel-stable-next' in bug.tags):
                retval = False
                break

            # As you'd expect, we shouldn't be touching private bugs.
            #
            if bug.private:
                retval = False
                break

            # We should not be trying to change the status of "bug watch" tasks.
            #
            watch_link = task.bug_watch
            if watch_link is not None:
                retval = False
                break

            # If the bug has been assigned to someone, leave it alone.
            #
            assignee = task.assignee
            if assignee is not None:
                retval = False
                break

            # If it was submitted against the kernel version that is currently in
            # the 'release' pocket, leave it alone.
            #
            bk = bug.booted_kernel_version
            if bk == self.cfg['released_development_kernel']:
                retval = False
                break

            # If we've already asked this kernel version be tested, leave it alone.
            #
            rt = "kernel-request-%s" % bk
            if rt in bug.tags:
                retval = False
                break

            break;
        return retval

# vi:set ts=4 sw=4 expandtab:
