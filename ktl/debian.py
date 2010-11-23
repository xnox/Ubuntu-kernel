#!/usr/bin/env python
#

# Note: It would be nice to not tie the debian class to the git class but
#       be able to handle projects that are under bzr as well. But for
#       now, we need to be practical for now.

from ktl.git                            import Git, GitError
from re                                 import compile

# DebianError
#
class DebianError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

# Note: Think about cacheing the decoded, changelog dictionary.
#

class Debian:
    debug = False
    version_line_rc = compile("^linux \(([0-9]+\.[0-9]+\.[0-9]+-[0-9]+\.[0-9]+)\) (\S+); urgency=\S+$")

    # changelog
    #
    @classmethod
    def changelog(cls):
        retval = []

        # Find the correct changelog for this branch of this repository.
        #
        current_branch = Git.current_branch()

        # The standard location for a changelog is in a directory named 'debian'.
        #
        cl_path = 'debian/changelog'
        try:
            changelog_contents = Git.show(cl_path, branch=current_branch)
        except GitError:
            # If this is a kernel tree, the changelog is in a debian.<branch> directory.
            #
            cl_path = 'debian.' + current_branch + '/changelog'
            try:
                changelog_contents = Git.show(cl_path, branch=current_branch)
            except GitError:
                raise DebianError('Failed to find the changelog.')

        # The first line of the changelog should always be a version line.
        #
        m = cls.version_line_rc.match(changelog_contents[0])
        if m == None:
            raise DebianError("The first line in the changelog is not a version line.")

        content = []
        for line in changelog_contents:
            m = cls.version_line_rc.match(line)
            if m != None:
                version = ""
                release = ""
                pocket  = ""
                version = m.group(1)
                rp = m.group(2)
                if '-' in rp:
                    release, pocket = rp.split('-')
                else:
                    release = rp

                section = {}
                section['version'] = version
                section['release'] = release
                section['pocket']  = pocket
                section['content'] = content
                retval.append(section)
                content = []
            else:
                content.append(line)

        return retval

# vi:set ts=4 sw=4 expandtab:
