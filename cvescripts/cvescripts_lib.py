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
from git_lib import *

#------------------------------------------------------------------------------
# Now with the path to the ubuntu-cve-tracker we can import libraries from
# them.
#------------------------------------------------------------------------------
sys.path.insert(0, os.path.join(cvetracker_dir, "scripts"))
import cve_lib

#def ListRepos(series):
#	repos = []
#	path = os.path.join(repo_path, series)
#	cmd = "ssh {0} ls -1 {1}".format(repo_host, path)
#
#	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
#	for line in p.stdout:
#		repos.append(line.strip())
#	p.stdout.close()
#	p.stderr.close()
#
#	return repos

def ListSupportedSeries():
	series = cve_lib.releases
	for eol in cve_lib.eol_releases:
        	if eol in series:
                	series.remove(eol)

	return series

#------------------------------------------------------------------------------
# PkgList is a dictionary containing per series information:
# PkgList[<series>][<package-shortname>] = <package-name>
#------------------------------------------------------------------------------
PkgList = {}
PkgList["dapper"] = dict([
        ( "linux", "linux-source-2.6.15" ),
        ( "lbm", "linux-backports-modules-2.6.15" )
])
PkgList["hardy"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.24" ),
	( "lrm", "linux-restricted-modules-2.6.24" ),
	( "lum", "linux-ubuntu-modules-2.6.24" )
])
PkgList["intrepid"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.27" ),
	( "lrm", "linux-restricted-modules" )
])
PkgList["jaunty"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.28" ),
	( "lrm", "linux-restricted-modules" )
])
PkgList["karmic"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.31" ),
])
PkgList["lucid"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.32" )
])

#------------------------------------------------------------------------------
# Return a list of all working repositories (all series, all packages)
#------------------------------------------------------------------------------
def ListAllWorkareas():
	list = []
	for series in ListSupportedSeries():
		if not series in PkgList.keys():
			continue
		for package in sorted(PkgList[series].keys()):
			list.append(os.path.join(series, package))

	return list

#------------------------------------------------------------------------------
# Verify whether all repositories are clean and return True or False
#------------------------------------------------------------------------------
def IsWorkareaClean():
	clean = True
	owd = os.getcwd()
	for area in ListAllWorkareas():
		os.chdir(area)
		files = GitListFiles("--others --modified")
		if files:
			clean = False
			print "WW: Workarea not clean " + area
			for file in files:
				print "WW: - " + file
		os.chdir(owd)

	return clean

def GetUploadVersion(series, package, pocket="updates"):
	if not pocket in [ "release", "updates", "security", "proposed" ]:
		raise NameError(pocket)
	cmd = "rmadison --arch=source " + package
	version = None
	stdout = Popen(cmd, shell=True, stdout=PIPE).stdout
	for line in stdout:
		fields = line.strip().split("|")
		rpocket = fields[2].strip().split("/")[0]
		if rpocket == series and not version:
			version = fields[1].strip()
			continue
		if rpocket == series + "-" + pocket:
			version = fields[1].strip()
			break
	stdout.close()
	
	return version

