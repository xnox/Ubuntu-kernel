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

# This software provides an interface to the Fluke 8846A precision multimeter
#
# It is designed to be extendable for other instruments, but has only been
# developed using this one model, and only tested using the ethernet interface
#
# For serial operation, you will need the (very nice) pySerial module, found here:
# http://pyserial.wiki.sourceforge.net/pySerial

import sys
#import os
#import os.path
#import string
#from array import *
#import serial

class Instrument():
    def __init__(self, manufacturer, model):
        """
        We establish the instrument interface by providing the
        instrument type and specifying which interface to use
        interface = ["serial" | "net" | "gpib"]
        """
        try:
            mpath = 'ilib.'+manufacturer
            __import__(mpath)
            manuf = sys.modules[mpath]
        except:
            raise #ValueError("Can't find a module for manufacturer %s" % manufacturer)

        model = manuf.Model(model)
        self.device = model.device()

        return

    def __del__(self):
        return

    def initSerial(self, device, baud, parity, data_bits, stop_bits, flow_control):
        """
        Initialize the serial port - required information is:
        device:       Path to the device
        baud:         Serial baud rate
        parity:       Serial parity
        data_bits:    Number of data bits
        stop_bits:    Number of stop bits
        flow_control: ["none" | "xon" | "rts"]
        """
        # TODO
        self.device.comm = 'serial'
        self.device.initSerial( device, baud, parity, data_bits, stop_bits, flow_control)
        return

    def initNet(self, host, port = '23'):
        # TODO
        self.device.comm = 'net'
        self.device.initNet(host, port)
        return

    def initGpib(self):
        # TODO
        self.device.comm = 'gpib'
        self.device.initGpib()
        return

    def getIdent(self):
        """
        Return whatever version string that the instrument provides.
        The returned string is device-dependent
        """
        return self.device.getIdent();
