<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<%
import operator

testlabels = []
versions   = []
metrics    = {}

sortedTests = sorted(template_data.iteritems(), key=operator.itemgetter(0), reverse=False)
# may need to display kernel versions and not test dates
for k in sortedTests:
    testrecord = template_data[k[0]]
    kvers = testrecord['meta']['sysinfo-uname'].split()[0]
    testlabels.append("%s" % (kvers.encode('ascii','ignore')))
    #testlabels.append("'%s'" % (k[0]))
    versions.append(kvers)
    for metricname in testrecord['metrics']:
        if metricname not in metrics:
            metrics[metricname] = []
        metrics[metricname].append(testrecord['metrics'][metricname])

sortedMetrics = sorted(metrics.keys())
chart_series  = 'series: [\n'
chart_series  += '                    {\n'

for metricname in sortedMetrics[:-1]:
    if metricname.endswith("{perf}"):
        displayname = metricname[:-len("{perf}")]
    else:
        displayname = metricname
    chart_series += '                      name: \'%s\',\n' % displayname
    chart_series += '                      data: [%s]\n' % (', '.join(metrics[metricname]))
    chart_series += '                    },{\n'

metricname = sortedMetrics[-1]
if metricname.endswith("{perf}"):
    displayname = metricname[:-len("{perf}")]
else:
    displayname = metricname
chart_series += '                      name: \'%s\',\n' % displayname
chart_series += '                      data: [%s]\n' % (', '.join(metrics[metricname]))
chart_series += '                    }\n'
chart_series += '                ]'

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
    <div id="highchart" style="width: 1000px; height: 2400px;"></div>
        <script type="text/javascript">
        $(function () {
            chart = new Highcharts.Chart({
                chart: {
                    renderTo: 'highchart',
                    defaultSeriesType: 'line'
                },
                title: {
                    text: '${report_title}'
                },
                legend: {
                    enabled: false
                },
                 legend: {
                     reversed: true
                 },
                xAxis: {
                    categories: ${testlabels}
                },
                yAxis: {
                   title: {
                       text: 'Benchmark Result'
                   },
                   min: 0
                },
                tooltip: {
                    formatter: function() {
                        return '<b>' + this.series.name + '</b> : ' + this.y;
                    }
                },
                plotOptions: {
                    line: {
                        dataLabels: {
                           enabled: true
                        },
                        enableMouseTracking: false
                    }
                },
                ${chart_series}
            });
        });

        </script>
    </body>
</html>
<!-- vi:set ts=4 sw=4 expandtab syntax=mako: -->
