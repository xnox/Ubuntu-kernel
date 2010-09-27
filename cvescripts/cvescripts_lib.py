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
http_host = "chinstrap.ubuntu.com"
repo_path = os.path.join("/home", "kernel-ppa", "security")

#------------------------------------------------------------------------------
# Little helper to unquote a string
#------------------------------------------------------------------------------
def Unquote(s):
	if s[0] == '"' and s[-1] == '"':
		s = s[1:-1]
	return s

#------------------------------------------------------------------------------
# Little helper to remove quotes and expand the '~' constructs in paths.
#------------------------------------------------------------------------------
def ParsePath(path):
	path = Unquote(path)
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
# Set up login, ircnick, fullname and email for the current user.
#------------------------------------------------------------------------------
my_local_login = os.getlogin()
try:
	my_login = Unquote(config.get(my_local_login, "login"))
except:
	my_login = os.getlogin()
else:
	pass
try:
	my_ircnick = Unquote(config.get(my_local_login, "ircnick"))
except:
	my_ircnick = my_local_login
else:
	pass
try:
	my_fullname = Unquote(config.get(my_local_login, "fullname"))
except:
	my_fullname = os.getenv("DEBFULLNAME")
	if not my_fullname:
		print "EE: fullname is not set in config or environment"
		sys.exit(1)
else:
	pass
try:
	my_email = Unquote(config.get(my_local_login, "email"))
except:
	my_email = os.getenv("DEBEMAIL")
	if not my_email:
		print "EE: email is not set in config or environment"
		sys.exit(1)
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
from buildenv_lib import *
from cvetracker_lib import *

#------------------------------------------------------------------------------
# The scripts usually assume to be called from the base directory. The tracker
# tries to find itsefl. So decide if that was successful.
#------------------------------------------------------------------------------
if not os.path.isdir(tracker_dir):
	print "EE: Script called outside base directory!"
	sys.exit(1)
caller_dir = os.getcwd()

os.chdir(tracker_dir)
from workitem_lib import *
import cve_lib
os.chdir(os.path.dirname(tracker_dir))

#------------------------------------------------------------------------------
# Make bzr use the name and email defined here.
#------------------------------------------------------------------------------
os.system("bzr whoami \"" + my_fullname + " <" + my_email + ">\"")

#------------------------------------------------------------------------------
# Return a list of currently supported releases.
#------------------------------------------------------------------------------
def ListSupportedSeries():
	series = list(cve_lib.releases)
	for eol in cve_lib.eol_releases:
		if eol in series:
			series.remove(eol)

	return series

#------------------------------------------------------------------------------
# PkgList is a dictionary containing per series information:
# PkgList[<series>][<package-shortname>] = <package-name>
#------------------------------------------------------------------------------
PkgList = dict()
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
PkgList["maverick"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.35" )
])

#------------------------------------------------------------------------------
# Generate a list of [<package>, <series>] tuples for all currently supported
# kernel packages and return it.
#------------------------------------------------------------------------------
def ListLinuxPackagesAndSeries():
	pands = []

	for series in ListSupportedSeries():
		for pkg in PkgList[series]:
			pands.append([pkg, series])

	return pands

#------------------------------------------------------------------------------
# Generate a list of workitems that have active or pending package status on
# a kernel related package.
#------------------------------------------------------------------------------
def ListWorkItemsToDo(waiting=True, pending=True):
	os.chdir(tracker_dir)

	print "II: Searching for workitems..."
	(cves, uems) = cve_lib.get_cve_list()
	workitems = []
	for cve in sorted(cves):
		wi = WorkItem(cve_lib.find_cve(cve))
		valid = False
		for (pkg, series) in ListLinuxPackagesAndSeries():
			if waiting and wi.is_pkg_waiting(pkg, series):
				valid = True
				break
			if pending and wi.is_pkg_pending(pkg, series):
				valid = True
				break
		if valid:
			workitems.append(wi)

	os.chdir(os.path.dirname(tracker_dir))
	return workitems

#------------------------------------------------------------------------------
# Make sure the given directory exists. Bail out if it cannot be created.
#------------------------------------------------------------------------------
def AssertDir(dir, verbose=True):
	if not os.path.isdir(dir):
		if verbose:
			print "LL: Create " + dir + " ...",
		try:
			os.makedirs(dir)
		except:
			if verbose:
				print "failed!"
			sys.exit(1)

		if verbose:
			print "ok"

#------------------------------------------------------------------------------
# Return the path for saving workitem data/patches. Make sure path does exist.
#------------------------------------------------------------------------------
def GetSaveDir(name, create=True):
	savedir = os.path.join(os.path.dirname(tracker_dir), "workitems", name)
	if create:
		AssertDir(savedir)

	return savedir

#------------------------------------------------------------------------------
# Return the path for saving patches. Make sure the path does exist.
#------------------------------------------------------------------------------
def GetPatchDir(name, create=True):
	patchdir = os.path.join(GetSaveDir(name, create=create), "patches")
	if create:
		AssertDir(patchdir)

	return patchdir

#------------------------------------------------------------------------------
# Return a list of all working repositories (all series, all packages)
#------------------------------------------------------------------------------
def ListWorkareas():
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
def IsWorkareaClean(branch=None):
	clean = True
	owd = os.getcwd()
	for area in ListWorkareas():
		os.chdir(area)
		files = GitListFiles(opts="--others --modified")
		if files:
			clean = False
			print "WW: Workarea not clean " + area
			for file in files:
				print "WW: - " + file
		if branch:
			if GitGetCurrentBranch() != branch:
				print "WW: Not on branch ", branch, area
				clean = False
		os.chdir(owd)

	return clean


#------------------------------------------------------------------------------
# Create a new branch for a package which is based on the last released
# version of it.
#------------------------------------------------------------------------------
def CreateBranch(series, package, name, force=False, checkout=True):
	pkgname = PkgList[series][package]

	version = GetUploadVersion(series, pkgname)
	if not version:
		version = GetUploadVersion(series, pkgname, pocket="release")

	print "  - " + package + "(" + version + ") -> " + name
	if name in GitListBranches():
		if force == False:
			print "    EE: Branch " + name + " already exists"
			raise BranchExists()
		else:
			print "    WW: Trhowing away previous " + name
			os.system("git checkout -q master >/dev/null")
			os.system("git branch -D " + name + " >/dev/null")
	rc = os.system("git branch " + name + " Ubuntu-" + version)
	if rc == 0 and checkout:
		rc = os.system("git checkout -q " + name)

	return rc

#------------------------------------------------------------------------------
# Return the URL to the personal CVE publishing page.
#------------------------------------------------------------------------------
def GetHttpLink(subdir):
	return "http://{0}/~{1}/CVEs/{2}".format(http_host, my_login, subdir)

#------------------------------------------------------------------------------
# Pushes all files in the given subdirectory into a directory with the same
# basename on the http host.
#------------------------------------------------------------------------------
def Publish(subdir):
	rdir = os.path.basename(subdir)
	if rdir == "":
		print "EE: Refusing to publish when basename is empty!"
		sys.exit(1)
	print "II: Publishing " + rdir
	rdir = os.path.join("public_html", "CVEs", rdir)
	cmd = "ssh " + http_host + " test -d " + rdir + " && rm -rf " + rdir
	os.system(cmd)
	cmd = "ssh " + http_host + " mkdir -p " + rdir
	os.system(cmd)
	cmd  = "scp -r " + os.path.join(subdir, "*") + " " + http_host
	cmd += ":" + rdir
	os.system(cmd)

#------------------------------------------------------------------------------
# Extracts the patches from the work repositories and saves them into the save
# directory. Optionally pushes to the personal repositories.
#------------------------------------------------------------------------------
def SavePatches(winame, push=False):
	workdir = os.path.dirname(tracker_dir)

	print "II: Extracting patches..."
	for area in ListWorkareas():
		areaprinted = False
		patchdir = os.path.join(GetPatchDir(winame), area)
		AssertDir(patchdir, verbose=False)
		#--------------------------------------------------------------
		# Remove the files currently in patchdir (not to gather dead
		# entries).
		#--------------------------------------------------------------
		for file in os.listdir(patchdir):
			if file.split(".")[-1] == "txt":
				os.unlink(os.path.join(patchdir, file))
		#--------------------------------------------------------------
		# Export all patches since the branch was split off.
		#--------------------------------------------------------------
		os.chdir(os.path.join(workdir, area))
		mergebase = GitMergeBase("ubuntu/master", winame)
		if not mergebase:
			print "EE: Failed to find a mergebase for", area
			sys.exit(1)
		cmd  = "git format-patch --suffix=.txt -o " + patchdir
		cmd += " " + mergebase + ".." + winame
		for line in Popen(cmd, shell=True, stdout=PIPE).stdout:
			if not areaprinted:
				print "II:", area
				areaprinted = True
			print "II:", os.path.basename(line.strip())
		#--------------------------------------------------------------
		# While we are there and there are patches, push the branch.
		#--------------------------------------------------------------
		if areaprinted and push:
			print "II: Updating personal security repo."
			cmd = "git push -q origin +" + winame
			os.system(cmd)
		os.chdir(workdir)

