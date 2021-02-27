//Libraries
#include <DHTesp.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <WiFiConfig.h> //load Wifi name and password
#include <TelegramConfig.h> //load botToken and userID
#include <Wire.h>
#include <time.h>

//define PINs
const int DHTpin = 33;   //DHT11 sensor
const int MOIST_SENS = 32; //soil moisture sensor
const int RELAY = 27; //relay control pin for pump

//settings for ntp server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//configure libraries
DHTesp dht;
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

//define parameters for usage
const int water_duration = 1500; //time for which the pump is activated each time the plant is watered
const int day_start = 9; //setting day time (atm not really needed as light sensor not integrated)
const int day_end = 16;

// predefine user variables (can be changed by the user)
int request_mode = 5; // interval for request checking: 0 every min., 1 every 10 min., 2 every 6h, 3 every 24h, 4 every hour, 5 "set-up mode"
bool status_report_noon = 0; //mode for status updates: 0 every 6 hours, 1 every 24 hours
int daily_status_time = 12; //time for sending the daily status (only 0, 6, 12 and 18 possible)
bool calibration_mode = true; //mode in which the soil moisture values are send to the user
int dry_limit = 9000; //set dry limit, this can be determined before by the user using given script or afterwards using the given commands
bool temp_humi_warning = true; //sends warning if temperature or humidity are outside the defined range
int temp_upper_limit = 25; //setting optimal temperature window for plant
int temp_lower_limit = 18;
int hum_upper_limit = 80; //setting optimal humidity window for plant
int hum_lower_limit = 40;
bool daily_water_mode = true; // activated daily watering by default
int watering_time = 7; //time when the plant is watered every day

//initialize variables
//sensors
int moistvalue = 0; // value of humidity sensor
float deg = 20.0; // temperature value from sensor
int air_humi = 50; // humidity value from sensor
//averages
int av_6h_count = 0; //number of taken values for the 6h average
int av_day_count = 0; //number of taken values for the daily average
float deg_av_day = 0.0; // average value of temperatures over the last day
int deg_av_day_comp = 0; // average value of temperatures over the last day (in computing)
float deg_av_6h = 0.0; // average value of temperatures over the last 6h
int deg_av_6h_comp = 0; // average value of temperatures over the last 6h (in computing)
float air_humi_av_day = 0.0; // average value of air humidity over the last day
int air_humi_av_day_comp = 0; // average value of air humidity over the last day (in computing)
float air_humi_av_6h = 0.0; // average value of air humidity over the last 6h
int air_humi_av_6h_comp = 0; // average value of air humidity over the last 6h (in computing)
bool start_phase_6h = true; //don't get 6h averages if not running for 6 hours
bool start_phase_day = true; //don't get day averages if not running for a whole day
// time
struct tm timeinfo; // C structure including all time data
char timeStringBuff[40]; // char with atm time and date
char watering_timestamp[40] = "Not yet watered."; // last watering time
int t_check_min = 1; // remainder of minutes/10
int t_check_h; //remainder of hours/6
// Telegram
bool Connected = false ; //variable to check if currently connected to the WiFi
int numNewRequests = 0; //variable for number of requests
int new_value; // variable to temporarily save a value before changing
String text = ""; //variable for text of request
String chat_id = ""; //UserID from request sender
String from_name = ""; //name of request sender
String welcome = ""; //variable for welcome message
String status_report = ""; //variable for status report
String calibration_mode_string = "true"; //save the state of the calibration mode in a string to print
String temp_humi_warning_string = "true"; //save the state of the temp_humi_warning mode in a string to print
String daily_water_mode_string = "activated"; //save the state of the daily watering in a string to print
String start_message = ""; //message to be displayed send at start_up
String range_commands = ""; //message displaying all possible commands to change the variables for the temperature and air humidity range
//define smileys
const char* freeze = "🥶";
const char* sweat = "😅";
const char* water = "🚿";


//define functions

//request time from server
void get_time() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
}

//Wifi connector
void connectToWiFi() {
  Serial.print("Establishing connection to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("");
  Serial.println("Connected!");
  Connected = true;
}
//Wifi disconnector
void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("Disconnected from WiFi!");
  Connected = false;
}

//Bot functionality

