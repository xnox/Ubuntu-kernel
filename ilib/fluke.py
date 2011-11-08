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
from time               import sleep, localtime, strftime

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
                return self.telnet.read_until('\r\n', 1)
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

    def getCalDate(self):
        """
        Return the ID and version information
        """
        self.send('CAL:DATE?\r')
        cd = self.receive()
        return cd

    def reset(self):
        """
        Reset the instrument to power-on state
        """
        self.send('*rst\r')

    def selfTest(self):
        """
        Perform instrument self test.
        Returns True if pass, False if fail
        """
        self.send('*tst\r')
        st = self.receive().strip
        if st == '0':
            return False
        elif st == '1':
            return True
        else:
            raise RuntimeError("received unexpected return value of %s from self test" % st)

    def checkStatus(self):
        """
        Check to see if anything's set in the status register. Ultimately,
        probably need to drill down to get details about what happened, but
        let's see what we need first
        """
        rs = ''
        self.send('*stb?\r')
        stb = self.receive()
        chkbits = 8 + 16 + 32
        return int(stb) & chkbits

    def clearStatus(self):
        """
        Clear the status byte summary and all event registers
        """
        self.send('*cls\r')

    def __boolean(self, value, bit):
        if int(value) & bit:
            rv = '1'
        else:
            rv = '0'
        return rv

    def getStatusDump(self):
        """
        Return the ID and version information
        """
        rs = ''
        self.send('*stb?\r')
        stb = self.receive()
        rs = rs + 'Status Byte (STB):\n'
        rs = rs + '    Not Used            0: %s\n' % self.__boolean(stb, 1)
        rs = rs + '    Not Used            1: %s\n' % self.__boolean(stb, 2)
        rs = rs + '    Not Used            2: %s\n' % self.__boolean(stb, 4)
        rs = rs + '    Questionable Data   3: %s\n' % self.__boolean(stb, 8)
        rs = rs + '    Message Available   4: %s\n' % self.__boolean(stb, 16)
        rs = rs + '    Standard Event      5: %s\n' % self.__boolean(stb, 32)
        rs = rs + '    Request Service     6: %s\n' % self.__boolean(stb, 64)
        rs = rs + '    Not Used            7: %s\n' % self.__boolean(stb, 128)

        self.send('*sre?\r')
        stb = self.receive()
        rs = rs + 'Status Byte Enable (SRE):\n'
        rs = rs + '    Not Used            0: %s\n' % self.__boolean(stb, 1)
        rs = rs + '    Not Used            1: %s\n' % self.__boolean(stb, 2)
        rs = rs + '    Not Used            2: %s\n' % self.__boolean(stb, 4)
        rs = rs + '    Questionable Data   3: %s\n' % self.__boolean(stb, 8)
        rs = rs + '    Message Available   4: %s\n' % self.__boolean(stb, 16)
        rs = rs + '    Standard Event      5: %s\n' % self.__boolean(stb, 32)
        rs = rs + '    Request Service     6: %s\n' % self.__boolean(stb, 64)
        rs = rs + '    Not Used            7: %s\n' % self.__boolean(stb, 128)

        self.send('*esr?\r')
        stb = self.receive()
        rs = rs + 'Standard Event Register (ESR):\n'
        rs = rs + '    Operation Complete  0: %s\n' % self.__boolean(stb, 1)
        rs = rs + '    Not Used            1: %s\n' % self.__boolean(stb, 2)
        rs = rs + '    Query Error         2: %s\n' % self.__boolean(stb, 4)
        rs = rs + '    Device Error        3: %s\n' % self.__boolean(stb, 8)
        rs = rs + '    Execution Error     4: %s\n' % self.__boolean(stb, 16)
        rs = rs + '    Command Error       5: %s\n' % self.__boolean(stb, 32)
        rs = rs + '    Not Used            6: %s\n' % self.__boolean(stb, 64)
        rs = rs + '    Power On            7: %s\n' % self.__boolean(stb, 128)

        self.send('*ese?\r')
        stb = self.receive()
        rs = rs + 'Standard Event Register Enable (ESE):\n'
        rs = rs + '    Operation Complete  0: %s\n' % self.__boolean(stb, 1)
        rs = rs + '    Not Used            1: %s\n' % self.__boolean(stb, 2)
        rs = rs + '    Query Error         2: %s\n' % self.__boolean(stb, 4)
        rs = rs + '    Device Error        3: %s\n' % self.__boolean(stb, 8)
        rs = rs + '    Execution Error     4: %s\n' % self.__boolean(stb, 16)
        rs = rs + '    Command Error       5: %s\n' % self.__boolean(stb, 32)
        rs = rs + '    Not Used            6: %s\n' % self.__boolean(stb, 64)
        rs = rs + '    Power On            7: %s\n' % self.__boolean(stb, 128)

        self.send('STAT:QUES:EVEN?\r')
        stb = self.receive()
        rs = rs + 'Questionable Data Event Register:\n'
        rs = rs + '    Voltage Overload    0: %s\n' % self.__boolean(stb, 1)
        rs = rs + '    Current Overload    1: %s\n' % self.__boolean(stb, 2)
        rs = rs + '    Not Used            2: %s\n' % self.__boolean(stb, 4)
        rs = rs + '    Not Used            3: %s\n' % self.__boolean(stb, 8)
        rs = rs + '    Not Used            4: %s\n' % self.__boolean(stb, 16)
        rs = rs + '    Not Used            5: %s\n' % self.__boolean(stb, 32)
        rs = rs + '    Not Used            6: %s\n' % self.__boolean(stb, 64)
        rs = rs + '    Not Used            7: %s\n' % self.__boolean(stb, 128)
        rs = rs + '    Not Used            8: %s\n' % self.__boolean(stb, 256)
        rs = rs + '    Ohms Overload       9: %s\n' % self.__boolean(stb, 512)
        rs = rs + '    Not Used           10: %s\n' % self.__boolean(stb, 256)
        rs = rs + '    Limit Test Fail LO 11: %s\n' % self.__boolean(stb, 512)
        rs = rs + '    Limit Test Fail HI 12: %s\n' % self.__boolean(stb, 1024)
        rs = rs + '    Remote Mode        13: %s\n' % self.__boolean(stb, 2048)
        rs = rs + '    Not Used           14: %s\n' % self.__boolean(stb, 4096)
        rs = rs + '    Not Used           14: %s\n' % self.__boolean(stb, 8192)

        self.send('STAT:QUES:ENAB?\r')
        stb = self.receive()

        return rs

    def setRemoteMode(self):
        """
        Place the meter into remote mode. Front Panel keys are disabled
        except for the 'local' button.
        """
        self.send('SYST:REM\r')
        return

    def setLocalMode(self):
        """
        Place the meter into local mode. Front panel keys enabled.
        """
        self.send('SYST:LOC\r')
        return

    def setPanelMsg(self, message):
        """
        print a message on the meter front panel
        """
        cmd = 'DISP:TEXT "%s"\r' % message.strip()
        print 'cmd = ', cmd
        self.send(cmd)
        return

    def clearPanelMsg(self):
        """
        clears the message on the meter front panel
        """
        self.send('DISP:TEXT:CLE\r')
        return

    def getDate(self):
        """
        Return meter date
        """
        self.send('SYST:DATE?\r')
        d = self.receive()
        return d

    def getTime(self):
        """
        Return meter time
        """
        self.send('SYST:TIME?\r')
        d = self.receive()
        return d

    def setDateTime(self):
        """
        Sets meter date and time to local system date and time
        """
        lt = localtime()
        datestr = strftime('%m/%d/%Y', lt) # mm/dd/yyyy
        timestr = strftime('%H:%M:%S', lt) # hh:mm:ss
        print 'datestr = ', datestr
        print 'timestr = ', timestr
        self.send('SYST:DATE %s\r' % datestr)
        self.send('SYST:TIME %s\r' % timestr)
        
        return


    def displayOn(self):
        """
        Turns the meter front panel on
        """
        self.send('DISP ON\r')
        return
        
    def displayOff(self):
        """
        Turns the meter front panel off
        """
        self.send('DISP OFF\r')
        return

