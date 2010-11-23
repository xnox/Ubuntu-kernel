#!/usr/bin/env python
#

from ktl.utils                  import run_command, o2ascii
from re                         import compile
import json

class GitError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

class Git:
    debug = False
    commit_rc  = compile('^commit\s+([a-f0-9]+)\s*$')
    author_rc  = compile('^Author:\s+(.*)\s+<(.*)>$')
    date_rc    = compile('^Date:\s+(.*)$')
    buglink_rc = compile('^\s+BugLink:\s+http.*launchpad\.net/bugs/([0-9]+)$')
    sob_rc     = compile('^\s+Signed-off-by:\s+(.*)\s+<(.*)>$')
    ack_rc     = compile('^\s+Acked-by:\s+(.*)\s+<(.*)>$')
    log_results = {}

    # is_repo
    #
    # If a "git branch" returns a 0 status and a non-empty result, then we
    # are in a git repository.
    #
    @classmethod
    def is_repo(cls):
        retval = False
        try:
            branches = cls.branches()
            if branches != "":
                retval = True

        except GitError as e:
            retval = False

        return retval

    # branches
    #
    # Return a list of all the git branches known to this git repository.
    #
    @classmethod
    def branches(cls):
        retval = []
        status, result = run_command("git branch", cls.debug)
        if status == 0:
            for line in result:
                if line[0] == '*':
                    line = line[1:]
                retval.append(line.strip())
        else:
            raise GitError(result)

        return retval

    # current_branch
    #
    # Return a string that is the current branch checked out.
    #
    @classmethod
    def current_branch(cls):
        retval = ""
        status, result = run_command("git branch", cls.debug)
        if status == 0:
            for line in result:
                if line[0] == '*':
                    retval = line[1:].strip()
                    break
        else:
            raise GitError(result)

        return retval

    # show
    #
    @classmethod
    def show(cls, obj, branch=''):
        cmd = 'git show '
        if branch != '':
            cmd += branch + ':'
        cmd += obj

        status, result = run_command(cmd, cls.debug)
        if status != 0:
            raise GitError(result)

        return result

    @classmethod
    def __process_log_commit(cls):
        pass

    # log
    #
    # The idea here is to take the output from "git log" and turn it into
    # a python dictionary structure which can be used by other scripts
    # to easily get information. One of the most uesful items in this
    # dictionary is a list of commits indexed by the related buglink.
    #
    @classmethod
    def log(cls, num=-1):
        debug = False
        cls.log_results = {}
        cls.log_results['commits']       = []
        cls.log_results['buglink-index'] = {}
        if num != -1:
            status, result = run_command("git log -%d" % (num), cls.debug)
        else:
            status, result = run_command("git log", cls.debug)
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
                                    if bug not in cls.log_results['buglink-index']:
                                        cls.log_results['buglink-index'][bug] = []

                                    if 'buglink' not in commit:
                                        commit['buglink'] = []

                                    cls.log_results['buglink-index'][bug].append(commit['sha1'])
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

                        cls.log_results['commits'].append(commit)
                        cn += 1

                        # Reset out working variables
                        #
                        commit = {}
                        commit_text = []
                        commit['sha1'] = sha1

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
            raise GitError(result)

        return cls.log_results

# vi:set ts=4 sw=4 expandtab:
