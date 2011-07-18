#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler
from ktl.termcolor                      import colored

class RequiredLogs(BugHandler):
    """
    The Kernel Team requires every bug to have certain logs attached before it can be worked on. The
    KernelBug class knows which logs are required. The rules for this method are:

    Bug status == "New":

       1. If the correct logs are attached the status is changed from "New" to "Confirmed".

       2. If it does not have the correct logs attached:
          a) It's status is changed from "New" to "Incomplete"
          b) A comment is added to the bug asking for the logs to be attached and how.
          c) The tag: "needs-logs" is added to the bug.

    Bug status == "Incomplete" & tagged with "needs-logs":

      1. If the correct logs are attached, the status is changed from "Incomplete" to
         "Confirmed".

      2. If it does not have the correct logs attached, no changes are made to it.

    Note: Only section 1, #1 has been implemented.
    """

    # __init__
    #
    def __init__(self, lp, vout):
        BugHandler.__init__(self, lp, vout)

        self.comment = "This bug is missing log files that will aid in dianosing the problem. From a terminal window please run:\n\napport-collect %s\n\nand then change the status of the bug back to 'New'.\n\nIf, due to the nature of the issue you have encountered, you are unable to run this command, please add a comment stating that fact and change the bug status to 'Confirmed'.\n\nThis change has been made by an automated script, maintained by the Ubuntu Kernel Team."
        self.comment_subject = "Missing required logs."

    # change_status
    #
    def change_status(self, bug, task, package_name, new_status):
        nominations = bug.nominations
        if len(nominations) > 0:
            if task.status == "New":
                try:
                    task.status = new_status  # Make the status change
                except:
                    pass # Just eat this one and move onto the nominations
            for nomination in nominations:
                if nomination.status != 'Approved': continue
                task_name = '%s (Ubuntu %s)' % (package_name, nomination.target.name.title())
                t = bug.get_relevant_task(task_name)
                if t is not None and t.status == "New":
                    self.show_buff += colored('%s Status: "%s" -> "%s"; ' % (task_name, t.status, new_status), 'green')
                    t.status = new_status  # Make the status change
        else:
            self.show_buff += colored('%s Status: "%s" -> "%s" ' % (task.bug_target_name, task.status, new_status), 'green')
            if task.status == "New":
                task.status = new_status  # Make the status change

    # run
    #
    def run(self, bug, task, package_name):
        retval = True
        if task.status == "New":
            if bug.has_required_logs:
                if self.show_buff == '':
                    self.show_buff += 'Bug: %s  ' % bug.id
                self.change_status(bug, task, package_name, "Confirmed")
            else:
                bug.add_comment(self.comment % (bug.id), self.comment_subject)
                self.change_status(bug, task, package_name, "Incomplete")

        return retval

# vi:set ts=4 sw=4 expandtab:
