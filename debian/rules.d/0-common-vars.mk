# Rip some version information from our changelog
release	:= $(shell sed -n '1s/^.*(\(.*\)\..*-.*).*$$/\1/p' debian/changelog)
revisions := $(shell sed -n 's/^linux-restricted-modules-2.6.24\ .*($(release)\.\(.*-.*\)).*$$/\1/p' debian/changelog | tac)
revision ?= $(word $(words $(revisions)),$(revisions))
prev_revisions := $(filter-out $(revision),0.0 $(revisions))
prev_revision := $(word $(words $(prev_revisions)),$(prev_revisions))
#  
gitver=$(shell if test -f .git/HEAD; then cat .git/HEAD; else uuidgen; fi)
