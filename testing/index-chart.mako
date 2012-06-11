<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<%
import operator

# TODO testlabels will be wrong
chart_series_map = {}
testlabels_map = {}

for template_data in template_data_list:
    chart_series_list = []
    metrics    = {}
    testlabels = []
    minY       = None
    maxY       = None

    sortedTests = sorted(template_data.iteritems(), key=operator.itemgetter(0), reverse=False)

    for k in sortedTests:
	testrecord = template_data[k[0]]
	kvers = testrecord['meta']['sysinfo-uname'].split()[0]

	# Chart_title needs to be one per chart
	ctitle = testrecord['meta']['chart-title']

        # display the kernel version 
	testlabels.append("%s" % (kvers.encode('ascii','ignore')))

	for metricname in testrecord['metrics']:
	    if metricname not in metrics:
		metrics[metricname] = []
	    metrics[metricname].append(testrecord['metrics'][metricname])

    testlabels_map[ctitle] = testlabels

    sortedMetrics = sorted(metrics.keys())
    chart_series  = 'series: [\n'
    chart_series  += '                    {\n'

    for metricname in sortedMetrics[:-1]:
	for mv in metrics[metricname]:
	    value = float(mv)
	    if minY is None:
		minY = value
	    elif value < minY:
		minY = value
	    if maxY is None:
		maxY = value
	    elif value > maxY:
		maxY = value
	if metricname.endswith("{perf}"):
	    displayname = metricname[:-len("{perf}")]
	else:
	    displayname = metricname
	chart_series += '                      name: \'%s\',\n' % displayname
	chart_series += '                      data: [%s]\n' % (', '.join(metrics[metricname]))
	chart_series += '                    },{\n'

    metricname = sortedMetrics[-1]
    for mv in metrics[metricname]:
	value = float(mv)
	if minY is None:
	    minY = value
	elif value < minY:
	    minY = value
	if maxY is None:
	    maxY = value
	elif value > maxY:
	    maxY = value
    if metricname.endswith("{perf}"):
	displayname = metricname[:-len("{perf}")]
    else:
	displayname = metricname
    chart_series += '                      name: \'%s\',\n' % displayname
    chart_series += '                      data: [%s]\n' % (', '.join(metrics[metricname]))
    chart_series += '                    }\n'
    chart_series += '                ]'
    chart_series_map[ctitle] = chart_series
%>
<html>
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
        <script type="text/javascript" src="http://people.canonical.com/~kernel/reports/js/jquery-latest.js"></script>
        <script type="text/javascript" src="http://people.canonical.com/~bradf/media/high/js/highcharts.js"></script>
        <title>${report_title}</title>
        <style>
            .title {
                  font-weight: bold;
                    font-size: 20px;
                        color: #0099ff;
                   text-align: center;
                      padding: 2px 0px 20px 0px;
            }
        </style>
    </head>
    <body>
    <center><h1>${report_title}</h1></center>
      % for chartname in chart_series_map:
        <div id="highchart-${chartname}" style="width: 1000px; height: 1000px;"></div><hr />
	    <script type="text/javascript">
	    $(function () {
		chart = new Highcharts.Chart({
		    chart: {
			renderTo: 'highchart-${chartname}',
			defaultSeriesType: 'line'
		    },
		    title: {
			text: '${chartname}'
		    },
		    legend: {
			enabled: false
		    },
		     legend: {
			 reversed: true
		     },
		    xAxis: {
			categories: ${testlabels_map[chartname]}
		    },
		    yAxis: {
		       title: {
			   text: 'Benchmark Result'
		       }
		    },
		    tooltip: {
			formatter: function() {
			    return this.x + '<br>' + '<b>' + this.series.name + '</b> : ' + this.y;
			}
		    },
		    ${chart_series_map[chartname]}
		});
	    });
	    </script>
       % endfor
    </body>
</html>
<!-- vi:set ts=4 sw=4 expandtab syntax=mako: -->
