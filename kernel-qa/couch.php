<?php
include_once "CouchDB.php";

$couchdb = new CouchDB("kernel_test_data");

$map = <<<MAP
function(doc) 
{
    if (doc.event == 'SCALE, 2010')
    {
        emit(null, doc.testResults);
    }
}
MAP;
$view = '{"map":"'.$map.'"}';

$view_results = $couchdb->send('/_temp_view', 'post', $view);
$results = $view_results->getBody(true);


foreach ($results->rows as $r => $row)
{
    $x = $couchdb->get_item($row->id);
    $y = $x->getBody(true);

    $t = (array)$y->testResults->tests;

    if (count($t) > 0)
    {
        foreach (array_keys($t) as $k)
        {
            $totals[$t[$k]][$k]++;

            $tests[$k] = 1;
        }
    }
}

echo '<html>';
echo '    <head>';
echo '         <meta http-equiv="refresh" content="5" />';
echo '    </head>';
echo '    <body>';
echo '        <center><img src="UbuntuLogo.png" alt="" /></center>';
echo '<br />';
echo '<br />';
echo '<table width="100%">';
echo '    <tr>';
echo '        <th align="left"><strong>Test Name</strong></th><th align="left"><strong>Successs</strong></th><th align="left"><strong>Failure</strong></th><th align="left"><strong>Skipped</strong></th>';
echo '    </tr>';
$keys = array_keys($tests);
sort($keys);
foreach ($keys as $k)
{
    echo '    <tr>';
    echo '        <td>' . $k . '</td><td>' . $totals[success][$k] . '</td><td>' . $totals[failure][$k] . '</td><td>' . $totals[skip][$k] . '</td>';
    echo '    </tr>';
}
echo '</table>';
echo "<h3>Total Systems Tested: ".$results->total_rows."</h3><br />";
echo '    </body>';
echo '</html>';
?>
<!--
  vi:set ts=4 sw=4 expandtab:
-->
