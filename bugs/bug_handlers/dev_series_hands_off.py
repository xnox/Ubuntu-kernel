#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler
from core.dbg                           import Dbg

class DevSeriesHandsOff(BugHandler):
    """
    """

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        Dbg.enter('DevSeriesHandsOff.__init__')
        BugHandler.__init__(self, cfg, lp, vout)
        Dbg.leave('DevSeriesHandsOff.__init__')

    # run
    #
    def run(self, bug, task, package_name):
        """
        There are certain bugs that we shouldn't mess with. This method tries to
        know about those and returns True if they are supposed to be ignored.
        """
        Dbg.enter('DevSeriesHandsOff.run')

        retval = True

        while True:

            # A bug that is filed for a particular CVE. This is part of the CVE/SRU
            # process.
            #
            if 'kernel-cve-tracking-bug' in bug.tags:
                retval = False
                Dbg.why('kernel-cve-tracking-bug tag exists\n')
                break

            # A bug that is used for workflow processes and is part of the kernel
            # stable teams workflow.
            #
            if 'kernel-release-tracking-bug' in bug.tags:
                retval = False
                Dbg.why('kernel-release-tracking-bug tag exists\n')
                break

            # A bug that is filed for a particular upstream stable release.
            #
            if 'kernel-stable-tracking-bug' in bug.tags:
                retval = False
                Dbg.why('kernel-stable-tracking-bug tag exists\n')
                break

            # The kernel stable team adds this tag onto bugs that get filed as part
            # of a commit revert. It's so the team doesn't forget about the revert.
            #
            if ('stable-next' in bug.tags) or ('kernel-stable-next' in bug.tags):
                retval = False
                Dbg.why('kernel-stable-next tag exists\n')
                break

            # If there is an instance where someone has decided the bot should stop
            # spamming bugs.
            #
            if 'kernel-bot-stop-nagging' in bug.tags:
                retval = False
                Dbg.why('kernel-bot-stop-nagging tag exists\n')
                break

            # As you'd expect, we shouldn't be touching private bugs.
            #
            if bug.private:
                retval = False
                Dbg.why('Private bug\n')
                break

            # We should not be trying to change the status of "bug watch" tasks.
            #
            watch_link = task.bug_watch
            if watch_link is not None:
                retval = False
                Dbg.why('Has a watch link.\n')
                break

            # If the bug has been assigned to someone, leave it alone.
            #
            assignee = task.assignee
            if assignee is not None:
                retval = False
                Dbg.why('Is assigned to someone.\n')
                break

            # If it was submitted against the kernel version that is currently in
            # the 'release' pocket, leave it alone.
            #
            bk = bug.booted_kernel_version
            if bk == self.cfg['released_development_kernel']:
                retval = False
                Dbg.why('The booted kernel version is the same as the version in -release.\n')
                break

            # If we've already asked this kernel version be tested, leave it alone.
            #
            rt = "kernel-request-%s" % self.cfg['released_development_kernel']
            if rt in bug.tags:
                retval = False
                Dbg.why('Already requested version (%s) be tested.\n' % self.cfg['released_development_kernel'])
                break

            break;
        Dbg.ret('DevSeriesHandsOff.run', retval)
        return retval

# vi:set ts=4 sw=4 expandtab:
