#!/usr/bin/env python
#

from getopt                             import getopt, GetoptError
from ktl.utils                          import stdo, error

# CmdlineError
#
# The type of exception that will be raised by Cmdline.process() if there
# are command line processing errors.
#
class CmdlineError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

# Cmdline
#
# Do all the command line processing.
#
class Cmdline:
    # __init__
    #
    def __init__(self):
        self.cfg = {}

    # error
    #
    def error(self, e, defaults):
        if e != '': error(e)
        self.usage(defaults)

    # usage
    #
    # Prints out the help text which explains the command line options.
    #
    def usage(self, defaults):
        stdo("    Usage:                                                                                   \n")
        stdo("        %s [--verbose] [--config=<cfg file>] [--debug=<dbg options>]                         \n" % self.cfg['app_name'])
        stdo("                                                                                             \n")
        stdo("    Options:                                                                                 \n")
        stdo("        --help                                                                               \n")
        stdo("              Prints this help text.                                                         \n")
        stdo("                                                                                             \n")
        stdo("        --verbose        Give some feedback of what is happening while the script is         \n")
        stdo("                         running.                                                            \n")
        stdo("                                                                                             \n")
        stdo("        --config=<cfg file>                                                                  \n")
        stdo("                         The full path to the configuration file to use instead of           \n")
        stdo("                         the default location. The configuration file is used by the         \n")
        stdo("                         underlying LP library (lpltk).                                      \n")
        stdo("                                                                                             \n")
        stdo("        --debug=<debug options>                                                              \n")
        stdo("              Performs additional output related to the option enabled and the               \n")
        stdo("              application defined support for the option.                                    \n")
        stdo("                                                                                             \n")
        stdo("              enter                                                                          \n")
        stdo("              leave                                                                          \n")
        stdo("              verbose                                                                        \n")
        stdo("              vbi                                                                            \n")
        stdo("                  Verbose Bug Information, show details about each bug that is being         \n")
        stdo("                  processed.                                                                 \n")
        stdo("                                                                                             \n")
        stdo("              what                                                                           \n")
        stdo("                  Print out which bug is being processed.                                    \n")
        stdo("                                                                                             \n")
        stdo("              why                                                                            \n")
        stdo("                  Print out why a bug is being 'skipped'.                                    \n")
        stdo("                                                                                             \n")
        stdo("        --bugs=<bug-list>                                                                    \n")
        stdo("                         Use a comma separated list of bug id's and use that list            \n")
        stdo("                         instead of doing a LP task search for 'New' bugs with one           \n")
        stdo("                         of the regressions tags. This is a handy debugging aid.             \n")
        stdo("                                                                                             \n")
        stdo("        --package                                                                            \n")
        stdo("                         This must accompany any use of the --bugs command line switch.      \n")
        stdo("                         When a search is done, it is done for a source package, when        \n")
        stdo("                         specific bugs are processed, we need to know which tasks are        \n")
        stdo("                         to be targeted.                                                     \n")
        stdo("                                                                                             \n")
        stdo("        --show                                                                               \n")
        stdo("                         A less verbose, verbose for when you just want to know what         \n")
        stdo("                         changes the script is making to bugs.                               \n")
        stdo("                                                                                             \n")
        stdo("        --search-all     Normally, the last modified timestamp in the config file is         \n")
        stdo("                         uses for searching 'modified_since'. However, this flag will        \n")
        stdo("                         cause that to be ignored and all relevant bugs will be processed.   \n")
        stdo("                                                                                             \n")
        stdo("        --sru-data=<sru-data-file>                                                           \n")
        stdo("                         Some scripts may wan the sru data for some reason.                  \n")
        stdo("                                                                                             \n")
        stdo("    Examples:                                                                                \n")
        stdo("        %s                                                                                   \n" % self.cfg['app_name'])
        stdo("        %s --verbose                                                                         \n" % self.cfg['app_name'])
        stdo("        %s --bugs=519841 --package=linux                                                     \n" % self.cfg['app_name'])
        stdo('        %s --bugs="519841,743907" --package=linux                                            \n' % self.cfg['app_name'])

    # process
    #
    # As you can probably tell from the name, this method is responsible
    # for calling the getopt function to process the command line. All
    # parameters are processed into class variables for use by other
    # methods.
    #
    def process(self, argv, defaults):
        self.cfg['app_name'] = argv[0]
        result = True
        try:
            optsShort = ''

            # --verbose=[1-n]
            # --dry-run
            # --v1
            # --v2

            optsLong  = ['help', 'verbose', 'verbosity', 'config=', 'debug=', 'bugs=', 'package=', 'show', 'search-all', 'sru-data=' ]
            opts, args = getopt(argv[1:], optsShort, optsLong)

            for opt, val in opts:
                if (opt == '--help'):
                    raise CmdlineError('')

                elif (opt == '--verbose'):
                    self.cfg['verbose'] = True

                elif (opt == '--verbosity'):
                    self.cfg['verbosity'] = val

                elif (opt == '--show'):
                    self.cfg['show'] = True

                elif (opt == '--search-all'):
                    self.cfg['search_all'] = True

                elif opt in ('--config'):
                    self.cfg['configuration_file'] = val

                elif opt in ('--sru-data'):
                    self.cfg['sru_data_file'] = val

                elif opt in ('--debug'):
                    self.cfg['debug'] = val.split(',')

                elif opt in ('--bugs'):
                    self.cfg['bug ids'] = []
                    for v in val.split(','):
                        self.cfg['bug ids'].append(v.strip())

                elif opt in ('--package'):
                    self.cfg['package'] = val

        except GetoptError as e:
            print(e, defaults)
            raise CmdlineError('')

        return self.cfg

    # verify_options
    #
    def verify_options(self, cfg):

        if 'bug ids' in cfg and 'package' not in cfg:
            raise CmdlineError('You must specify a source package for the bugs specified.')
        return

# vi:set ts=4 sw=4 expandtab:

