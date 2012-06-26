<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" dir="ltr" lang="en-US">
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
        <!-- <meta http-equiv="refresh" content="60" /> -->
        <title>Ubuntu - Kernel Team Server</title>
        <link rel="stylesheet" href="http://kernel.ubuntu.com/beta/media/kernel-style.css" type="text/css" media="screen" />
        <style>
            div.index-bottom-section {
                 border-radius: 0px;
                    box-shadow: 0 1px 3px rgba(0,0,0,0.3);
                    background: #f7f5f6;
                       padding: 20px;
                 margin-top: -16px;
                       /*
                 margin-bottom: 20px;
                 */

                     font-size: 11px;
                   line-height: 16px;
                         color: #333;
            }

            a {
                         color: #0000cc;
            }

            a:hover {
               text-decoration: underline;
            }

            a:active a:link, a:visited {
               text-decoration: none;
            }

        </style>
    </head>


    <body class="dash-body">
        <div class="dash-center-wrap">
            <div class="dash-center">
                <div class="dash-center-content">

                    <div id="dash-header">
                        <div id="dash-timestamp">
                            <a href="http://ubuntu.com" title="Home" rel="home"><img src="http://kernel.ubuntu.com/beta/media/ubuntu-logo.png" alt="Home" /></a>
                        </div>
                        <h1>Kernel Testing &amp; Benchmarks</h1>
                    </div> <!-- header -->

                    <br />

                    <div class="dash-section">
                        <table width="100%"> <!-- The section is one big table -->
                            <tr>
                                <td width="100%" valign="top">
                                    <table width="100%" style="font-size: 0.9em"> <!-- SRU Data -->
                                        % for ubuntu_series in sorted(data, reverse=True):
                                        <tr>
                                            <td style="background: #e9e7e5;">${ubuntu_series}</td>
                                        </tr>
                                        <tr>
                                            <td width="100%">
                                                <table width="100%" border="0">
                                                    <tbody>
                                                        % for kernel_version in data[ubuntu_series]:
                                                        <tr>
                                                            <td align="right">${ kernel_version } </td> <td></td>
                                                        </tr>
                                                        <tr>
                                                            <th align="right">&nbsp;</th>
                                                            <th align="left">&nbsp;</th>
                                                            <th align="left">&nbsp;</th>
                                                            <th align="center">&nbsp;</th>
                                                            <th align="center" colspan="3">Tests</th>
                                                            <th align="center">&nbsp;</th>
                                                        </tr>
                                                        <tr>
                                                            <th align="right" width="150">&nbsp;</th>
                                                            <th align="left" width="100">&nbsp; Host &nbsp;</th>
                                                            <th align="left" width="50">&nbsp; Arch &nbsp;</th>
                                                            <th align="center" width="130">Date</th>

                                                            <th align="center" width="30">Ran</th>
                                                            <th align="center" width="30">Passed</th>
                                                            <th align="center" width="30">Failed</th>

                                                            <th align="center">Benchmarks</th>
                                                        </tr>
                                                            % for record in data[ubuntu_series][kernel_version]:
                                                            <%
                                                                total = 0
                                                                passed = 0
                                                                failed = 0
                                                                for suite in record['results']['suites']:
                                                                    total += suite['tests run']
                                                                    passed += suite['tests run'] - suite['tests failed']
                                                                    failed += suite['tests failed']

                                                                link = "http://kernel.ubuntu.com/beta/testing/test-results/%s.%s/results-index.html" % (record['attributes']['environ']['NODE_NAME'], record['attributes']['environ']['BUILD_ID'])
                                                            %>
                                                            <tr>
                                                                <td align="right">&nbsp;</td> <td>${ record['attributes']['environ']['NODE_NAME'] }</td><td>${ record['attributes']['platform']['arch']['bits'] } </td> <td align="center">${ record['attributes']['timestamp'] }</td> <td align="center"><a href="${ link }">${ total }</a></td> <td align="center"><a href="${ link }">${ passed }</a></td> <td align="center"><a href="${ link }">${ failed }</a></td><td></td>
                                                            </tr>
                                                            % endfor
                                                        % endfor
                                                    </tbody>
                                                </table>
                                            </td>
                                        </tr>
                                        % endfor
                                    </table>
                                </td>
                            </tr>
                        </table>
                    </div> <!-- dash-section -->

                    <div class="index-bottom-section">
                        <table width="100%"> <!-- The section is one big table -->
                            <tr>
                                <td align="left" valign="bottom" colspan="5">
                                  <span style="font-size: 10px; color: #aea79f !important">(c) 2012 Canonical Ltd. Ubuntu and Canonical are registered trademarks of Canonical Ltd.</span>
                                </td>
                                <td align="right" valign="top">
                                    <a href="http://ubuntu.com"><img src="http://kernel.ubuntu.com/beta/media/ubuntu-footer-logo.png"></a>
                                </td>
                            </tr>
                        </table>
                    </div> <!-- dash-section -->

                </div>
            </div>
        </div>
    </body>

</html>
<!-- vi:set ts=4 sw=4 expandtab syntax=mako: -->

