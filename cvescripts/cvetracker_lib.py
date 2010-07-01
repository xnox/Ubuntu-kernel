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

#------------------------------------------------------------------------------
# A bit of hackery to allow scripts to successful include this library when
# being called somewhere below the base directory. Fall back to current work
# directory if nothing is found to allow for creation.
#------------------------------------------------------------------------------
tracker_name = "ubuntu-cve-tracker"
tracker_dir  = os.path.join(os.getcwd(), tracker_name)
while not os.path.isdir(tracker_dir):
	tracker_libdir = os.path.dirname(os.path.dirname(tracker_dir))
	if tracker_libdir == os.path.dirname(tracker_dir):
		tracker_dir  = os.path.join(os.getcwd(), tracker_name)
		break
	tracker_dir = os.path.join(tracker_libdir, tracker_name)
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
# Change directory to the local branch.
#------------------------------------------------------------------------------
def TrackerChangetoBranch():
	owd = os.getcwd()
	if not owd == tracker_dir:
		try:
			os.chdir(tracker_dir)
		except:
			print "EE: Unable to change into " + tracker_dir + "!"
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
# Commit a change. If there actually is no change to commit, there is no need
# to commit and push.
#
# returns: 1 when there was actually something committed and 0 otherwise.
#------------------------------------------------------------------------------
def TrackerCommit(message):
	owd = TrackerChangetoBranch()
	rc = 0
	if os.system("bzr diff -q >/dev/null 2>&1") != 0:
		print "II: Commiting changes to local branch."
		if os.system("bzr commit -q -m \"" + message + "\"") == 0:
			TrackerPush()
			rc = 1
	os.chdir(owd)
	return rc

#------------------------------------------------------------------------------
# Check the origin branch for updates to be merge. If there are any, then
# merge them and automatically push the result to the team branch.
#------------------------------------------------------------------------------
def TrackerMerge():
	owd = TrackerChangetoBranch()

	msg = ""
	cmd = "bzr missing -q --theirs-only --line " + origin_branch
	for line in Popen(cmd, shell=True, stdout=PIPE).stdout:
		msg += line

	if msg != "":
		try:
			print "II: Merging tracker changes from origin."
			os.system("bzr merge -q " + origin_branch)
		except:
			print "EE: Failed!"
			raise

		#--------------------------------------------------------------
		# Argh, so if the master branch has merged back changes from
		# the team branch, there will be need to push but nothing to
		# commit!
		#--------------------------------------------------------------
		msg = "Merge of changes to master\n\n" + msg
		if not TrackerCommit(msg):
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

	local_branch = os.path.basename(tracker_dir)
	if not os.path.isdir(local_branch):
		print "II: Creating new tracker branch."
		os.system("bzr branch " + team_branch + " " + local_branch)
	TrackerPull()

