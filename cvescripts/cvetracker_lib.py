#!/usr/bin/python
#==============================================================================
# Author: Stefan Bader <stefan.bader@canonical.com>
# Copyright (C) 2010
#
# This script is distributed under the terms and conditions of the GNU General
# Public License, Version 3 or later. See http://www.gnu.org/copyleft/gpl.html
# for details.
#==============================================================================
import os, sys
from subprocess import *

origin_branch  = "lp:~ubuntu-security/ubuntu-cve-tracker/master"
team_branch    = "lp:~canonical-kernel-team/ubuntu-cve-tracker/kernel-team"
local_branch   = "ubuntu-cve-tracker"
tracker_dir    = os.path.join(os.getcwd(), local_branch)
tracker_libdir = os.path.join(tracker_dir, "scripts")

#------------------------------------------------------------------------------
# Conditionally insert the tracker script directory to the system paths to
# allow importing libraries from there.
#------------------------------------------------------------------------------
if not tracker_libdir in sys.path:
	sys.path.insert(0, tracker_libdir)

#------------------------------------------------------------------------------
# Test availability of bzr
#------------------------------------------------------------------------------
if os.system("bzr help >/dev/null 2>&1"):
	print "EE: bzr is required (sudo apt-get install bzr)!"
	sys.exit(1)

#------------------------------------------------------------------------------
# Change directory to the local branch. This assumes the current directory is
# either one above or right in the local branch directory.
#------------------------------------------------------------------------------
def TrackerChangetoBranch():
	owd = os.getcwd()
	if not os.path.basename(owd) == local_branch:
		try:
			os.chdir(local_branch)
		except:
			print "EE: Unable to change into " + local_branch + "!"
			raise

	return owd
	
#------------------------------------------------------------------------------
# Send back any updates to the team branch.
#------------------------------------------------------------------------------
def TrackerPush():
	owd = TrackerChangetoBranch()

	try:
		print "II: Pushing tracker updates to team branch."
		os.system("bzr push -q " + team_branch)
	except:
		print "EE: Failed!"
		raise

	os.chdir(owd)

#------------------------------------------------------------------------------
# Check the origin branch for updates to be merge. If there are any, then
# merge them and automatically push the result to the team branch.
#------------------------------------------------------------------------------
def TrackerMerge():
	owd = TrackerChangetoBranch()
	rc  = 0

	try:
		rc = os.system("bzr missing -q --theirs-only " + origin_branch)
	except:
		print "EE: Failed to check for pending master updates!"
		raise

	if rc:
		try:
			print "II: Merging tracker changes from origin."
			os.system("bzr merge " + origin_branch)
		except:
			print "EE: Failed!"
			raise

		TrackerPush()

	os.chdir(owd)

#------------------------------------------------------------------------------
# Updates the tracker from the team and origin branches.
#------------------------------------------------------------------------------
def TrackerPull():
	owd = TrackerChangetoBranch()
	rc  = 0

	print "II: Pulling tracker changes from team branch."
	try:
		os.system("bzr pull -q " + team_branch)
	except:
		print "EE: Failed!"
		raise

	TrackerMerge()

	os.chdir(owd)

#------------------------------------------------------------------------------
# Commit a change
#------------------------------------------------------------------------------
def TrackerCommit(message):
	owd = TrackerChangetoBranch()

	print "II: Commiting changes to local branch."
	if os.system("bzr commit -q -m '" + message + "'") == 0:
		TrackerPush()

#------------------------------------------------------------------------------
# Create a local copy of the tracker (assumes to be in the base directory.
#------------------------------------------------------------------------------
def TrackerCreate():
	#----------------------------------------------------------------------
	# Is the LP user set?
	#----------------------------------------------------------------------
	p = Popen("bzr launchpad-login -q", shell="true", stdout=PIPE)
	if p.stdout.readline().strip() == "":
		print "EE: Need to set launchpad login " + \
		      "(bzr launchpad-login <name>)!"
		p.stdout.close()
		sys.exit(1)
	p.stdout.close()

	if not os.path.isdir(local_branch):
		print "II: Creating new tracker branch."
		os.system("bzr branch " + team_branch + " " + local_branch)
	TrackerPull()

