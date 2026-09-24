# wifi_rfid

So far we:

0. Set up a google document that allows GET requests to fetch a Json file with our data.
https://docs.google.com/spreadsheets/d/1XQJovlWFOaad1_bkNsqT0VvluUiznTj9TqH80cHhJdY/edit?gid=0#gid=0

2. wired the Esp32 to the reader
Wiring (ESP32 <-> RC522, SPI)

RC522 Pin	    ESP32 Pin

SDA (SS)    	GPIO 5

SCK          	GPIO 18

MOSI	        GPIO 23

MISO        	GPIO 19

RST          	GPIO 22

3.3V        	3.3V

GND           GND

4. flashed the code in the esp32 that fetchs from the google document.

So, at the moment, it successfully reads the NFCs and shows on the Serial Monitor the information about the different members.
-----------------------------------------------------------------------------------------------------------

Next steps.

Graham:

1. open the Apps Scrip (in extensions) of the fablab's google document and apply the code found in AppsScript_doGet.gs.
2. Change the google document to match the column names:
   
  UID	  Name	Onboarding  	Laser  	Cnc  	Bambu

Code:

1. The .ino file should be updated to the one new App Script.
2. Adding a UI tbat communicates with an encoder/buttons and screen.

Hardware:

1. Install screen
2. Install buttons 

3D:

1. Case
2. Design for NFCs