//check for new messages
void check_requests() {
  //connect to WiFi
  if (!Connected) {
    connectToWiFi();
  }
  //checking for new requests
  numNewRequests = bot.getUpdates(bot.last_message_received + 1);
  //activate if new requests were received.
  while (numNewRequests >= 1) {
    Serial.println("Request received.");
    handleNewRequests(numNewRequests);
    numNewRequests = bot.getUpdates(bot.last_message_received + 1);
  }
}

//function for processing new requests inlcluding all possible interactions of the user
void handleNewRequests(int numNewRequests) {
  for (int i = 0; i < numNewRequests; i++) { //loops trough new tasks
    //checks if sender of request is authorized
    chat_id = String(bot.messages[i].chat_id);
    if (chat_id != userID) {
      bot.sendMessage(chat_id, "You are not authorized!", "");
      continue;
    }
    // save request
    text = bot.messages[i].text;
    Serial.println(text);
    //save sender and print
    from_name = bot.messages[i].from_name;
    Serial.print("from: " + from_name);
    //help message containing all possible functions
    if (text == "/start") {
      welcome = "Welcome, " + from_name + ".\n";
      welcome += "With the following commands you can get information: \n\n";
      welcome += "/status get a full status report \n";
      welcome += "/temp get the last read temperature\n";
      welcome += "/humi get the last read air humidity \n";
      welcome += "/soilhumi get current soil humidity\n";
      welcome += "change the interval between the checking for new meassage:\n";
      welcome += "  /setup_mode to check at every internal cycle\n";
      welcome += "  /check_1m to check every minute\n";
      welcome += "  /check_10m to check every 10 minutes\n";
      welcome += "  /check_1h to check every hour\n";
      welcome += "  /check_6h to check every 6 hours\n";
      welcome += "  /check_24h to check once a day\n";
      welcome += "/calibration_mode to change the setting for the calibration mode (turn on or off); in calibration mode the device sends soil moisture values when they are measured and informs if the value is too low\n";
      welcome += "/dry_limit show current value above which plant is watered\n";
      welcome += "/set_dry_limit_NEWVALUE set a new value above which the the plant is watered\n";
      welcome += "/water Manually activate the watering.\n";
      welcome += "/daily_watering activate/deactivate the daily watering at the set time\n";
      welcome += "/daily_watering_time show time at which the plan is watered once every day\n";
      welcome += "/set_daily_watering_NEWVALUE set the daily watering time (in hours)\n";
      welcome += "/temp_humi_warning Activate/deactivate the warnings for temperature and air humidity.\n";
      welcome += "/warning_range show commands to change the optimal temperature and humidity range";
      bot.sendMessage(userID, welcome, "");
    }
    //execute commands of the user
    else if (text == "/status") {
      comp_status_report();
      bot.sendMessage(userID, status_report, "");
    }
    else if (text == "/temp") {
      bot.sendMessage(userID, "Last measured temperature: " + String(deg) + "ºC", "");
    }
    else if (text == "/humi") {
      bot.sendMessage(userID, "Last measured air humidity: " + String(air_humi) + "%", "");
    }
    else if (text == "/soilhumi") {
      moistvalue = analogRead(MOIST_SENS);
      bot.sendMessage(userID, "Current soil humidity: " + String(moistvalue) + " (a.u.)", "");
    }
    else if (text == "/setup_mode") {
      bot.sendMessage(userID, "Set-up mode enabled.", "");
      request_mode = 5;
    }
    else if (text == "/check_1m") {
      bot.sendMessage(userID, "Set interval for checking for new messages to 1 minute.", "");
      request_mode = 0;
    }
    else if (text == "/check_10m") {
      bot.sendMessage(userID, "Set interval for checking for new messages to 10 minutes.", "");
      request_mode = 1;
    }
    else if (text == "/check_1h") {
      bot.sendMessage(userID, "Set interval for checking for new messages to 1 hour.", "");
      request_mode = 4;
    }
    else if (text == "/check_6h") {
      bot.sendMessage(userID, "Set interval for checking for new messages to 6 hours.", "");
      request_mode = 2;
    }
    else if (text == "/check_24h") {
      bot.sendMessage(userID, "Set interval for checking for new messages to once a day.", "");
      request_mode = 3;
    }
    else if (text == "/calibration_mode") {
      calibration_mode = !calibration_mode;
      if (calibration_mode) {
        calibration_mode_string = "true";
      }
      else if (!calibration_mode) {
        calibration_mode_string = "false";
      }
      bot.sendMessage(userID, "Calibration mode is now set to: " + calibration_mode_string, "");
    }
    else if (text == "/dry_limit") {
      bot.sendMessage(userID, "Currently set value above which the plant is watered: " + String(dry_limit), "");
    }
    else if (text.startsWith("/set_dry_limit_")) {
      new_value = extract_int_command(text, "/set_dry_limit_");
      if (new_value != 9999) {
        dry_limit = new_value;
        bot.sendMessage(userID, "New value above which the plant is watered: " + String(dry_limit), "");
      }
    }
    else if (text == "/daily_watering") {
      daily_water_mode = !daily_water_mode;
      if (daily_water_mode) {
        daily_water_mode_string = "activated";
      }
      else if (!daily_water_mode) {
        daily_water_mode_string = "deactivated";
      }
      bot.sendMessage(userID, "Daily watering at the set time is now " + daily_water_mode_string + ".\n The plant will be watered every day at: " + String(watering_time) + ":00.", "");
    }
    else if (text == "/daily_watering_time") {
      bot.sendMessage(userID, "Currently set time when the plant is watered: " + String(watering_time) + ":00", "");
    }
    else if (text.startsWith("/set_daily_watering_")) {
      new_value = extract_int_command(text, "/set_daily_watering_");
      if (new_value != 9999 && new_value < 25) {
        watering_time = new_value;
        bot.sendMessage(userID, "New time at which the plant is watered daily: " + String(watering_time) + ":00", "");
      }
    }
    else if (text == "/water") {
      bot.sendMessage(userID, "Plant is watered." + String(water), "");
      water_plant();
    }
    else if (text == "/temp_humi_warning") {
      temp_humi_warning = !temp_humi_warning;
      if (temp_humi_warning) {
        temp_humi_warning_string = "activated";
      }
      else if (!temp_humi_warning) {
        temp_humi_warning_string = "deactivated";
      }
      bot.sendMessage(userID, "The temperature and air humidity warnings are now " + temp_humi_warning_string + ".", "");
    }
    else if (text == "/warning_range") {
      compile_range_message();
      bot.sendMessage(userID, range_commands, "");
    }
    else if (text.startsWith("/set_temp_low_limit_")) {
      new_value = extract_int_command(text, "/set_temp_low_limit_");
      if (new_value != 9999) {
        temp_lower_limit = new_value;
        bot.sendMessage(userID, "New lower limit of the optimal temperature range for the plant: " + String(temp_lower_limit) + "°C", "");
      }
    }
    else if (text.startsWith("/set_temp_high_limit_")) {
      new_value = extract_int_command(text, "/set_temp_high_limit_");
      if (new_value != 9999) {
        temp_upper_limit = new_value;
        bot.sendMessage(userID, "New upper limit of the optimal temperature range for the plant: " + String(temp_upper_limit) + "°C", "");
      }
    }
    else if (text.startsWith("/set_humi_low_limit_")) {
      new_value = extract_int_command(text, "/set_humi_low_limit_");
      if (new_value != 9999) {
        hum_lower_limit = new_value;
        bot.sendMessage(userID, "New lower limit of the optimal air humidity range for the plant: " + String(hum_lower_limit) + "%", "");
      }
    }
    else if (text.startsWith("/set_humi_high_limit_")) {
      new_value = extract_int_command(text, "/set_humi_high_limit_");
      if (new_value != 9999) {
        hum_upper_limit = new_value;
        bot.sendMessage(userID, "New upper limit of the optimal air humidity range for the plant: " + String(hum_upper_limit) + "%", "");
      }
    }
    else {
      bot.sendMessage(userID, "Please send /start for more information on the available commands.", "");
    }
  }
}

