#!/usr/bin/env python
#

from getopt                             import getopt, GetoptError
from os                                 import path, makedirs, listdir
from shutil                             import rmtree, copytree
import json

from lib.core.utils                     import stdo, error, json_load, file_load, FileDoesntExist
from lib.core.dbg                       import Dbg

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
class Cmdline:
    """
    Handle all the command line processing for the application.
    """
    # error
    #
    def error(self, e, defaults):
        """
        Simple helper which prints out an error message and then prints out the usage.
        """
        if e != '': error("%s\n" % e)
        self.usage(defaults)

    # usage
    #
    def usage(self, defaults):
        """
        Prints out the help text which explains the command line options.
        """
        stdo("    Usage:                                                                                   \n")
        stdo("        %s [<options>] <test-results-root>                                                   \n" % defaults['app_name'])
        stdo("                                                                                             \n")
        stdo("    Options:                                                                                 \n")
        stdo("        --help           Prints this text.                                                   \n")
        stdo("                                                                                             \n")
        stdo("        --debug=<debug options>                                                              \n")
        stdo("                         Performs additional output related to the option enabled and        \n")
        stdo("                         the application defined support for the option.                     \n")
        stdo("                                                                                             \n")
        stdo("                         Recognized debug options:                                           \n")
        stdo("                             enter                                                           \n")
        stdo("                             leave                                                           \n")
        stdo("                             verbose                                                         \n")
        stdo("                             cfg                                                             \n")
        stdo("                                                                                             \n")
        stdo("    Examples:                                                                                \n")
        stdo("        %s --debug=\"enter,leave,verbose\"                                                   \n" % defaults['app_name'])
        stdo("        %s /var/lib/jenkins/jobs/foo/builds/01                                               \n" % defaults['app_name'])

    # process
    #
    def process(self, argv, defaults):
        """
        This method is responsible for calling the getopt function to process the command
        line. All parameters are processed into class variables for use by other methods.
        """
        result = True
        try:
            cfg = defaults

            optsShort = ''
            optsLong  = ['help', 'debug=']
            opts, args = getopt(argv[1:], optsShort, optsLong)

            for opt, val in opts:
                if (opt == '--help'):
                    raise CmdlineError('')

                elif opt in ('--debug'):
                    cfg['debug'] = val.split(',')
                    for level in cfg['debug']:
                        if level not in Dbg.levels:
                            Dbg.levels.append(level)

            if result: # No errors yet
                # There might be some bugs listed on the command line.
                #
                if len(args) > 0:
                    cfg['root'] = args[0]


            # Required options:
            #
            if 'root' not in cfg:
                raise CmdlineError("You must specify the root directory of the test results that you wish to incorporate into the test-results repository.")

        except GetoptError, error:
            raise CmdlineError(error)

        return cfg

# Exit
#
class Exit():
    """
    If an error message has already been displayed and we want to just exit the app, this
    exception is raised.
    """
    pass

from xml.dom.minidom                    import Node, parseString
from lib.core.utils                     import dump

# x2dict
#
class x2dict(dict):

    # __init__
    #
    def __init__(self, xmlstring, *args):
        Dbg.enter("x2dict.__init__")

        dict.__init__(self, args)

        doc = parseString(xmlstring)

        x = self.e2d(doc)
        for k in x:
            self[k] = x[k]

        Dbg.leave("x2dict.__init__")

    # e2d
    #
    def e2d(self, node):
        Dbg.enter("x2dict.e2d")

        retval = None

        child = node.firstChild
        if not child:
            Dbg.verbose('No child nodes\n')
            Dbg.leave("x2dict.e2d")
            return None

        retval = {}
        text = ''
        while child is not None:
            if child.nodeType == Node.TEXT_NODE:
                Dbg.verbose('nodeType: Node.TEXT_NODE\n')
                Dbg.verbose('data: \'%s\'\n' % child.data.strip())
                text = child.data.strip()
                if text != '':
                    retval = { 'text' : text.split('\n') }
            elif child.nodeType == Node.ELEMENT_NODE:
                Dbg.verbose('tagName: %s\n' % child.tagName)
                Dbg.verbose('nodeType: Node.ELEMENT_NODE\n')

                if child.tagName not in retval:
                    Dbg.verbose('Creating retval[%s] list.\n' % (child.tagName))
                    retval[child.tagName] = []

                neo = self.e2d(child)
                if child.hasAttributes:
                    if neo is None:
                        neo = {}

                    for a in child.attributes.keys():
                        Dbg.verbose("attributes[%s] = %s\n" % (a, child.attributes[a].value))
                        neo[a] = child.attributes[a].value

                retval[child.tagName].append(neo)

            child = child.nextSibling

        Dbg.leave("x2dict.e2d")
        return retval

    # dump
    def dump(self):
        dump(self)

# JenkinsTestResultsTree
#
class JenkinsTestResultsTreeError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = "%s" % error

class JenkinsTestResultsTree():
    # __init__
    #
    def __init__(self, root):
        Dbg.enter("JenkinsTestResultsTree.__init__")

        self.root = root
        self.archive = path.join(root, 'archive')

        Dbg.leave("JenkinsTestResultsTree.__init__")

    # results
    #
    @property
    def results(self):
        Dbg.enter("JenkinsTestResultsTree.results")

        retval = None
        try:
            content = file_load(path.join(self.root, 'junitResult.xml'))
            retval = x2dict(content)

        except FileDoesntExist as e:
            raise JenkinsTestResultsTreeError('The Jenkins test results tree (%s) specified on the command line does\n           not appear to be a valid results tree, the file (%s)\n           does not exist.\n' % (self.root, e.file_name))
            Dbg.leave("TestResultsRepository.__init__")

        Dbg.leave("JenkinsTestResultsTree.results")
        return retval

    @property
    def attributes(self):
        Dbg.enter("JenkinsTestResultsTree.attributes")

        retval = None
        try:
            retval = json_load(path.join(self.archive, 'test-attributes.json'))

        except FileDoesntExist as e:
            raise JenkinsTestResultsTreeError('The Jenkins test results tree (%s) specified on the command line does\n           not appear to be a valid results tree, the file (%s)\n           does not exist.\n' % (self.root, e.file_name))
            Dbg.leave("TestResultsRepository.__init__")

        Dbg.leave("JenkinsTestResultsTree.attributes")
        return retval

    @property
    def archive(self):
        Dbg.enter("JenkinsTestResultsTree.archive")
        retval = path.join(self.root, 'archive')
        Dbg.leave("JenkinsTestResultsTree.archive")
        return retval

# TestResultsRepository
#
class TestResultsRepositoryError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = "%s" % error

class TestResultsRepository():
    # __init__
    #
    def __init__(self, rc = 'test-results.rc'):
        '''
        Load the test-results.rc file into self.
        '''
        Dbg.enter("TestResultsRepository.__init__")

        try:
            self.cfg = json_load(rc)

        except FileDoesntExist as e:
            raise TestResultsRepositoryError('The file (%s) does not exist.\n' % e.file_name)
            Dbg.leave("TestResultsRepository.__init__")

        Dbg.leave("TestResultsRepository.__init__")

    # initialize_results_dir
    #
    def initialize_results_dir(self, dirname):
        Dbg.enter("TestResultsRepository.initialize_results_dir")

        self.results_dir = path.join(self.cfg['repository_root'], dirname)
        if path.exists(self.results_dir):
            Dbg.verbose("%s exists.\n" % (self.results_dir))
            rmtree(self.results_dir)
        else:
            Dbg.verbose("%s does not exist.\n" % (self.results_dir))
        makedirs(self.results_dir)
        Dbg.leave("TestResultsRepository.initialize_results_dir")
        return self.results_dir

    def store_results(self, data):
        Dbg.enter("TestResultsRepository.store_results")

        destdir = path.join(self.results_dir, 'results.json')
        Dbg.verbose('destdir: "%s"' % destdir)
        with open(destdir, 'w') as f:
            f.write(json.dumps(data, sort_keys=True, indent=4))

        Dbg.leave("TestResultsRepository.store_results")

    def ingest(self, jtr):
        Dbg.enter("TestResultsRepository.ingest")

        data = {}
        data['results'] = jtr.results
        data['attributes'] = jtr.attributes

        # Name of the directory where we will be storing the results
        #
        tr_dest = "%s.%s" % (data['attributes']['environ']['NODE_NAME'], data['attributes']['environ']['BUILD_ID'])
        dest = self.initialize_results_dir(tr_dest)
        for n in listdir(jtr.archive):
            if path.isdir(path.join(jtr.archive, n)):
                copytree(path.join(jtr.archive, n), path.join(dest, n))

        self.store_results(data)

        Dbg.leave("TestResultsRepository.ingest")

# vi:set ts=4 sw=4 expandtab:

