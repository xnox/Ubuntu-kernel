#!/usr/bin/env python
#

#from sys                    import stdout, stderr
#from commands               import getstatusoutput
#from decimal                            import Decimal
#import json
#from os.path                import exists, getmtime
#from time                   import time
from email.mime.text         import MIMEText



# A class for sending email
class Email:
    """
    This class encapsulates sending email.
    """
    def __init__(self, smpt_server = None, smpt_user = None, smpt_password = None, port = 587):
        """
        Save the information needed to contact an smtp server
        """
        # This is pretty much only tested and working with the authentication required by the Canonical server
        # It probably needs more options added
        if (smtp_server is None) or (smtp_user is None) or (smtp_password is None):
            raise ValueError, "Must supply smpt server information"
        self.smtp_server = smtp_server
        self.smtp_user = smtp_user
        self.smtp_password = smtp_password
        # can be set for debugging
        self.verbose = False
        return

    def send(self, from_address, to_address, subject, body):
        """
        Send email. Uses the smtp server info already initialized.
        """
        if self.verbose:
            print 'send_email: from=<%s>, to=<%s>, subject=<%s>, body=<%s>' % (from_address, to_address, subject, body)

        msg = MIMEText(body)
        msg['Subject'] = subject
        msg['From'] = from_address
        msg['To'] = to_address
        # Send the message via our own SMTP server, but don't include the
        # envelope header.
        s = SMTP(self.smtp_server, 587)
        if self.verbose:
            s.set_debuglevel(1)
        s.ehlo()
        s.starttls()
        s.login(self.smtp_user.encode('UTF-8'),self.smtp_pass.encode('UTF-8'))
        s.sendmail(self.from_address, self.to_address, msg.as_string())
        s.quit()
        return

# A class for status messages
class Status:
    """
    This class encapsulates sending status updates to twitter, identi.ca, or status.net APIs.
    """
    def __init__(self, status_url = None, status_user = None, status_password = None):
        """
        Save the information needed to contact the server
        """
        if (status_url is None) or (status_user is None) or (status_password is None):
            raise ValueError, "Must supply status server information"
        self.status_url = status_url
        self.status_user = status_user
        self.status_password = status_password
        return

    def update(self, message):
        # TODO check length of message
        curl = 'curl -s -u %s:%s -d status="%s" %s' % (self.status_user,self.status_password,message,self.status_url)
        pipe = popen(curl, 'r')
        return

# vi:set ts=4 sw=4 expandtab:
