#!/usr/bin/env python
#

#from lpltk.service                      import LaunchpadService, LaunchpadServiceError
from ktl.workflow                       import Workflow
from ktl.ubuntu                         import Ubuntu

class TrackingBug:

    def __init__(self, lp, staging):
        self.lp = lp
        self.staging = staging

    def open(self, package, version, release, new_abi):
        wf = Workflow()
        ub = Ubuntu()

        # Title: <release>: <version> -proposed tracker
        title = "%s: %s -proposed tracker" % (package, version)

        # Description:
        #    This bug is for tracking the <version> upload package. This bug will
        #    contain status and testing results related to that upload.
        #
        description  = "This bug is for tracking the %s upload package. " % (version)
        description += "This bug will contain status and testing results releated to that upload."
        description += "\n\n"
        description += "For an explanation of the tasks and the associated workflow see: "
        description += "https://wiki.ubuntu.com/Kernel/kernel-sru-workflow"

        bug = self.lp.create_bug(project='ubuntu', package=package, title=title, description=description)

        id = bug.id
        if self.staging:
            print("https://bugs.qastaging.launchpad.net/bugs/%s" % (id))
        else:
            print("https://bugs.launchpad.net/bugs/%s" % (id))

        # Tags:
        #    add all tags for this package name
        taglist = wf.initial_tags(package)
        for itag in taglist:
            bug.tags.append(itag)

        # Get the one task and modify the status and importance.
        #
        for task in bug.tasks:
            task.status = "In Progress"
            task.importance = "Medium"
            break

        # Teams / individuals to be automatically subscribed to the tracking bugs
        #   These vary per package
        #
        teams = wf.subscribers(package)
        for team in teams:
            lp_team = self.lp.launchpad.people[team]
            bug.lpbug.subscribe(person=lp_team)

        # For the given version, figure out the series and nominate that series.
        #
        lp = self.lp.launchpad
        ubuntu = lp.distributions["ubuntu"]
        sc = ubuntu.series_collection
        for s in sc:
            if s.name == release:
                nomination = bug.lpbug.addNomination(target=s)
                if nomination.canApprove():
                    nomination.approve()
                break

        # search for dependent packages
        found = {}
        for entry in iter(ub.db):
            if ub.db[entry]['name'] == s.name.lower():
                found = ub.db[entry]
        if found:
            dep_list = []
            if 'dependent-packages' in found:
                for dep in iter(found['dependent-packages']):
                    if dep == package:
                        dep_list = found['dependent-packages'][dep]
                        break
            if dep_list and new_abi:
                for pkg in dep_list:
                    src_pkg = ubuntu.getSourcePackage(name=pkg)
                    bug.lpbug.addTask(target=src_pkg)

        # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        # Add a task for kernel-sru-workflow and then nominate all the series that belong
        # to that project.
        #

        ubuntu = lp.distributions["ubuntu"]
        project = 'kernel-sru-workflow'
        proj = lp.projects[project]
        bug.lpbug.addTask(target=proj)

        sc = proj.series_collection
        for s in sc:
            if s.active and s.name not in ['trunk', 'latest']:
                nomination = bug.lpbug.addNomination(target=s)
                if nomination.canApprove():
                    nomination.approve()

        # Task assignments
        # Set status of the master task so the bot will start processing
        #
        for t in bug.tasks:
            task       = t.bug_target_name
            parts = task.partition(proj.display_name)
            if parts[0] == '' and parts[1] == proj.display_name and parts[2] == '':
                t.status = "In Progress"
            else:
                if parts[0] != '':
                    continue
                task = parts[2].strip()
                assignee = wf.assignee(package, task)
                if assignee is None:
                    print 'Note: Found a workflow task named %s with no automatic assignee, leaving unassigned and setting to invalid' % task
                    t.status = "Invalid"
                else:
                    try:
                        t.assignee = self.lp.launchpad.people[assignee]
                    except KeyError:
                        print("Can't find '%s' team in Launchpad!" % (assignee))

        return bug

# vi:set ts=4 sw=4 expandtab:
