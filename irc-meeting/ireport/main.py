#!/usr/bin/python
#

from ireport.cmdline                import Cmdline
from ireport.cmdline                import CmdlineError
from ireport.report                 import MoinReport
import sys
import re

class Ireport:
    # __init__
    #
    def __init__(self):
        self.opts = {}
        self.opts['format'] = 'moin'
        self.state = ''
        self.metrics = {}
        return

    # process
    #
    def process(self, stream):
        first_topic_reached = False
        report = MoinReport()

        started = False

        for line in stream.readlines():

            # Separate the line into it's components. We're not interested in the timestamp
            # information. Lines that don't match the pattern, we're not interested in.
            #
            m = re.match('^[A-Za-z]+ [0-9]+ [0-9][0-9]:[0-9][0-9]:[0-9][0-9]\s<(.*)>\s(.*)$', line)
            if m != None:
                nic = m.group(1)
                msg = m.group(2)
            else:
                m = re.match('^<(.*)>\s(.*)$', line)
                if m != None:
                    nic = m.group(1)
                    msg = m.group(2)
                else:
                    continue

            print(msg)

            # Ignore everything until the meeting gets started
            #
            if not started:
                if msg.startswith('#startmeeting'):
                    started = True
                    report.start()
                continue

            # Ignore everything after the meeting has ended.
            #
            if msg.startswith('#endmeeting'):
                report.finish()
                started = False
                continue

            if 'meetingology' == nic: continue
            if msg.startswith('[LINK]'): continue

            if msg.startswith('[TOPIC]'):
                msg = msg.replace('[TOPIC]', '')
                msg = msg.replace(': Anyone have anything?', '')
                m = re.match('^\s*(.*)\s*\(\S*\)\s*$', msg)
                if m != None:
                    msg = m.group(1)
                first_topic_reached = True

                if 'Open Action Item' in msg:
                    msg = msg.replace('Open Action Item: ', '')
                    report.topic_open_action_item(nic, msg)
                else:
                    report.topic(msg)

                continue

            # Handle the URLs that ubottu generates
            #
            if 'ubottu' in nic:
                m = re.match('^(.*\] )(https:.*)$', msg)
                if m != None:
                    report.w(" * [[%s|%s]]" % (m.group(2), m.group(1)))
                continue

            if first_topic_reached:
                report.out(nic, msg)

            pass

    # main
    #
    def main(self):
        cmdline = Cmdline(self.opts)
        try:
            self.opts = cmdline.process(sys.argv)

            self.process(sys.stdin)

        # Handle the user presses <ctrl-C>.
        #
        except KeyboardInterrupt:
            pass

        # Handle ommand line errors.
        #
        except CmdlineError as e:
            cmdline.error(e.msg)


# vi:set ts=4 sw=4 expandtab:
