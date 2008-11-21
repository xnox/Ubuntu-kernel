Target: communicate with the AP by the rtl8187 card using the 
        modified ieee80211 stack wich support WMM.
Within ieee80211_xmit function
1. because the packet to be sent is from the station, 
   so 3 address space is enough.

  NOw card can get the manage frame from the AP, but can not 
  get the AP address.
