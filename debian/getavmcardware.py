#! /usr/bin/python

import os, re, sys
import ftplib
import fnmatch, glob
import shutil, tempfile, tarfile

download_base = 'avm-download'
server = 'ftp.avm.de'
prefix = '/cardware'
versions = ['93', '92', '91', '90', '82', '81', '80']
active_cards = [
         'a1',
         'a1_pcm',
         'a1_plus',
         'b1',
         'b1_pci',
         'b1_pcm',
         'b1_usb',
         'c2',
         'c4',
         't1',
         't1-b',
]
fritz_cards = [
         'fritzcrd.dsl',
         'fritzcrd.dsl_sl',
         'fritzcrd.dsl_sl_usb',
         'fritzcrd.dsl_usb',
         'fritzcrd.dsl_usb_analog',
         'fritzcrd.dsl_v20',
         'fritzcrd.pci',
         'fritzcrd.pcm',
         #'fritzcrd.pnp', not supported anymore
         'fritzcrd.usb',
         'fritzcrdusb-v20',
         'fritzgsm',
         'fritzx.isdn',
         'fritzx.pc',
         'fritzx.usb',
         'fritzxpc.v20',
         'fritzxpc.v30',
         'fritzxusb.v20',
         'fritzxusb.v30',
]

cards = active_cards + fritz_cards
cards = fritz_cards[:]
 
class Driver:
    def __init__(self, card, is64bit=False, localdir = None, tarname = None, tarsize = 0):
        self.name = None
        self.card = card
        self.cards = set([card])
        self.dir = None
        self.tarname = tarname
        self.tarsize = tarsize
        self.localdir = localdir
        self.is64bit = is64bit
        self.set_name_and_version()

    def set_name_and_version(self):
        if self.tarname:
            match = re.match(r'(\w+)-suse[\d.]*(?:-64bit)?-([\d.-]+)\.tar.gz', self.tarname)
            self.name = match.group(1)
            self.version = match.group(2)

    def getline(self, s):
        self.line = s
    
    def get_tarball_info(self, ftp, prefix):
        if self.is64bit:
            linuxdir = 'linux_64bit'
        else:
            linuxdir = 'linux'
        for version in versions:
            self.directory = '%s/%s/%s/suse.%s' % (prefix, self.card, linuxdir, version)
            try:
                ftp.cwd(self.directory)
                files = ftp.nlst()
            except ftplib.error_perm, msg:
                continue
            p = '*suse*tar.gz'
            files = fnmatch.filter(files, p)
            if len(files):
                self.tarname = files[0]
                self.set_name_and_version()
                ftp.retrlines('LIST %s/%s' % (self.directory, files[0]), self.getline)
                self.tarsize = int(self.line.split()[4])
                return
        return

    def is_download_needed(self, ftp, localdir):
        try:
            localsize = os.stat(os.path.join(localdir, self.tarname))[6]
        except OSError:
            return True
        return localsize != self.tarsize

    def fetch_tarball(self, ftp, localdir):
        print 'get %-23s %9d %s' % (self.card, self.tarsize, self.tarname)
        self.localdir = localdir
        ftp.retrbinary('RETR %s/%s' % (self.directory, self.tarname),
                       file(os.path.join(localdir, self.tarname), 'wb').write)

    def read_tarball(self):
        return file(os.path.join(self.localdir, self.tarname)).read()

    def extract_tarball(self):
        cwd = os.getcwd()
        tmpdir = tempfile.mkdtemp()
        print self.name, tmpdir
        tar = tarfile.open(os.path.join(self.localdir, self.tarname), 'r:gz')
        os.chdir(tmpdir)
        for tarinfo in tar:
            tar.extract(tarinfo)
        tar.close()
        os.chdir(cwd)
        if os.path.isdir(os.path.join(tmpdir, 'active')):
            driverdir = 'active'
        elif os.path.isdir(os.path.join(tmpdir, 'fritz')):
            driverdir = 'fritz'
        else:
            raise "ERROR, unknown directory"
        print driverdir, self.name
        try:
            shutil.rmtree(os.path.join(driverdir, self.name))
        except:
            pass
        if self.is64bit:
            targetdir = driverdir + '64'
        else:
            targetdir = driverdir
        try:
            os.makedirs(targetdir)
        except:
            pass
        shutil.move(os.path.join(tmpdir, driverdir),
                    os.path.join(targetdir, self.name))
        shutil.rmtree(tmpdir)

def do_download(cards, download_dir, is64bit):
    tarballs = []
    ftp = ftplib.FTP(server)
    ftp.login()
    for card in cards:
        d = Driver(card, is64bit)
        d.get_tarball_info(ftp, prefix)
        if d.tarname == None:
            continue
        try:
            shutil.rmtree('%s/%s' % (download_dir, card))
        except:
            pass
        os.makedirs('%s/%s' % (download_dir, card))
        if d.is_download_needed(ftp, '%s/%s' % (download_dir, card)):
            d.fetch_tarball(ftp, '%s/%s' % (download_dir, card))
        else:
            print "up to date:", card, d.tarname
        tarballs.append(d)

def do_compare(tarballs, download_dir, is64bit):
    if tarballs == None:
        tarballs = []
        for tb in glob.glob('%s/*/*.tar.gz' % download_dir):
            localdir, tarname = os.path.split(tb)
            card = os.path.basename(localdir)
            localsize = os.stat(tb)[6]
            d = Driver(card, is64bit, localdir, tarname, localsize)
            tarballs.append(d)
    
    tbset = set(tarballs)
    for i1 in range(len(tarballs)):
        tb1 = tarballs[i1]
        data1 = tb1.read_tarball()
        for i2 in range(i1+1, len(tarballs)):
            tb2 = tarballs[i2]
            data2 = tb2.read_tarball()
            if data1 == data2:
                print "Identical:", tb1.card, tb2.card
                tb1.cards.add(tb2.card)
                tb2.cards.add(tb1.card)
                tbset.discard(tb2)
    for i in range(len(tarballs)-1, -1, -1):
        if not tarballs[i] in tbset:
            del tarballs[i]
    return tarballs

def extract_tarballs(tarballs):
    for tb in tarballs:
        tb.extract_tarball()

def do_arch(is64bit):
    if is64bit:
        download_dir = os.path.join(download_base, '64')
    else:
        download_dir = os.path.join(download_base, '32')
    if not os.path.isdir(download_dir):
        os.makedirs(download_dir)
    tarballs = None
    tarballs = do_download(cards, download_dir, is64bit)
    tarballs = do_compare(tarballs, download_dir, is64bit)
    for tb in tarballs:
        print tb.cards
    extract_tarballs(tarballs)

def main():
    do_arch(True)  # 64bit
    do_arch(False) # 32bit

main()
