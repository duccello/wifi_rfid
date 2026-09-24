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

Next steps.
1. Graham should:
go to Apps Scrip in extension of the fablab google document and apply the code found in AppsScript_doGet.gs
Then copy the link into the .ino file.

Best if the file just matches the column names as in the example file.

2. Hardware setup:
Install a screen to display the info (right now it shows on the Serial Monitor)