# MEAS? - quick and simple
# CONF - to change one or two parameters
# READ? - take a measurement the next time a trigger happens
#

    def measureVoltage(self, macdc, mrange = 'Def', mresolution = 'Min'):
        # TODO not finished or tested
        """
        Set to voltage measurement mode
        macdc = 'AC' or 'DC'
        mrange = ['Def'|'Min'|'Max']
        mresolution = ['Def'|'Min'|'Max']
        """
        macdc = macdc.upper()
        if macdc not in ['AC', 'DC']:
            raise ValueError("Invalid Voltage measurement type")
        mrange = mrange.upper()
        if mrange not in ['DEF', 'MIN', 'MAX']:
            raise valueError("Inavlid range for voltage measurement")
        mresolution = mresolution.upper()
        if mresolution not in ['DEF', 'MIN', 'MAX']:
            raise valueError("Inavlid resolution for voltage measurement")

        # Send a MEAS? Command
        # TODO - What does "SCAL" do?
        # MEAS:CURR:DC?DEF,MIN
        command = 'MEAS:VOLT:%s?%s,%s\r' % (macdc, mrange, mresolution)
        self.send(command)
        stb = self.receive()
        return stb

    def measureCurrent(self, macdc, mrange = 'Def', mresolution = 'Min'):
        """
        Set to voltage measurement mode - Max rate is around 3.5 measurements/second
        macdc = 'AC' or 'DC'
        mrange = ['Def'|'Min'|'Max']
        mresolution = ['Def'|'Min'|'Max']
        """
        macdc = macdc.upper()
        if macdc not in ['AC', 'DC']:
            raise ValueError("Invalid Voltage measurement type")
        mrange = mrange.upper()
        if mrange not in ['DEF', 'MIN', 'MAX']:
            raise valueError("Inavlid range for voltage measurement")
        mresolution = mresolution.upper()
        if mresolution not in ['DEF', 'MIN', 'MAX']:
            raise valueError("Inavlid resolution for voltage measurement")

        command = 'MEAS:CURR:%s? %s,%s\r' % (macdc, mrange, mresolution)

        self.send(command)

        stb = self.receive()
        return stb

    def setVoltage(self, mtype, mrange = 'Auto', mresolution = 'MAX', mbandwidth = None):
        """
        Set to voltage measurement mode
        mtype = 'AC' or 'DC'
        mrange = 'Auto', 'Min', 'Max', or a value
        mresolution = 'Min', 'Max', or a value
        mbandwidth = 3, 20, or 200
        """
        mtype = mtype.upper()
        if mtype not in ['AC', 'DC']:
            raise ValueError("Invalid Voltage measurement type")
        mrange = mrange.upper()
        if mrange not in ['', '']:
            raise valueError("Inavlid range for voltage measurement")
        mresolution = mresolution.upper()
        mbandwidth = mbandwidth.upper()
        # Send a CONFigure Command
        
        
        return

    def setCurrent(self):
        """
        Set to current measurement mode
        """
        return
