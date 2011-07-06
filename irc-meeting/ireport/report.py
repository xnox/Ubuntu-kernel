#!/usr/bin/python
#

from datetime           import *

# Report
#
class Report:
    # __init__
    #
    def __init__(self):
        self.now = datetime.now()
        self.first_action = True
        self.topic_type = 'none'
        return

    # w
    #
    def w(self, str):
        print(str)
        return

    # y
    #
    def y(self, str):
        print(str)
        return

# HtmlReport
#
class HtmlReport(Report):
    # __init__
    #
    def __init__(self):
        Report.__init__(self)
        return

    # start
    #
    def start(self):
        self.w("<h3 style=\"font-weight: bold;font-size: 1.75em; font-weight: bold;font-size:1.75em;border-bottom: 2px solid silver;\">Meeting Minutes</h3>")
        self.w("<a href=\"http://irclogs.ubuntu.com/%d/%02d/%02d/%%23ubuntu-meeting.txt\">IRC Log of the meeting.</a>" % (self.now.year, self.now.month, self.now.day))
        self.w("<a href=\"http://voices.canonical.com/kernelteam\">Blog of the meeting minutes.</a>")
        self.w("")
        self.w("<br>")
        self.w("")
        self.w("<h3 style=\"font-weight: bold;font-size:1.75em;border-bottom: 2px solid silver;\">Agenda</h3>")
        self.w("<a href=\"https://wiki.ubuntu.com/KernelTeam/Meeting#Meeting:%%20Tues,%%20%02d%%20%s%%20%02d\">%d-%02d-%02d Meeting Agenda</a>" % (self.now.day, self.now.strftime('%b'), self.now.year, self.now.year, self.now.month, self.now.day))
        return

    # finish
    #
    def finish(self):
        return

    # topic
    #
    def topic(self, msg):
        if self.topic_type == 'action-item':
            self.topic_open_action_item_finish()

        self.w("")
        self.w("<!-- -------------------------------------------------------------------------")
        self.w("     %s" % (msg))
        self.w("-->")
        self.w("<br>")
        self.w("<h3 style=\"font-weight: bold;font-size:1.75em;border-bottom: 2px solid silver;\">%s</h3>" % (msg))

        self.topic_type = 'general'
        return

    # topic_open_action_item
    #
    def topic_open_action_item(self, nic, msg):
        if self.first_action:
            self.w("")
            self.w("<!-- -------------------------------------------------------------------------")
            self.w("     Outstanding Actions")
            self.w("-->")
            self.w("<br>")
            self.w("<h3 style=\"font-weight: bold;font-size:1.75em;border-bottom: 2px solid silver;\">Outstanding actions from the last meeting</h3>")
            self.w("<table width=\"100%\">")
            self.first_action = False

        self.w("")
        self.w("    <tr> <td width=\"10%%\"><strong>Item:  </strong></td> <td>%s</td>" % (msg))
        self.w("")

        self.topic_type = 'action-item'
        return

    # topic_open_action_item_finish
    #
    def topic_open_action_item_finish(self):
        self.w("</table>")
        return

    def releaseMetricsFeatures(self):
        self.w('')
        self.w('<h3 style="font-weight: bold;font-size:1.5em;border-bottom: 2px solid silver;">Milestoned Features</h3>')
        self.topic_type = 'features'
        return

    def releaseMetricsBugsWithPatches(self, msg):
        self.w('')
        self.w('<h3 style="font-weight: bold;font-size:1.5em;">%s</h3>' % msg)
        self.topic_type = 'bugs-with-patches'
        return

    def releaseMetricsBugs(self, metrics):
        out = self.w
        out("")
        out("<!-- -------------------------------------------------------------------------")
        out("     Release Metrics")
        out("-->")
        out('<h3 style="font-weight: bold;font-size:1.75em;border-bottom: 2px solid silver;">Release Metrics</h3>')
        out('<h3 style="font-weight: bold;font-size:1.5em;">Bugs</h3>')

        out('<table width="100%" cellspacing="0">')
        out('    <tr>')
        out('        <td width="30">&nbsp;</td>')
        out('        </td>')
        out('        <td valign="top">')

        out('            <table width="100%" cellspacing="0">')
        str = '                <tr><td width="100" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;">&nbsp;</td>'
        str += '<td width="350" align="center" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;">%-60s</td>' % "<strong>%s</strong>" % metrics['milestoned']['header']

        str += '<td width="350" align="center" style="border-spacing: 0; border-width: 0 0px 2px 0; border-style: solid; border-color: blue;">%-60s</td>' % "<strong>%s</strong>" % metrics['targeted']['header']
        str += '<td></td></tr>'
        out(str)

        str = '                <tr><td width="100" style="border-spacing: 0; border-width: 0 1px 0px 0; border-style: solid; border-color: blue;">linux</td>'
        str += '<td width="350" align="center" style="border-spacing: 0; border-width: 0 1px 0px 0; border-style: solid; border-color: blue;"> %-60s </td>' % (metrics['milestoned']['value'])
        str += '<td width="350" align="center" style="border-spacing: 0; border-width: 0 0px 0px 0; border-style: solid; border-color: blue;"> %-60s </td>' % (metrics['targeted']['value'])
        str += '<td></td></tr>'
        out(str)
        out("            </table>")

        out('        </td>')
        out('    </tr>')
        out("</table>")
        return

    def incomingBugsFinish(self, incoming, regressions):
        out = self.w
        out("")
        out("<!-- -------------------------------------------------------------------------")
        out("     Incoming Bugs and Regressions Stats")
        out("-->")
        out('<h3 style="font-weight: bold;font-size:1.75em;border-bottom: 2px solid silver;">Incoming Bugs and Regression Stats</h3>')
        out('<h3 style="font-weight: bold;font-size:1.5em;">Incoming Bugs:</h3>')
        out('<table width="100%" cellspacing="0">')
        out('    <tr><td width="30">&nbsp;</td><td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;"><b>Version</b></td> <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 2px 0; border-style: solid; border-color: blue;"><b>Count</b></td><td></td></tr>')

        if 'Maverick' in incoming and 'Lucid' in incoming:
            out('    <tr><td width="30">&nbsp;</td><td width="150" align="left"   style="border-spacing: 0; border-width: 0 1px 1px 0; border-style: solid; border-color: blue;">%-22s</td> <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 1px 0; border-style: solid; border-color: blue;">%-22s</td><td></td></tr>' % ('Maverick', incoming['Maverick']))
            out('    <tr><td width="30">&nbsp;</td><td width="150" align="left"   style="border-spacing: 0; border-width: 0 1px 0px 0; border-style: solid; border-color: blue;">%-22s</td> <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 0px 0; border-style: solid; border-color: blue;">%-22s</td><td></td></tr>' % ('Lucid', incoming['Lucid']))

        out("</table>")


        # The reason this table is more complicated that it should be is i'm playing games
        # with the cell borders.
        #
        out("")
        out('<h3 style="font-weight: bold;font-size:1.5em;">Current regression stats:</h3>')
        out('<table width="100%" cellspacing="0">')
        out('    <tr><td width="30">&nbsp;</td><td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;"><b>Version</b></td> <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;"><b>Potential</b></td> <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;"><b>Update</b></td> <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 2px 0; border-style: solid; border-color: blue;"><b>Release</b></td><td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 2px 0; border-style: solid; border-color: blue;"><b>Proposed</b></td></tr>')
        if 'maveric' in regressions:
            for v in ['maverick', 'lucid', 'karmic', 'jaunty']:
                str = '    <tr><td width="30">&nbsp;</td><td width="150" align="left" style="border-spacing: 0; border-width: 0 1px 1px 0; border-style: solid; border-color: blue;"> %-21s </td>' % v
                for state in ['potential', 'update', 'release', 'proposed']:
                    if state == 'proposed':
                        if state in regressions[v]:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 1px 0; border-style: solid; border-color: blue;"> %-21s </td>' % regressions[v][state]
                        else:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 1px 0; border-style: solid; border-color: blue;"> &nbsp; </td>'
                    else:
                        if state in regressions[v]:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 1px 0; border-style: solid; border-color: blue;"> %-21s </td>' % regressions[v][state]
                        else:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 1px 0; border-style: solid; border-color: blue;"> &nbsp; </td>'
                str += "<td></td></tr>"
                out(str)

        if 'hardy' in regressions:
            for v in ['hardy']:
                str = '    <tr><td width="30">&nbsp;</td><td width="150" align="left" style="border-spacing: 0; border-width: 0 1px 0px 0; border-style: solid; border-color: blue;"> %-21s </td>' % v
                for state in ['potential', 'update', 'release', 'proposed']:
                    if state == 'proposed':
                        if state in regressions[v]:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 0px 0; border-style: solid; border-color: blue;"> %-21s </td>' % regressions[v][state]
                        else:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 0px 0px 0; border-style: solid; border-color: blue;"> &nbsp; </td>'
                    else:
                        if state in regressions[v]:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 0px 0; border-style: solid; border-color: blue;"> %-21s </td>' % regressions[v][state]
                        else:
                            str += '    <td width="150" align="center" style="border-spacing: 0; border-width: 0 1px 0px 0; border-style: solid; border-color: blue;"> &nbsp; </td>'
                str += "<td></td></tr>"
                out(str)
        out("</table>")

        return

    # out
    #
    def out(self, nic, msg):
        if msg != '..':
            if self.topic_type == 'none':
                self.w("html: (%s) '%s'" % (nic, msg))
            elif self.topic_type == 'action-item':
                self.w("    <tr> <td width=\"10%%\"><strong>Status:</strong></td> <td>%s</td>" % (msg))
            elif self.topic_type == 'general':
                self.w("%s" % (msg))
            elif self.topic_type == 'features':
                self.w("%s" % (msg))
            elif self.topic_type == 'bugs-with-patches':
                self.w("%s" % (msg))
        return

