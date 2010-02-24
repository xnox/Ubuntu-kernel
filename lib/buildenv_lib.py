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

def GetDebianDir():
	cmd = "debian/rules printdebian"
	debdir = "debian"

	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		debdir = line.strip()
	p.stdout.close()
	p.stderr.close()

	return debdir

print GetDebianDir()
print sys.path