//compile message for range changing
void compile_range_message() {
  range_commands += "At the moment the following values are set as the optimal range for temperature and air humidity of the plant:\n";
  range_commands += "temperature range: ";
  range_commands += String(temp_lower_limit);
  range_commands += "°C - ";
  range_commands += String(temp_upper_limit);
  range_commands += "°C\n";
  range_commands += "air humidity range: ";
  range_commands += String(hum_lower_limit);
  range_commands += "% - ";
  range_commands += String(hum_upper_limit);
  range_commands += "%\n\n";
  range_commands += "To change the values send the following commands with the new values:\n";
  range_commands += "/set_temp_low_limit_NEWVALUE set a new minimum temperature for the plant.\n";
  range_commands += "/set_temp_high_limit_NEWVALUE set a new maximum temperature for the plant.\n";
  range_commands += "/set_humi_low_limit_NEWVALUE set a new minimum air humidity for the plant.\n";
  range_commands += "/set_humi_high_limit_NEWVALUE set a new maximum air humidity for the plant.\n";
}

// extracts integer from variables
int extract_int_command(String input_string, String input_command) {
  int value_int = 9999;
  input_string.remove(0, input_command.length());
  input_string.trim();
  if (!input_string.toInt()) {
    bot.sendMessage(userID, "Please give an integer value as input.", "");
  }
  else {
    value_int = input_string.toInt();
  }
  return value_int;
}

