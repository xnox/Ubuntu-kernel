#!/usr/bin/env python
#

from ktl.utils                  import run_command, o2ascii
from re                         import compile
import json

class gitError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

class git:
    verbose = False
    #commit_rc = compile('^commit/s+([a-f0-9]+)')
    commit_rc  = compile('^commit\s+([a-f0-9]+)\s*$')
    author_rc  = compile('^Author:\s+(.*)\s+<(.*)>$')
    date_rc    = compile('^Date:\s+(.*)$')
    buglink_rc = compile('^\s+BugLink:\s+http.*launchpad\.net/bugs/([0-9]+)$')
    sob_rc     = compile('^\s+Signed-off-by:\s+(.*)\s+<(.*)>$')
    ack_rc     = compile('^\s+Acked-by:\s+(.*)\s+<(.*)>$')

    # branches
    #
    # Return a list of all the git branches known to this git repository.
    #
    @classmethod
    def branches(cls):
        retval = []
        status, result = run_command("git branch", cls.verbose)
        if status == 0:
            for line in result.split('\n'):
                if line[0] == '*':
                    line = line[1:]
                retval.append(line.strip())
        else:
            raise gitError(result)

        return retval

    # current_branch
    #
    # Return a string that is the current branch checked out.
    #
    @classmethod
    def current_branch(cls):
        retval = ""
        status, result = run_command("git branch", cls.verbose)
        if status == 0:
            for line in result.split('\n'):
                if line[0] == '*':
                    retval = line[1:].strip()
                    break
        else:
            raise gitError(result)

        return retval

    # log
    #
    # The idea here is to take the output from "git log" and turn it into
    # a python dictionary structure which can be used by other scripts
    # to easily get information. One of the most uesful items in this
    # dictionary is a list of commits indexed by the related buglink.
    #
    @classmethod
    def log(cls):
        debug = False
        retval = {}
        retval['commits']       = []
        retval['buglink-index'] = {}
        status, result = run_command("git log", cls.verbose)
        commit = {}
        commit_text = []
        cn = 0
        if status == 0:
            for line in result:
                m = cls.commit_rc.match(line)
                if m != None:
                    sha1 = m.group(1)
                    # This is a new commit sha1. We are going to build up a dictionary entry
                    # for this one commit and then add it to the dictionary of all commits.
                    #
                    # When building up this commit's dictionary entry, we go back through the
                    # commit text looking for information we want to pull out and make easily
                    # available via dictionary keys.
                    #
                    if len(commit_text) > 0:
                        commit['txt'] = []
                        for txt in commit_text:

                            while True:
                                m = cls.author_rc.match(txt)          # Author
                                if m != None:
                                    id = {}
                                    id['name'] = m.group(1)
                                    id['email'] = m.group(2)
                                    commit['author'] = id
                                    break

                                m = cls.date_rc.match(txt)            # Date
                                if m != None:
                                    commit['date'] = m.group(1)
                                    break

                                m = cls.buglink_rc.match(txt)         # BugLink
                                if m != None:
                                    bug = m.group(1)
                                    if bug not in retval['buglink-index']:
                                        retval['buglink-index'][bug] = []

                                    if 'buglink' not in commit:
                                        commit['buglink'] = []

                                    retval['buglink-index'][bug].append(commit['sha1'])
                                    commit['buglink'].append(bug)
                                    commit['txt'].append(txt)
                                    break

                                m = cls.sob_rc.match(txt)             # Signed-off-by
                                if m != None:
                                    if 'sob' not in commit:
                                        commit['sob'] = []

                                    id = {}
                                    id['name'] = m.group(1)
                                    id['email'] = m.group(2)
                                    commit['sob'].append(id)
                                    commit['txt'].append(txt)
                                    break

                                m = cls.ack_rc.match(txt)             # Acked-by
                                if m != None:
                                    if 'acks' not in commit:
                                        commit['acks'] = []

                                    id = {}
                                    id['name'] = m.group(1)
                                    id['email'] = m.group(2)
                                    commit['acks'].append(id)
                                    commit['txt'].append(txt)
                                    break

                                commit['txt'].append(txt)
                                break

                        retval['commits'].append(commit)
                        cn += 1

                        # Reset out working variables
                        #
                        commit = {}
                        commit_text = []
                        commit['sha1'] = sha1
                        #if sha1 == "3ce5c896fc76921cfea33e1c4a85187f1001f23b":
                        #    debug = True
                        #else:
                        #    debug = False

                    else:
                        # This is the very first sha1
                        #
                        commit['sha1'] = m.group(1)
                else:
                    # This is text between two SHA1s, just add it to the working
                    # buffer for the current commit.
                    #
                    if debug:
                        print line
                    commit_text.append(line)

        else:
            raise gitError(result)

        return retval

# vi:set ts=4 sw=4 expandtab:
