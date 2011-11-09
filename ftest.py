#!/usr/bin/env python
# Copyright 2011  Steve Conklin 
# sconklin at canonical dot com
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# This file provides test code for development
#

#import sys
#import os
#import os.path
#import string
#from array import *
#import serial

import time

from time import sleep, time

from instrument import Instrument

meter = Instrument('fluke', '8846A')

url = '172.31.0.157'
port = '3490'

meter.initNet(url, port)

id = meter.getIdent()
print 'id = ', id.strip()

md = meter.device

cd = md.getCalDate()
print 'Calibration Date = ', cd.strip()

md.clearStatus()

d = md.getDate()
print 'meter date = ', d.strip()
t = md.getTime()
print 'meter time = ', t.strip()

#md.setDateTime()

# Doesn't work
#md.selfTest()

#md.displayOff()
#sleep(1)
#md.displayOn()

# Doesn't work
#md.setPanelMsg("Testing")
#sleep(1)
#md.clearPanelMsg()

st = md.checkStatus()
if st:
    print 'BAD STATUS = %d' % st

md.setRemoteMode()

# =======================================
# Measuring Current with a single command
# =======================================

# For Measuring Current -
# Range of MAX selects the 10A input
# Range of DEF selects the 400 mA input
if False:
    st = md.measureCurrent('DC', 'Max')
    print 'Measured Current = ', st

# The measureCurrent method can make a maximum of around 3.5 requests/second
# according to this measurement
if False:
    tstart = time()
    for i in range(100):
        st = md.measureCurrent('DC', 'Max')
    tend = time()
    et = tend - tstart
    print "Elapsed time = ", et
    print "requests/second = ", (100.0/et)

# =======================================
# Measuring Voltage with a single command
# =======================================

# For Measuring Voltage -
# Some measurements take longer than 1 second
if False:
    st = md.measureVoltage('DC', 'Max')
    print 'Measured DC Voltage = ', st.strip()
    st = md.measureVoltage('AC', 'Def')
    print 'Measured AC Voltage = ', st.strip()

# =======================================
# Measuring Voltage with set and read
# =======================================

if False:
    print 'sending set'
    md.setVoltage('DC', 'Max')
    print 'sending read'
    st = md.read()
    print 'Measured DC Voltage = ', st.strip()

if False:
    print 'sending set'
    md.setVoltage('AC', 'Max')
    print 'sending read'
    st = md.read()
    print 'Measured AC Voltage = ', st.strip()

# ================================================
# speed test - Measuring Voltage with set and read
# ================================================
# for DC voltage, rate is around 4/second
# For AC Voltage, rate is around 0.4/second
if True:
    print 'sending set DC'
    md.setVoltage('DC', 'Max')
    sleep(1.0)
    print 'sending reads'
    tstart = time()
    for i in range(100):
        st = md.read()
    tend = time()
    et = tend - tstart
    print "Elapsed time = ", et
    print "requests/second = ", (100.0/et)

    print 'sending set AC'
    md.setVoltage('AC', 'Max')
    sleep(1.0)
    print 'sending reads'
    tstart = time()
    for i in range(100):
        st = md.read()
    tend = time()
    et = tend - tstart
    print "Elapsed time = ", et
    print "requests/second = ", (100.0/et)

#os = md.getStatusDump()
#print os

md.setLocalMode()