//compile status report
void comp_status_report() {
  strftime(timeStringBuff, sizeof(timeStringBuff), "%B %d %Y %H:%M:%S", &timeinfo);
  status_report = "Status report compiled on: " + String(timeStringBuff) + "\n";
  status_report += "Last measured temperature: " + String(deg) + "ºC\n";
  status_report += "Last measured air humidity: " + String(air_humi) + "%\n";
  status_report += "Last measured soil humidity: " + String(moistvalue) + " (a.u.)\n";
  status_report += "Last time watered: " + String(watering_timestamp) + "\n";
  if (!start_phase_6h && air_humi_av_6h != 0) {
    status_report += "Average temperature over the last six hours (recorded between 0h - 6h, 6h - 12h, 12h - 18h or 18h - 24h): " + String(deg_av_6h) + "°C\n";
    status_report += "Average air humidity over the last six hours (recorded between 0h - 6h, 6h - 12h, 12h - 18h or 18h - 24h): " + String(air_humi_av_6h) + "%\n";
  }
  else {
    status_report += "The system has not been running for a 6 hour cycle and therefore no average values are available.\n";
  }
  if (!start_phase_day && air_humi_av_day != 0) {
    status_report += "Average temperature over the last day (recorded from " + String(daily_status_time) + "h to " + String(daily_status_time) + "h): " + String(deg_av_day) + "ºC\n";
    status_report += "Average air humidity over the last day (recorded from " + String(daily_status_time) + "h to " + String(daily_status_time) + "h): " + String(air_humi_av_day) + "%\n";
  }
  else {
    status_report += "The system has not been running for a 24 hour cycle and therefore no average values are available.\n";
  }
}


//Send data helpful for debugging in start phase and compile start message
void print_initialized_values() {
  Serial.println("Plant Keeper was started with the following parameters:");
  start_message += "Plant Keeper was started with the following parameters:\n";
  if (request_mode == 5) {
    Serial.println("Started in set-up mode.");
    start_message += "Set-up mode is enabled. The device will check for new message as often as possible. Please change to another check interval to save energy if not needed.\n";
  }
  else {
    Serial.print("Telegram update check interval: ");
    start_message += "Telegram update check interval: ";
    switch (request_mode) {
      case 0:
        Serial.println("every minute");
        start_message += "every minute\n";
        break;
      case 1:
        Serial.println("every 10 minutes");
        start_message += "every 10 minutes\n";
        break;
      case 2:
        Serial.println("every six hours");
        start_message += "every six hours\n";
        break;
      case 3:
        Serial.println("every 24 hours");
        start_message += "every 24 hours\n";
        break;
      case 4:
        Serial.println("every hour");
        start_message += "every hour\n";
        break;
    }
  }
  Serial.print("Interval for sending status reports: ");
  start_message += "Interval for sending status reports: ";
  if (status_report_noon) {
    Serial.print("once a day at ");
    start_message += "once a day at " + String(daily_status_time) + "h\n";
    Serial.println(daily_status_time);
  }
  else {
    Serial.println("every six hours at 0:00, 6:00, 12:00 and 18:00");
    start_message += "every six hours at 0:00, 6:00, 12:00 and 18:00\n";
  }
  Serial.print("Optimal temperature window: ");
  start_message += "Optimal temperature window: " + String(temp_lower_limit) + "°C - " + String(temp_upper_limit) + "°C\n";
  Serial.print(temp_lower_limit);
  Serial.print("°C - ");
  Serial.print(temp_upper_limit);
  Serial.println("°C");
  Serial.print("Optimal air humidity window: ");
  start_message += "Optimal air humidity window: " + String(hum_lower_limit) + "% - " + String(hum_upper_limit) + "%\n";
  Serial.print(hum_lower_limit);
  Serial.print("% - ");
  Serial.print(hum_upper_limit);
  Serial.println("%");
  Serial.print("Daytime from ");
  // start_message += "Daytime from " + String(day_start) + "h to " + String(day_end) + "h\n"; //not given in Telegram as not used at the moment
  Serial.print(day_start);
  Serial.print("h to ");
  Serial.print(day_end);
  Serial.println("h");
  start_message += "Dry limit set at: " + String(dry_limit) + ". Please adjust this value to your personal needs.";
  Serial.print("Dry limit set at: ");
  Serial.print(dry_limit);
  Serial.println(". Please adjust this value to your personal needs.\n");
  Serial.println("Calibration mode: " + calibration_mode_string);
  if(daily_water_mode){
    start_message += "The plant is watered daily at " + String(watering_time) + ":00.";
    Serial.print("The plant is watered daily at ");
    Serial.print(watering_time);
    Serial.println(":00.");
  }
  else {
    start_message += "The daily plant watering is currently deactivated.";
    Serial.println("The daily plant watering is currently deactivated.");
  }
  start_message += "Calibration mode: " + calibration_mode_string + "\n\n";
  start_message += "For more information on the available commands send /start.";
}

//read DHT data and check if values inside the required range
void dhtread() {
  //read DHT 11 data
  deg = dht.getTemperature();
  air_humi = dht.getHumidity();

  //display information from DHT 11
  Serial.print("Temperature: ");
  Serial.print(deg);
  Serial.println("°C");
  Serial.print("Humidity: ");
  Serial.print(air_humi);
  Serial.println("%");

  if (temp_humi_warning) {
    //warn if temperature is outside the defined range
    if (deg < temp_lower_limit) {
      if (!Connected) {
        connectToWiFi(); //connect to WiFi
      }
      bot.sendMessage(userID, "Temperature warning: The temperature is below the defined range!" + String(freeze) + "\nCurrent temperature: " + String(deg) + "°C", "");
    }
    else if (deg > temp_upper_limit) {
      if (!Connected) {
        connectToWiFi(); //connect to WiFi
      }
      bot.sendMessage(userID, "Temperature warning: The temperature is above the defined range!" + String(sweat) + "\nCurrent temperature: " + String(deg) + "°C", "");
    }

    //warn if air humidity is outside the defined range
    if (air_humi < hum_lower_limit) {
      if (!Connected) {
        connectToWiFi(); //connect to WiFi
      }
      bot.sendMessage(userID, "Air humidity warning: The humidity of the air is below the defined range! \nCurrent air humidity: " + String(air_humi) + "%", "");
    }
    else if (air_humi > hum_upper_limit) {
      if (!Connected) {
        connectToWiFi(); //connect to WiFi
      }
      bot.sendMessage(userID, "Air humidity warning: The humidity of the air is above the defined range! \nCurrent air humidity: " + String(air_humi) + "%", "");
    }
  }
}

//plant watering
void water_plant() {
  //save time of watering
  strftime(watering_timestamp, sizeof(watering_timestamp), "%B %d %Y %H:%M:%S", &timeinfo);
  //activate pump
  digitalWrite(RELAY, LOW);
  Serial.println("Relay on.");
  delay(water_duration);
  digitalWrite(RELAY, HIGH);
  Serial.println("Relay off.");
}


///////////////////////// main programm part //////////////////////

void setup() {
  //initialize Relay
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  //begin monitor
  Serial.begin(115200);
  // configure dht
  dht.setup(DHTpin, DHTesp::DHT11); //for DHT11 Connect DHT sensor to defined pin
  //setup Serverconnection setting
  client.setInsecure();
  //connect to the WiFi
  connectToWiFi();
  //init and get the current time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  get_time();
  //print the values with which the system is initialized to the Serial monitor and send bot start message
  print_initialized_values();
  bot.sendMessage(userID, start_message, "");
  //initialy read moisture value
  moistvalue = analogRead(MOIST_SENS);
  Serial.print("soil moisture value: ");
  Serial.println(moistvalue);

  delay(5000);
}

