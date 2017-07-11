Robust Arduino Xively client supporting Maxim DS18S20 temperature sensor bus

0.4 Release

Copywrite 2017 Peter Milne

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, 
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
GNU General Public License for more details.

You should have received a copy of the GNU General Public License 
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Setup:
1. Open arduino-xively-client.ino in Arduino IDE

2. Configure Onewire pin & number of sensors - my setup has 2 sensors on pin 7:
```
#define ONEWIRE_BUS 7   // Pin 7 
#define NUM_DEVICES 2   // 2 sensors
```
3. Configure your Xively account settings and sample frequency in client-conf.h
```
#define UPDATE_INTERVAL 20000 // Every 20 seconds
#define APIKEY         "MySecretKey1234567890" //Your key
#define FEEDID         57506 // Your feed ID
#define USERAGENT      "TEST FEED" // Your project name
```
4. Upload sketch and test - you can see debugging info in serial monitor

The code should discover network connection via DHCP and sensor addresses automagically
on Onewire bus. This code is specifically for DS18S20 sensors only.

Tested on Arduino Duemilanove / Uno / Arduino IDE 1.6.7 / Xively API v2

Pin map showing connections to Ethernet(SPI), display(SPI), Onewire & LED,s:

Pin |Ethernet |TFT  | Other
----|---------|-----|------
13  |SCK      |SCK  |
12  |??       |     |
11  |MOSI     |MOSI |
10  |CS       |     |
09  |         |DC   |
08  |RESET    |     |
07  |         |     |ONEWIRE
06  |         |LC/CS|
05  |         |     |RED LED
04  |SD/CS    |     |
03  |         |RESET|
02  |         |     |GREEN LED
01  |         |     |
00  |         |     |