# MoinReport
#
class MoinReport(Report):
    # __init__
    #
    def __init__(self):
        Report.__init__(self)
        return

    # start
    #
    def start(self):
        self.w("= Meeting Minutes =")
        self.w("[[http://irclogs.ubuntu.com/%d/%02d/%02d/%%23ubuntu-meeting.txt|IRC Log of the meeting.]]" % (self.now.year, self.now.month, self.now.day))
        self.w("<<BR>>")
        self.w("[[http://voices.canonical.com/kernelteam|Meeting minutes.]]")
        self.w("")
        self.w("== Agenda ==")
        self.w("[[https://wiki.ubuntu.com/KernelTeam/Meeting#Tues, %02d %s, %d|%d%02d%02d Meeting Agenda]]" % (self.now.day, self.now.strftime('%b'), self.now.year, self.now.year, self.now.month, self.now.day))
        self.w("")
        return

    # finish
    #
    def finish(self):
        return

    # topic
    #
    def topic(self, msg):
        if self.topic_type == 'action-item':
            self.topic_open_action_item_finish()

        self.w("")
        self.w("=== %s ===" % (msg))
        self.topic_type = 'general'
        return

    # topic_open_action_item
    #
    def topic_open_action_item(self, nic, msg):
        #self.w("moin: (topic_opn_action_item) %s" % msg)
        if self.first_action:
            self.w("=== Outstanding actions from the last meeting ===")
            self.first_action = False

        self.w("")
        self.w("  * %s" % (msg))

        self.topic_type = 'action-item'
        return

    # topic_open_action_item_finish
    #
    def topic_open_action_item_finish(self):
        self.w("")
        return

    def releaseMetricsFeatures(self):
        self.w('')
        self.w('==== Milestoned Features ====')
        self.topic_type = 'features'
        return

    def releaseMetricsBugsWithPatches(self, msg):
        self.w('')
        self.w('==== %s ====' % msg)
        self.topic_type = 'bugs-with-patches'
        return

    def releaseMetricsBugs(self, metrics):
        self.w('=== Release Metrics ===')
        self.w('==== Bugs ====')
        self.w('||<-3 tablestyle="width: 100%;")>                                                                                                                             ||')
        str = '||<:>                   '
        for state in ['milestoned', 'targeted']:
            tmp = "'''%s'''" % metrics[state]['header']
            str += "||<:> %-60s " % tmp
        str += '||'
        self.w(str)

        str = '||linux                 '
        for state in ['milestoned', 'targeted']:
            str += "||<:> %-60s " % (metrics[state]['value'])
        str += '||'
        self.w(str)
        return

    def incomingBugsFinish(self, incoming, regressions):
        self.w("")
        if 'Maverick' in incoming and 'Lucid' in incoming:
            self.w("=== Incoming Bugs and Regressions ===")
            self.w("==== Incoming Bugs: ====")
            self.w('||<-2 tablestyle="width:  35%;")>                   ||')
            self.w("||<:> '''Version'''      ||<:> '''Count'''          ||")
            for k in ['Maverick', 'Lucid']:
                self.w("||%-22s ||<:>%-22s||" % (k, incoming[k]))

            self.w("")
            self.w("==== Current regression stats: ====")
            self.w('||<-5 tablestyle="width: 100%;")>                                                                                                   ||')
            self.w("||<:> '''Version'''     ||<:> '''Potential'''      ||<:> '''Update'''         ||<:> '''Release'''        ||<:> '''Proposed'''       ||")
            for v in ['maverick', 'lucid', 'karmic', 'jaunty', 'hardy']:
                str = "|| %-21s" % v
                for state in ['potential', 'update', 'release', 'proposed']:
                    if state in regressions[v]:
                        str += "||<:> %-21s" % regressions[v][state]
                    else:
                        str += "||<:>                      "
                str += "||"
                self.w(str)

        return

    # out
    #
    def out(self, nic, msg):
        if msg != '..':
            if self.topic_type == 'none':
                self.w("moin: (%s) '%s'" % (nic, msg))
            elif self.topic_type == 'action-item':
                self.w("    %s" % (msg))
            elif self.topic_type == 'general':
                self.w("(%s) %s" % (nic, msg))
            elif self.topic_type == 'features':
                self.w("%s" % (msg))
            elif self.topic_type == 'bugs-with-patches':
                self.w("%s" % (msg))

        return



# vi:set ts=4 sw=4 expandtab:
