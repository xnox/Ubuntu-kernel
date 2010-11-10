#!/usr/bin/env python
#

from sys                                import stdout

# stdo
#
# My own version of print but won't automatically add a linefeed to the end. And
# does a flush after every write.
#
def stdo(ostr):
    stdout.write(ostr)
    stdout.flush()
    return

# vi:set ts=4 sw=4 expandtab:
