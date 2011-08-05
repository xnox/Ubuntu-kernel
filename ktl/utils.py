#!/usr/bin/env python
#

from sys                    import stdout, stderr
from commands               import getstatusoutput
from decimal                import Decimal
import json
from os.path                import exists, getmtime
from time                   import time
from datetime               import datetime
import re

# o2ascii
#
# Convert a unicode string or a decial.Decimal object to a str.
#
def o2ascii(obj):
    retval = None
    if type(obj) != str:
        if type(obj) == unicode:
            retval = obj.encode('ascii', 'ignore')
        elif type(obj) == Decimal:
            retval = str(obj)
        elif type(obj) == int:
            retval = str(obj)
    else:
        retval = obj
    return retval

# fileage
#
def fileage(filename):
    # get time last modified
    if not exists(filename):
        return None

    # time in seconds since last change to file
    return time() - getmtime(filename)

# run_command
#
def run_command(cmd, dbg=False, dry_run=False):
        """
        Run a command in the shell, returning status and the output.
        prints a message if there's an error, and raises an exception.
        """
        status = ""
        result = ""
        if not dry_run:
            status, result = getstatusoutput(cmd)
            debug("     cmd: '%s'" % cmd, dbg)
            debug("  status: '%d'" % status, dbg)
            debug("  result: '%s'" % result, dbg)
        else:
            debug("run_command: '%s'" % (cmd))

        return status, result.split('\n')

# error
#
# Print strings to standard out preceeded by "error:".
#
def error(out):
    stderr.write("\n ** Error: %s\n" % out)
    stderr.flush()

# debug
#
# Print strings to standard out preceeded by "debug:".
#
def debug(out, dbg=False, prefix=True):
    if dbg:
        if prefix:
            stdo("debug: ")
        stdo(out)

# eout
#
# Print out an error message to stderr in a standard format. Note, since this uses
# the stderr.write() function, the emsg must contain any desired newlines.
#
def eout(emsg):
    stderr.write("\n")
    stderr.write("  ** Error: %s" % (emsg))
    stderr.write("\n")
    stderr.flush()

# stdo
#
# My own version of print but won't automatically add a linefeed to the end. And
# does a flush after every write.
#
def stdo(ostr):
    stdout.write(ostr)
    stdout.flush()
    return

def stde(ostr):
    stderr.write(ostr)
    stderr.flush()
    return

def dump(obj):
    stdo(json.dumps(obj, sort_keys=True, indent=4))
    stdo('\n')

# date_to_string
#
def date_to_string(date):
    """
    Return a standard, string representation of the date given. It is assumed that the
    date is UTC based.
    """
    return "None" if date is None else date.strftime("%A, %d. %B %Y %H:%M UTC")

# string_to_date
#
def string_to_date(date):
    """
    Return a datetime object based on the string in a well known format.
    """
    return datetime.strptime(date, '%A, %d. %B %Y %H:%M UTC')

# setBugProperties
#
def setBugProperties(bug, newprops):
    """
    Set key:value pairs in the bug description. This
    follows a convention established in lpltk
    Input: a lpltk bug object and a dictionary
    of key:value pairs
    """
    # Set a name:value pair in a bug description
    olddescr = bug.description
    newdscr = ''
    props = bug.properties
    re_kvp            = re.compile("^(\s*)([\.\-\w]+):\s*(.*)$")
    # copy everything, removing an existing one with this name if it exists
    for line in olddescr.split("\n"):
        m = re_kvp.match(line)
        if m:
            # There is a property on this line (assume only one per line)
            # see if it matches the one we're adding
            level = m.group(1)
            item = m.group(2)
            value = m.group(3)
            key = item
            if len(level) > 0:
                key = "%s.%s" %(last_key[''], item)
            if key in newprops:
                # we're going to be adding this one, remove the existing one
                continue
        newdscr = newdscr + line + '\n'

    for k in newprops:
        newdscr = newdscr + '%s:%s\n' % (k, newprops[k])
    bug.description = newdscr
    return

# vi:set ts=4 sw=4 expandtab:
