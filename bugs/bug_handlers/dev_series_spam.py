#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler
from core.dbg                           import Dbg

from ktl.termcolor                      import colored

class DevSeriesSpammer(BugHandler):
    """
    Add a tag to the given bug. That tag is the name of the series the bug was
    filed against.
    """

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        BugHandler.__init__(self, cfg, lp, vout)

        self.comment = "Thank you for taking the time to file a bug report on this issue.\n\nHowever, given the number of bugs that the Kernel Team receives during any development cycle it is impossible for us to review them all. Therefore, we occasionally resort to using automated bots to request further testing. This is such a request.\n\nWe have noted that there is a newer version of the development kernel than the one you last tested when this issue was found. Please test again with the newer kernel and indicate in the bug if this issue still exists or not.\n\nYou can update to the latest development kernel by simply running the following commands in a terminal window:\n\n    sudo apt-get update\n    sudo apt-get dist-upgrade\n\nIf the bug still exists, change the bug status from Incomplete to <previous-status>. If the bug no longer exists, change the bug status from Incomplete to Fix Released.\n\nIf you want this bot to quit automatically requesting kernel tests, add a tag named: bot-stop-nagging.\n\n Thank you for your help, we really do appreciate it.\n"
        self.comment_subject = "Test with newer development kernel (%s)" % self.cfg['released_development_kernel']

    # change_status
    #
    def change_status(self, bug, task, package_name, new_status):
        self.show_buff += colored('%s Status: "%s" -> "%s" ' % (task.bug_target_name, task.status, new_status), 'green')
        task.status = new_status  # Make the status change

    # run
    #
    def run(self, bug, task, package_name):
        retval = True

        current_status = task.status
        comment = self.comment.replace('<previous-status>', task.status)

        Dbg.verbose('Modifying this bug.\n')
        self.change_status(bug, task, package_name, "Incomplete")
        bug.add_comment(comment, self.comment_subject)
        tag = 'kernel-request-%s' % self.cfg['released_development_kernel']
        bug.tags.append(tag)

        return retval

# vi:set ts=4 sw=4 expandtab:
