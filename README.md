# TrailMarker
Coordinate guiding tool build for the [Heltec Wireless Tracker V2](https://heltec.org/project/wireless-tracker-v2/)

## Dependencies

[Follow this guide from Heltec to import the boards and libraries](https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/esp32-quick-start#there-are-three-methods-to-install-the-development-framework-choose-one-of-they)

## Information

<img width="3432" height="1660" alt="image" src="https://github.com/user-attachments/assets/9fefaa50-0cd6-4eec-8646-fa71baa4256b" />

The text on the third row is your distance and bearings (145d = 145 degrees) to the coordinates, since the development board lacks a electronic compass, you must be facing north for it to accurate.

You can switch to the next coordinate in the array by pressing the USER button, double pressing will go to the previous one (not available in -noprev version)

Distance under 1000 meters will be displayed in meters and otherwise will be in miles.

The red number on the bottom right shows the coordinate you are currently being guided to in the array.

The sats number shows the amount of satellites you are currently connected to.

[Demo video](https://youtu.be/jib_LkI1jkI)
