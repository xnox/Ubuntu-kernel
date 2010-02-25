#!/usr/bin/python
#==============================================================================
# Author: Stefan Bader <stefan.bader@canonical.com>
# Copyright (C) 2010
#
# This script is distributed under the terms and conditions of the GNU General
# Public License, Version 3 or later. See http://www.gnu.org/copyleft/gpl.html
# for details.
#==============================================================================
import sys, os, ConfigParser, optparse
from subprocess import *
from getopt import *

cvescripts_cfg = os.path.expanduser(os.path.join('~', '.cvescripts.cfg'))
repo_host = "kernel.ubuntu.com"
repo_path = os.path.join("/home", "kernel-ppa", "security")

#------------------------------------------------------------------------------
# Little helper to remove quotes and expand the '~' constructs in paths.
#------------------------------------------------------------------------------
def ParsePath(path):
	if path[0] == '"' and path[-1] == '"':
		path = path[1:-1]
	if path[0] == '~':
		path = os.path.expanduser(path)
	return path

#------------------------------------------------------------------------------
# Read in the config file and set variables.
#------------------------------------------------------------------------------
# key, val = config.item("section")
# val = config.get("section", "name")
config = ConfigParser.ConfigParser()
try:
	config.read(cvescripts_cfg)
except:
	print "EE: Configuration file {0} not found!".format(cvescripts_cfg)
	sys.exit(1)
else:
	pass

#------------------------------------------------------------------------------
# One required entry in the "Paths" section is the location of the
# ubuntu-cve-tracker bazar branch.
#------------------------------------------------------------------------------
try:
	cvetracker_dir = ParsePath(config.get("Paths", "cvetracker"))
except:
	print "No {0} in {1}!".format("cvetracker", "Paths")
	sys.exit(1)
else:
	pass


#------------------------------------------------------------------------------
# There might or might not be a local clone of the upstream linux repo. This
# will be used to speed up cloning and decrease disk usage.
#------------------------------------------------------------------------------
try:
	kernel_reference = ParsePath(config.get("Paths", "kernelreference"))
except:
	kernel_reference = ""
else:
	pass

#------------------------------------------------------------------------------
# Libraries shared by all kteam-tools will be found here (../lib)
#------------------------------------------------------------------------------
cvescripts_common_lib = os.path.dirname(os.path.abspath(sys.argv[0]))
cvescripts_common_lib = os.path.dirname(cvescripts_common_lib)
cvescripts_common_lib = os.path.join(cvescripts_common_lib, "lib")
sys.path.insert(0, cvescripts_common_lib)

#------------------------------------------------------------------------------
# Now with the path to the ubuntu-cve-tracker we can import libraries from
# them.
#------------------------------------------------------------------------------
sys.path.insert(0, os.path.join(cvetracker_dir, "scripts"))
import cve_lib

def ListRepos(series):
	repos = []
	path = os.path.join(repo_path, series)
	cmd = "ssh {0} ls -1 {1}".format(repo_host, path)

	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		repos.append(line.strip())
	p.stdout.close()
	p.stderr.close()

	return repos

def ListSupportedSeries():
	series = cve_lib.releases
	for eol in cve_lib.eol_releases:
        	if eol in series:
                	series.remove(eol)

	return series

