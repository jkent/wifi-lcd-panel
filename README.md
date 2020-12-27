# wifi-lcd-panel

This thing is an ESP8266 connected to a DSR-4400 satelite receiver front panel
LCD module.  It has useful dialog code for a 40x2 LCD display, otherwise it is
probably useless to you.  The current configuration shows a clock that sync's
it's time via SNTP.

To build you need to use [ESP8266_RTOS_SDK](https://github.com/espressif/ESP8266_RTOS_SDK/).

```
. ~/usr/ESP8266_RTOS_SDK/export.sh
idf.py build
```
