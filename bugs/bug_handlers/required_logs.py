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

    # run
    #
    def run(self, bug, task, package_name):
        retval = True
        if task.status == "New":
            if bug.has_required_logs:
                if self.show_buff == '':
                    self.show_buff += 'Bug: %s  ' % bug.id
                self.vout(3, colored("           - Has the required logs.\n", 'yellow'))

                nominations = bug.nominations
                if len(nominations) > 0:
                    if task.status == "New":
                        try:
                            task.status = "Confirmed"  # Make the status change
                        except:
                            pass # Just eat this one and move onto the nominations
                    for nomination in nominations:
                        if nomination.status != 'Approved': continue
                        task_name = '%s (Ubuntu %s)' % (package_name, nomination.target.name.title())
                        t = bug.get_relevant_task(task_name)
                        if t is not None and t.status == "New":
                            self.show_buff += colored('%s Status: "%s" -> "Confirmed"; ' % (task_name, t.status), 'green')
                            t.status = "Confirmed"  # Make the status change
                else:
                    self.show_buff += colored('%s Status: "%s" -> "Confirmed" ' % (task.bug_target_name, task.status), 'green')
                    if task.status == "New":
                        task.status = "Confirmed"  # Make the status change
            else:
                self.vout(3, colored("           - Does not have the required logs.\n", 'white'))

        return retval

# vi:set ts=4 sw=4 expandtab:
