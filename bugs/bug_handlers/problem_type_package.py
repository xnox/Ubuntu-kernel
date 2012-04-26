#!/usr/bin/env python
#

from core.bug_handler                   import BugHandler
import gzip
from StringIO                           import StringIO

class ProblemTypePackage(BugHandler):
    """
    """

    # __init__
    #
    def __init__(self, cfg, lp, vout):
        BugHandler.__init__(self, cfg, lp, vout)
        self.comment = ''
        self.comment_subject = 'BOT: Examined VarLogDistupgradeApttermlog.gz'

    # dpkg_error
    #
    def dpkg_error(self, f):
        retval = False
        printed = 0
        previous_line = ''
        for line in f:
            if printed > 0:
                self.comment += "   " + line + "\n"
                printed += 1
                if printed == 2:
                    printed = 0
                    self.comment += '\n'

            if (('dpkg: ' in line) or ('dpkg' in line)) and (('linux-image' in line) or ('linux-headers' in line)):
                self.comment += "   %s\n" % previous_line
                self.comment += "   %s\n" % line
                printed += 1
                retval = True

            previous_line = line
        return retval

    # linux_image
    #
    def linux_image(self, f):
        retval = False
        printed = 0
        previous_line = ''
        for line in f:
            if printed > 0:
                self.comment += "   " + line + "\n"
                printed += 1
                if printed == 2:
                    printed = 0
                    self.comment += '\n'

            if ('dpkg: error processing linux-image' in line) or ('dpkg: Fehler beim Bearbeiten von linux-image' in line) or ('dpkg: error processing /var/cache/apt/archives/linux-image' in line) or ('dpkg: erro ao processar linux-image' in line):
                self.comment += "   %s\n" % previous_line
                self.comment += "   %s\n" % line
                printed += 1
                retval = True

            previous_line = line
        return retval

    # linux_headers
    #
    def linux_headers(self, f):
        retval = False
        printed = 0
        previous_line = ''
        for line in f:
            if printed > 0:
                self.comment += "   " + line + "\n"
                printed += 1
                if printed == 3:
                    printed = 0
                    self.comment += '\n'

            if 'dpkg: errore nell\'elaborare /var/cache/apt/archives/linux-headers' in line:
                self.comment += "   %s\n" % previous_line
                self.comment += "   %s\n" % line
                printed += 1
                retval = True

            previous_line = line
        return retval

    # update_initramfs
    #
    def update_initramfs(self, f):
        retval = False
        printed = 0
        previous_line = ''
        for line in f:
            if printed > 0:
                self.comment += "   " + line + "\n"
                printed += 1
                if printed == 5:
                    printed = 0
                    self.comment += '\n'

            if 'cpio: ./lib/udev/firmware.sh: Cannot stat:' in line:
                self.comment += "   %s\n" % previous_line
                self.comment += "   %s\n" % line
                printed += 1
                retval = True

            previous_line = line
        return retval

    # gzip_error
    #
    def gzip_error(self, f):
        retval = False
        printed = 0
        previous_line = ''
        for line in f:
            if printed > 0:
                self.comment += "   " + line + "\n"
                printed += 1
                if printed == 4:
                    printed = 0
                    self.comment += '\n'

            if 'dpkg-deb (subprocess): data: internal gzip read error:' in line:
                self.comment += "   %s\n" % previous_line
                self.comment += "   %s\n" % line
                printed += 1
                retval = True

            previous_line = line
        return retval

    # dependency_problem
    #
    def dependency_problem(self, f):
        retval = False
        printed = 0
        for line in f:
            if printed > 0:
                self.comment += "   " + line + "\n"
                printed += 1
                if printed == 3:
                    printed = 0
                    self.comment += '\n'

            if 'dependency problems prevent configuration of linux-image' in line:
                self.comment += "   %s\n" % line
                printed += 1
                retval = True

        return retval

    # no_space
    #
    def no_space(self, f):
        retval = False
        previous_line = ''
        for line in f:
            if 'mkinitramfs failure' in line:
                if 'No space left on device' in previous_line:
                    self.comment += "   It looks like you ran out of space on the partition containing /boot.\n\n"
                    retval = True
                    break

            previous_line = line
        return retval

    # cannot_stat
    #
    def cannot_stat(self, f):
        retval = False
        previous_line = ''
        for line in f:
            if 'Failed to copy' in line and 'to /initrd.img' in line:
                if 'cannot stat' in previous_line:
                    self.comment += "   It looks like you may have run out of space on the partition containing /.\n\n"
                    self.comment += "   %s\n" % previous_line
                    self.comment += "   %s\n" % line
                    self.comment += "\n"
                    retval = True
                    break

            previous_line = line
        return retval

    # failed_to_process
    #
    def failed_to_process(self, f):
        retval = False
        previous_line = ''
        for line in f:
            if 'Failed to process /etc/kernel/postinst.d' in line:
                (before, after) = line.split(' at ')
                (fid, where) = after.split(' line ')
                fid = fid.strip()
                line = line.replace('.', '')
                self.comment += "   Something failed in the kernel postinstall script.\n   %s line %s\n\n" % (fid, where)
                retval = True
                break

            previous_line = line
        return retval

    # grub_probe
    #
    def grub_probe(self, f):
        retval = False
        previous_line = ''
        for line in f:
            if '/usr/sbin/grub-probe: error:' in line:
                self.comment += "   This looks like a Grub issue rather than a kernel issue.\n"
                self.comment += "   %s\n" % line
                retval = True
                break

            previous_line = line
        return retval

    # run
    #
    def run(self, bug, task, package_name):
        retval = True
        self.comment = ""

        try:
            if bug.properties['ProblemType'] == 'Package':
                if 'varlogdistupgradeapttermlog-examined' in bug.tags:
                    raise Exception # Caught below

                gz_content = bug.find_attachment('VarLogDistupgradeApttermlog.gz')
                if gz_content is None:
                    raise Exception # Caught below

                content = gzip.GzipFile(fileobj=StringIO(gz_content)).read().split('\n')
                while (1):
                    if self.no_space(content):           break
                    if self.cannot_stat(content):        break
                    if self.grub_probe(content):         break
                    if self.dependency_problem(content): break
                    if self.gzip_error(content):         break
                    if self.update_initramfs(content):   break
                    if self.linux_headers(content):      break
                    if self.linux_image(content):        break
                    if self.dpkg_error(content):         break
                    if self.failed_to_process(content):  break

                    break # Final break to get out of the while loop

                if len(self.comment) > 0:
                    bug.add_comment(self.comment, self.comment_subject)
                    bug.tags.append('varlogdistupgradeapttermlog-examined')

        except: # Just used as a goto
            pass

        return retval

# vi:set ts=4 sw=4 expandtab:
