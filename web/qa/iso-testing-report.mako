<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" dir="ltr" lang="en-US">
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<title>${report_title}</title>

        <link title="light" rel="stylesheet" href="http://people.canonical.com/~kernel/reports/css/themes/blue/style.css" type="text/css" media="print, projection, screen" />
        <link title="light" rel="stylesheet" href="http://people.canonical.com/~kernel/reports/css/light.css" type="text/css" media="print, projection, screen" />

        <link title="dark" rel="stylesheet" href="http://people.canonical.com/~kernel/reports/css/themes/bjf/style.css" type="text/css" media="print, projection, screen" />
        <link title="dark" rel="stylesheet" href="http://people.canonical.com/~kernel/reports/css/dark.css" type="text/css" media="print, projection, screen" />

        <script type="text/javascript" src="http://people.canonical.com/~kernel/reports/js/styleswitcher.js"></script>

        <link href='http://fonts.googleapis.com/css?family=Cantarell&subset=latin'              rel='stylesheet' type='text/css'>
        <script type="text/javascript" src="http://people.canonical.com/~kernel/reports/js/jquery-latest.js"></script>
        <script type="text/javascript" src="http://people.canonical.com/~kernel/reports/js/jquery.tablesorter.js"></script>
        <script type="text/javascript">
           // add parser through the tablesorter addParser method
           $.tablesorter.addParser({
               // set a unique id
               id: 'age',
               is: function(s) {
                   // return false so this parser is not auto detected
                   return false;
               },
               format: function(s) {
                   // format your data for normalization
                   fields  = s.split('.')
                   days    = parseInt(fields[0], 10) * (60 * 24);
                   hours   = parseInt(fields[1], 10) * 60;
                   minutes = parseInt(fields[2]);
                   total   = minutes + hours + days
                   return total;
               },
               // set type, either numeric or text
               type: 'numeric'
           });

           // add parser through the tablesorter addParser method
           $.tablesorter.addParser({
               // set a unique id
               id: 'importance',
               is: function(s) {
                   // return false so this parser is not auto detected
                   return false;
               },
               format: function(s) {
                   // format your data for normalization
                       return s.toLowerCase().replace(/critical/,6).replace(/high/,5).replace(/medium/,4).replace(/low/,3).replace(/wishlist/,2).replace(/undecided/,1).replace(/unknown/,0);
               },
               // set type, either numeric or text
               type: 'numeric'
           });

           // add parser through the tablesorter addParser method
           $.tablesorter.addParser({
               // set a unique id
               id: 'status',
               is: function(s) {
                   // return false so this parser is not auto detected
                   return false;
               },
               format: function(s) {
                   // format your data for normalization
                       return s.toLowerCase().replace(/new/,12).replace(/incomplete/,11).replace(/confirmed/,10).replace(/triaged/,9).replace(/in progress/,8).replace(/fix committed/,7).replace(/fix released/,6).replace(/invalid/,5).replace(/won't fix/,4).replace(/confirmed/,3).replace(/opinion/,2).replace(/expired/,1).replace(/unknown/,0);
               },
               // set type, either numeric or text
               type: 'numeric'
           });
           $(function() {
                $("#linux").tablesorter({
                    headers: {
                        4: {
                            sorter:'importance'
                        },
                        5: {
                            sorter:'status'
                        }
                    },
                    widgets: ['zebra']
                });
            });
        </script>

    </head>


    <body class="bugbody">
        <div class="outermost">
            <div class="title">
		    ${report_title}
            </div>
            <div class="section">
                <table id="linux" class="tablesorter" border="0" cellpadding="0" cellspacing="1" width="100%%">
                    <thead>
                        <tr>
                            <th width="40">Bug</th>
                            <th>Summary</th>
                            <th width="100">Owner</th>
                            <th width="100">Package</th>
                            <th width="80">Importance</th>
                            <th width="80">Status</th>
                            <th width="140">Assignee</th>
                            <th width="60">Series</th>
                            <th width="60">Found</th>
                            <th width="60">Target</th>
                            <th width="30">CO</th>
                            <th width="30">SU</th>
                            <th width="30">AM</th>
                            <th width="30">DU</th>
                            <th width="100">Created</th>
                        </tr>
                    </thead>
		    <tbody>
                <%
                    importance_color = {
                            "Unknown"       : "importance-unknown",
                            "Critical"      : "importance-critical",
                            "High"          : "importance-high",
                            "Medium"        : "importance-medium",
                            "Low"           : "importance-low",
                            "Wishlist"      : "importance-wishlist",
                            "Undecided"     : "importance-undecided"
                        }
                    status_color     = {
                            "New"           : "status-new",
                            "Incomplete"    : "status-incomplete",
                            "Confirmed"     : "status-confirmed",
                            "Triaged"       : "status-triaged",
                            "In Progress"   : "status-in_progress",
                            "Fix Committed" : "status-fix_committed",
                            "Fix Released"  : "status-fix_released",
                            "Invalid"       : "status-invalid",
                            "Won't Fix"     : "status-wont_fix",
                            "Opinion"       : "status-opinion",
                            "Expired"       : "status-expired",
                            "Unknown"       : "status-unknown"
                        }

                    tasks = template_data['tasks']
                %>
                % for bid in tasks:
                    % for t in tasks[bid]:
                        <%
                            importance       = t['importance']
                            importance_class = importance_color[importance]

                            status           = t['status']
                            status_class     = status_color[status]

                            assignee         = t['assignee'] if 'assignee'    in t else 'unknown'

                            nominations = ''
                            noms = t['bug']['nominations']
                            noms.sort()
                            noms.reverse()
                            for n in noms:
                                if nominations != '':
                                    nominations += ', '
                                nominations += n.title()[0]

                            task_name = t['bug_target_name'].replace('(Ubuntu Oneiric)', '').strip()

                            when = ''
                            if t['bug']['age_days'] > 1:
                                when = '%d days' % (t['bug']['age_days'])
                            elif t['bug']['age_days'] == 1:
                                when = '1 day, %d hours' % (t['bug']['age_hours'])
                            elif t['bug']['age_hours'] > 1:
                                when = '%d hours' % (t['bug']['age_hours'])
                            elif t['bug']['age_hours'] == 1:
                                when = '1 hour, %d mintues' % (t['bug']['age_minutes'])
                        %>
                        <tr>
                            <td><a href="http://launchpad.net/bugs/${bid}">${bid}</a></td>
                            <td>${t['bug']['title']}</td>
                            <td>${t['team']}</td>
                            <td>${task_name}</td>
                            <td class="${importance_class}">${importance}</td>
                            <td class="${status_class}">${status}</td>
                            <td>${assignee}</td>
                            <td>${t['bug']['series_name']}</td>
                            <td>${t['milestone_found']}</td>
                            <td>${t['milestone_target']}</td>
                            <td>${t['bug']['number_of_messages']}</td>
                            <td>${t['bug']['number_subscribed']}</td>
                            <td>${t['bug']['number_affected']}</td>
                            <td>${t['bug']['number_of_duplicates']}</td>
                            <td>${t['bug']['iso_date_created']}</td>
                        </tr>
                    % endfor
                % endfor
		    </tbody>
                </table>
            </div>
            <br />
            <br />
            <div>
                <br />
                <hr />
                <table width="100%%" cellspacing="0" cellpadding="0">
                    <thead>
                        <tr>
                            <td width="100">Column</td>
                            <td>Description</td>
                        </tr>
                    </th>
                    <tbody>
                        <tr><td>Bug       </td><td>The Launcpad Bug number and a link the the Launchpad Bug.            </td></tr>
                        <tr><td>Summary   </td><td>The 'summary' or 'title' from the bug.                               </td></tr>
                        <tr><td>Owner     </td><td>The team that is responsible for bugs affecting the specific package.</td></tr>
                        <tr><td>Package   </td><td>The package a bug task was created for relating to the specific bug. </td></tr>
                        <tr><td>Importance</td><td>The bug task's importance.                                           </td></tr>
                        <tr><td>Status    </td><td>The bug task's status.                                               </td></tr>
                        <tr><td>Assignee  </td><td>The person or team assigned to work on the bug.                      </td></tr>
                        <tr><td>Series    </td><td>The Ubuntu series name that the bug was filed against.               </td></tr>
                        <tr><td>Found     </td><td>The milestone the bug task found during.                             </td></tr>
                        <tr><td>Target    </td><td>The milestone the bug task is targeted to be fixed.                  </td></tr>
                        <tr><td>CO        </td><td>The number of comments that have been added to the bug.              </td></tr>
                        <tr><td>SU        </td><td>The number of subscribers to the bug.                                </td></tr>
                        <tr><td>AM        </td><td>The number of "affects me" for the bug.                              </td></tr>
                        <tr><td>DU        </td><td>The number of duplicates of the bug.                                 </td></tr>
                        <tr><td>Created   </td><td>The date the bug was created.                                        </td></tr>
                    </tbody>
                </table>
                <br />
            </div>
            <div>
                <br />
                <hr />
                <table width="100%%" cellspacing="0" cellpadding="0">
                    <tr>
                        <td>
                            ${timestamp}
                        </td>
                        <td align="right">
                            Themes:&nbsp;&nbsp;
                            <a href='#' onclick="setActiveStyleSheet('dark'); return false;">DARK</a>
                            &nbsp;
                            <a href='#' onclick="setActiveStyleSheet('light'); return false;">LIGHT</a>
                        </td>
                    </tr>
                </table>
                <br />
            </div>


        </div> <!-- Outermost -->
    </body>


</html>
<!-- vi:set ts=4 sw=4 expandtab: -->