void loop() {
  get_time();
  //print output of clock
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //checking for new messages if checking is set to "set-up mode"
  if (request_mode == 5) {
    check_requests();
  }
  //only activate code once a minute
  if (timeinfo.tm_sec >= 30 && timeinfo.tm_sec < 40)
  {
    //print message that code is activated
    Serial.println("Seconds between 30 and 39.");
    //read DHT 11 data
    dhtread();

    //checking for new messages if checking is set to "every minute"
    if (request_mode == 0) {
      check_requests();
    }

    // only run every 10 minutes
    t_check_min = timeinfo.tm_min % 10;
    if (t_check_min == 0)
    {
      //add to averages
      deg_av_6h_comp += int(deg * 100.0);
      air_humi_av_6h_comp += air_humi;
      av_6h_count += 1;
      deg_av_day_comp += int(deg * 100.0);
      air_humi_av_day_comp += air_humi;
      av_day_count += 1;

      //checking for new messages if checking is set to "every 10 minutes"
      if (request_mode == 1) {
        check_requests();
      }
      //read moisture value
      moistvalue = analogRead(MOIST_SENS);
      Serial.print("soil moisture value: ");
      Serial.println(moistvalue);
      //to simplify calibration
      if (calibration_mode) {
        if (!Connected) {
          connectToWiFi();
        }
        bot.sendMessage(userID, "Soil humidity: " + String(moistvalue), ""); //send moistvalue to user
      }
      //check if soil is too dry
      if (moistvalue >= dry_limit)
      {
        Serial.println("Soil too dry! Watering now." + String(water));
        if (calibration_mode) {
          if (!Connected) {
            connectToWiFi();
          }
          bot.sendMessage(userID, "Soil too dry! Plant is watered."  + String(water), ""); // inform user, that watering case was activated
        }
        //activate watering function
        water_plant();
      }
      else if (timeinfo.tm_hour == watering_time && timeinfo.tm_min < 10 && (moistvalue + 300) > dry_limit) { //only activate daily waitering if soil is close enough to being dry
        Serial.println("Daily watering time.");
        if (calibration_mode) {
          if (!Connected) {
            connectToWiFi();
          }
          bot.sendMessage(userID, "Daily watering time.", ""); // inform user, that watering case was activated
        }
        //activate watering function
        water_plant();
      }

      // activate every 6 hours (and only once in this hour)
      t_check_h = timeinfo.tm_hour % 6;
      if (t_check_h == 0 && timeinfo.tm_min < 10) {
        // write 6h average values from comp variables into those for report
        if (start_phase_6h) {
          //if system in start phase, deactivate 6h start phase
          start_phase_6h = false;
          deg_av_6h_comp = 0;
          air_humi_av_6h_comp = 0;
          av_6h_count = 0;
        }
        else {
          deg_av_6h = float(deg_av_6h_comp) / (av_6h_count*100);
          air_humi_av_6h = float(air_humi_av_6h_comp) / av_6h_count;
          deg_av_6h_comp = 0;
          air_humi_av_6h_comp = 0;
          av_6h_count = 0;
        }
        //checking for new messages if checking is set to "every 6 hours"
        if (request_mode == 2) {
          check_requests();
        }

        //checking for new messages if checking is set to "once a day"
        else if (request_mode == 3 && timeinfo.tm_hour == daily_status_time) {
          check_requests();
        }

        //write daily averages
        if (timeinfo.tm_hour == daily_status_time) {
          if (start_phase_day) {
            start_phase_day = false;
            deg_av_day_comp = 0;
            air_humi_av_day_comp = 0;
            av_day_count = 0;
          }
          else {
            deg_av_day = float(deg_av_day_comp) / av_day_count;
            air_humi_av_day = float(air_humi_av_day_comp) / (av_day_count*100);
            deg_av_day_comp = 0;
            air_humi_av_day_comp = 0;
            av_day_count = 0;
          }
        }

        //send status reports depending on mode every 6h or 24h
        if (!status_report_noon) {
          if (!Connected) {
            connectToWiFi();
          }
          comp_status_report();
          bot.sendMessage(userID, status_report, "");
          get_time(); //sync time every six hours
        }
        else if (status_report_noon && timeinfo.tm_hour == daily_status_time)  {
          comp_status_report();
          bot.sendMessage(userID, status_report, "");
          get_time(); //sync time once a day
        }
      }
    }
    // disconnect from WiFi if connected
    if (Connected && !request_mode == 5 && !request_mode == 0) {
      disconnectWiFi();
    }
  }
  delay(10000);
}
