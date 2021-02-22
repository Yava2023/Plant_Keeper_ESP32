# Plant_Keeper_ESP32
Program for using an ESP32 developement board as a care taker for plants while also providing other information.

Installation:
- Download the "complete" folder and paste it anywhere
- needs the Arduino IDE and the ESP32 packages provided by Espressif

Configuration files needed for compiling:
- create WiFiConfig.h and TelegramConfig.h in a folder in the path of your Arduino libraries e.g. Configurations
- format files as given under "examples"

WiFi configuration:
Here you only need to enter the name of your network and the passcode.

Telegram Configuration:
In the Telegram configuration you need to enter the bot-Token which you receive when you create the Bot in Telegram (using the "BotFather") and your User-ID, which you can get by using the "IDBot".

Configuration in the file:
- In the programm the PINs for the soil humidity sensor, the DHT11 and the PIN controlling the relay of the pump have to be set correctly (by default they are set as follows: soil humidity sensor: PIN32, DHT11: PIN33, relay: PIN27).
- To set the correct time zone you can modify the variables given for the ntp server, you can also change to a local ntp server.
- The time for which the pump is activated each time the watering is activated by default is set to 5s. For smaller/larger plants this can be changed, it is stored in the variable "water_duration" and is given in ms.
- in the section "predefine user variables" all the variables later used for operation can changed. All these variables can also be changed trough the bot functionality. For longer uses it is however adviced to change them here, as they reset with every restart.

General programm structure:
The main goal of the programm is to water the plant it is attached to when it needs water. As the read-out from the soil moisture sensor is in arbitrary values it is necessary to let is run for a few days in "calibration mode" where it will send the values it reads every 10 minutes and from that it is possible to define a useful "dry_limit". Note that the value is higher when the soil gets dryer.

You can send commands to the ESP32 using the Telegram Bot. It only accept commands from the one person configured in the TelegramConfig. As the standard initialization command for a Bot in Telgram is /start this is here command to send, when you want to know more about the available commands (like /help in most programs).

The main loop always starts by asking for the time, which it gets from the local clock after one time synchronization in the start-up phase. Depending on the time the parts of the program are started. After finishing one loop the program holds for 10 seconds before starting again.
The ESP is configured to normally start in the "setup-mode" this means that it checks for new messages on every loop, so approximately every 10 seconds. The WiFi connection is not disconnected. To save energy it is possible to change the intervall on which it checks for new messages, the commands are provided when sending /start.

Additionally the ESP32 measures temperature and air humidity every minute and sends a warning if they are outside the defined ranges. These limits can be changed and also the warnings can be turned off.

If the warnings are turned off and the calibration mode is disabled only the status reports are send via the Bot to the User. They can be send every six hours or every 24 hours. It is possible to change the time at which the daily report is sent, but is is only possible to set it to 0, 6, 12 or 18. The reports also include average temperature and air humidity values.
