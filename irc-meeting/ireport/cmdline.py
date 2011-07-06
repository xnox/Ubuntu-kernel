#!/usr/bin/python

# This utility updates the relevant bug entries in a database. The database
# entries have a "tasks" attribute that is a list of the update tasks to be
# performed on the database document.
#

from getopt                         import getopt, GetoptError
import sys

class CmdlineError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

class Cmdline:
    # __init__
    #
    def __init__(self, defaults):
        self.opts = defaults
        return
    
    # error
    #
    def error(self, e):
        if e != '': print e
        self.usage()

    # usage
    #
    # Prints out the help text which explains the command line options.
    #
    def usage(self):
        sys.stdout.write("""
    Usage:
        %s (--moin | --html)

    Options:
        --help           Prints this text.

        --moin           Output the report in MoinMoin format.

        --html           Output the report in Html format.

    Examples:
        %s --html < foo.txt
        %s --moin < foo.txt

""" % (self.opts['argv0'], self.opts['argv0'], self.opts['argv0']))

    # process
    #
    # As you can probably tell from the name, this method is responsible
    # for calling the getopt function to process the command line. All
    # parameters are processed into class variables for use by other
    # methods.
    #
    def process(self, argv):
        self.opts['argv0'] = argv[0]
        try:
            optsShort = ''
            optsLong  = ['help', 'moin', 'html']
            opts, args = getopt(argv[1:], optsShort, optsLong)

            for opt, val in opts:
                if (opt == '--help'):
                    raise CmdlineError('')

                if (opt == '--html'):
                    self.opts['format'] = 'html'

                if opt == '--moin':
                    self.opts['format'] = 'moin'


        except GetoptError, error:
            print(error)
            raise CmdlineError('')

        return self.opts

# vi:set ts=4 sw=4 expandtab:
