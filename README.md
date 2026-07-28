# TrailMarker
Coordinate guiding tool build for the [Heltec Wireless Tracker V2](https://heltec.org/project/wireless-tracker-v2/)

## Dependencies

[Follow this guide from Heltec to import the boards and libraries](https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/esp32-quick-start#there-are-three-methods-to-install-the-development-framework-choose-one-of-they)

## Information

Tab 1 (Main navigation)

<img width="3490" height="1682" alt="IMG_9951" src="https://github.com/user-attachments/assets/573d4498-fcc7-4449-a6d1-ee81cba215fe" />

Tab 2 (GNSS data)

<img width="3456" height="1662" alt="IMG_9949" src="https://github.com/user-attachments/assets/96989726-0a76-4484-9753-608803dd26f4" />

You can switch between tabs by long pressing the USER button for 1 second.

The text on the third row is your distance and bearings (145d = 145 degrees) to the coordinates, since the development board lacks a electronic compass, you must be facing north for it to accurate.

You can switch to the next coordinate in the array by pressing the USER button, double pressing will go to the previous one

Distance under 1000 meters will be displayed in meters and otherwise will be in miles.

The yellow number on the bottom right shows the coordinate you are currently being guided to in the array.

The sats number shows the amount of satellites you are currently connected to.

[Demo video](https://youtu.be/jib_LkI1jkI) (Not updated to latest version yet)
