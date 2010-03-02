#!/usr/bin/python
#
# Author: Stefan Bader <stefan.bader@canonical.com>
# Copyright (C) 2010
#
# This script is distributed under the terms and conditions of the GNU General
# Public License, Version 3 or later. See http://www.gnu.org/copyleft/gpl.html
# for details.

import sys, os
from subprocess import *

#------------------------------------------------------------------------------
# Get the directory where the debian config files are located. Usually this is
# "./debian", but for abstracted debian it might be an alternate directory.
#------------------------------------------------------------------------------
def GetDebianDir():
	cmd = "debian/rules printdebian"
	debdir = "debian"

	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		debdir = line.strip()
		break
	p.stdout.close()
	p.stderr.close()

	return debdir

def GetPackageName():
	changelog = os.path.join(GetDebianDir(), "changelog")
	pkg = None

	try:
		file = open(changelog, "r")
		pkg = file.readline().strip().split()[0]
		file.close()
	except:
		pass

	return pkg
