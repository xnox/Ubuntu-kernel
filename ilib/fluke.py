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

# This file provides definitions for fluke instruments
#
# It is designed to be extendable for other instruments, but has only been
# developed using this one model
#

#import sys
#import os
#import os.path
#import string
#import serial

from telnetlib          import Telnet
from time               import sleep

class Model():
    def __init__(self, modelname):
        self.modelDict = {'8846A':m8846A}
        if modelname not in self.modelDict:
            raise ValueError("Fluke instrument model %s not found in library" % modelname)
        self.modelname = modelname
        return

    def device(self):
        return self.modelDict[self.modelname]()


class m8846A():
    def __init__(self):
        """
        TODO
        """
        self.comm = None
        return

    def __del__(self):
        self.closeComms()
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
        self.comm = 'serial'
        raise NotImplementedError()
        return

    def initNet(self, host, port):
        self.comm = 'net'
        self.port = port
        tries = 5
        while tries:
            try:
                self.telnet = Telnet(host, port)
                #self.telnet.set_debuglevel(1000)
                return
            except Exception, e:
                tries = tries - 1
                if tries == 0:
                    raise
                print'failed with exception type %s' % `e`
                sleep(1)
        return

    def initGpib(self):
        # TODO
        self.comm = 'gpib'
        raise NotImplementedError()
        return

    def getIdent(self):
        """
        Return whatever version string that the instrument provides.
        The returned string is device-dependent
        """
        return self.device.getIdent();

    def send(self, output):
        # Send output to the device
        if self.comm is None:
            print 'connection not initialized for send'
        elif self.comm == 'serial':
            print 'Serial port not supported yet (send)'
        elif self.comm == 'net':
            self.telnet.write(output)
        elif self.comm == 'gpib':
            print 'GPIB not supported yet (send)'
        else:
            raise RuntimeError("unexpected comm type of <%s> found internally on send" % self.comm)
        return

    def closeComms(self):
        if self.comm is None:
            print 'connection not initialized for close'
        elif self.comm == 'serial':
            print 'Serial port not supported yet (close)'
        elif self.comm == 'net':
            self.telnet.close()
        elif self.comm == 'gpib':
            print 'GPIB not supported yet (close)'
        else:
            raise RuntimeError("unexpected comm type of <%s> found internally on send" % self.comm)
        self.comm = None
        return


    def receive(self):
        # receive from the device
        results = ''
        if self.comm is None:
            print 'connection not initialized for receive'
        elif self.comm == 'serial':
            print 'Serial port not supported yet (send)'
        elif self.comm == 'net':
            while 1:
                return self.telnet.read_until('\r\n')
        elif self.comm == 'gpib':
            print 'GPIB not supported yet (receive)'
        else:
            raise RuntimeError("unexpected comm type of <%s> found internally on receive" % self.comm)
        return results

    def getIdent(self):
        """
        Return the ID and version information
        """
        self.send('*idn?\r')
        id = self.receive()
        return id
