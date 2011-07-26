#!/usr/bin/env python
######################################################################
# automatically generate a html page which lists open kernel cve bug tasks
#
# http://qa.ubuntu.com/reports/ogasawara/kernel-cves.html
#
######################################################################
import os
import sys
import commands
import re
from launchpadlib.launchpad import Launchpad

class Status(object):
    def __init__(self):
        self.triaged = []
        self.confirmed = []
        self.new = []
        self.inprogress = []
        self.fixcommitted = []
        self.fixreleased = []
        self.incomplete = []
        self.invalid = []
        self.wontfix = []

class Importance(object):
    def __init__(self):
        self.critical= Status()
        self.high = Status()
        self.medium = Status()
        self.low = Status()
        self.wishlist = Status()
        self.undecided = Status()
        self.unknown = Status()

def StoreImportance(html, importance, status, list):
    temp = importance.lower()
    value = getattr(list, temp)
    StoreStatus(html, status, value)
    return

def StoreStatus(html, status, list):
    stat = status.replace(" ", "").lower()
    stat = stat.replace("'", "")
    value = getattr(list, stat)
    value.append(html)
    return

def BuildBuglist(list, rows, id):
    html = ""
    url = "https://bugs.launchpad.net/ubuntu/+bug/%d" % id
    first = True
    importance_list = ["critical", "high", "medium", "low", "wishlist", "undecided", "unknown"]
    status_list = ["triaged", "confirmed", "new", "inprogress", "fixcommitted", "fixreleased", "incomplete", "invalid", "wontfix"]
    for importance in importance_list:
        value = getattr(list, importance)
        for status in status_list:
            lines = getattr(value, status)
            for line in lines:
                if first:
                    html += "<tr>\n"
                    html += "\t<td class=\"first\" rowspan=%d><a href=\"%s\">%s</a></td>\n" % (rows, url, bug.id)
                else:
                    html += "<tr>\n"
                first = False
                html += line
    return html

def GetColor(data):
    red = "#ffa2a2"
    green = "#c4ffc4"
    yellow = "#fffec4"
    grey = "#e0e0e0"

    if data == "Critical" or data == "High":
        color = red
    elif data == "Medium":
        color = yellow
    elif data == "Fix Released" or data == "Fix Committed":
        color = green
    elif data == "In Progress":
        color = yellow
    elif data == "Invalid" or data == "Won\'t Fix":
        color = grey
    else:
        color=""

    return color

def ParseNomination(task):
    nomination = ""
    nom = re.search("\(.* (.*)\)", task.bug_target_name)
    if nom:
        nomination = nom.group(1)
    return nomination

def LoopBugTasks(bug, cvelist):
    bug_tasks = {}
    
    for task in bug.bug_tasks:
        if task.status == "Won't Fix" or task.status == "Invalid" or task.status == "Fix Released":
            continue

        bug_tasks[task.bug_target_name] = {"nomination": "",
                                            "importance": "",
                                            "status": "",
                                            "assignee": ""}
         
        bug_tasks[task.bug_target_name]["nomination"] = ParseNomination(task)
        bug_tasks[task.bug_target_name]["importance"] = task.importance
        bug_tasks[task.bug_target_name]["status"] = task.status 
        if task.assignee_link:
            bug_tasks[task.bug_target_name]["assignee"] = task.assignee.name

    return bug_tasks

def StoreHtml(bug, open_tasks, cvelist):
    for pkg in open_tasks.keys():
        html_row = ""
        html_row += "\t<td>%s</td>\n" % (pkg)
        html_row += "\t<td>%s</td>\n" % (open_tasks[pkg]["nomination"])
        color = GetColor(open_tasks[pkg]["importance"])
        html_row += "\t<td bgcolor =\"%s\">%s</td>\n" % (color, open_tasks[pkg]["importance"])
        color = GetColor(open_tasks[pkg]["status"])
        html_row += "\t<td bgcolor=\"%s\">%s</td>\n" % (color, open_tasks[pkg]["status"])
        html_row += "\t<td>%s</td>\n" % (bug.title)
        html_row += "\t<td>%s</td>\n" % (open_tasks[pkg]["assignee"])
        html_row += "</tr>\n"
        StoreImportance(html_row, open_tasks[pkg]["importance"], open_tasks[pkg]["status"], cvelist)

    return

cachedir = os.path.expanduser("~/.cache/")
lp = Launchpad.login_anonymously('kernel team tools', 'production', cachedir)
ubuntu = lp.distributions['ubuntu']
cves = ubuntu.searchTasks(tags=['kernel-cve-tracking-bug', 'kernel-cve-tracker'])
htmlfile = "./kernel-cves.html"
date = commands.getoutput('date -u +"%b %e %G %k:%M %Z"')
uniq_list = []
bugs = {}

for cve in cves:
    num = cve.bug.id
    cvelist = Importance()
    if num in uniq_list:
#        print "Bug %s already exists in list.  Skipping." % (num)
        continue
    else:
        uniq_list.append(num)

    bug = lp.bugs[num]
    open_tasks = LoopBugTasks(bug, cvelist)
    StoreHtml(bug, open_tasks, cvelist) 

    rows = len(open_tasks.keys())
    html_row = BuildBuglist(cvelist, rows, num)
    bugs[num] = html_row    

html = '''
<table class="listing" id="trackers" border=1>
<thead><tr>
<th align=center>Bug</defanghtml_hd>
<th align=center>Package</th>
<th align=center>Nomination</th>
<th align=center>Status</th>
<th align=center>Importance</th>
<th align=center>Summary</th>
<th align=center>Assignee</th>
</tr></thead><tbody>
'''
for bug_id in sorted(bugs):
    html += bugs[bug_id]
html += "</tbody></table></body></html>\n"

templatefile = open ('../misc/mult-tables-template.html', 'r')
datafile = open(htmlfile, 'w')
for line in templatefile:
    if line.strip() == "<!-- *** Title Space *** -->":
        datafile.write("Ubuntu Kernel CVE List\n")
    elif line.strip() == "<!-- *** Header Space *** -->":
        datafile.write("<h1>Ubuntu Kernel CVE List</a> - %s</h1>\n" % (date))
    elif line.strip() == "<!-- *** Table Space *** -->":
        datafile.write(html)
    else:
        datafile.write(line)
templatefile.close()
datafile.close()
