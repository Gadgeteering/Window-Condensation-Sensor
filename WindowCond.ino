/**
 * Window Condensatio SensorFirmware
 * Copyright (c) 2020 Gadgeteering 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <Adafruit_MLX90614.h>
#include <MQTT.h>
#include <HTU21D.h>
#include <JsonParserGeneratorRK.h>
#include <HttpClient.h>


#define LOGGING
char httpserverAddress[] = "XXX.XX.XX.XX";  // server address
const int hueHubPort = 80;
String hueUsername;
bool hueJoinned = false;
bool fan = false;

// Global parser that supports up to 256 bytes of data and 20 tokens
JsonParserStatic<256, 20> parser;

TCPClient clienttcp;
HttpClient http;

http_request_t request;
http_response_t response;



// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
     { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
    //{ "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
 

boolean ok=false;
unsigned long timestart=millis(); // save for 15 seconds timeout


Adafruit_MLX90614 mlx = Adafruit_MLX90614();
//Create an instance of the object
HTU21D myHumidity;


int outputValue = 0;        // value output to the PWM (analog out)
int ObjectTempOffset =5; //Window temp offset + differential below top and bottom of glass


char mqttUserName[] = "Window Condensation";    // Can be any name.
char mqttPass[] = "XXXXXXXXXXXX";        // Change this to your MQTT API Key from Account > My Profile.
char writeAPIKey[] = "XXXXXXXXXXXXX";     // Change this to your channel Write API Key.
long channelID = XXXXXXX;                     // Change this to your channel number.

char server[] = "mqtt.thingspeak.com";       // Define the ThingSpeak MQTT broker
static const char alphanum[] ="0123456789"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz";  // This variable is used for random generation of client ID.

typedef enum {
APP_STATE_INITIAL = 1,
APP_STATE_IP,
APP_STATE_RDY
}  state_t;

state_t state = APP_STATE_IP;


struct EEPROMStruct{
  char  header[18];
  state_t state;
  IPAddress httpserverAddress;
  char hueUsername[40];
} ;

EEPROMStruct EEPROMData = {"WindowCond_EEPROM",APP_STATE_IP,IPAddress(XXX,XX,XX,XXX),"Blank"};


//union {
//    structEEPROM eevar;
///    char eeArray[sizeof(structEEPROM)];
//} EEPROMData;

// Define a callback function to initialize the MQTT client.
void callback(char* topic, byte* payload, unsigned int length) {
}


MQTT MQTTclient(server, 1883, callback);       // Initialize the MQTT client.

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 200L * 1000L; // Post data every 20 seconds.

void initEEPROM(void){
    EEPROM.clear();
    EEPROM.put(0,EEPROMData);//writeEEPROM();
    Particle.publish("Init EEPROM");
    Serial.println("Init EEPROM");
}

void dumpEEPROM(void){
    
    for (int i=0; i<EEPROM.length();i++){
        uint8_t dump = EEPROM.read(i);
        Serial.print(dump,HEX);
        Serial.print(":");
    }
    Serial.println("");
    for (int i=0; i<EEPROM.length();i++){
        char dump = EEPROM.read(i);
        Serial.print(dump);

    }
}

void setup() {
  STARTUP(WiFi.selectAntenna(ANT_AUTO));
  // Make sure your Serial Terminal app is closed before powering your device
  Serial.begin(9600);
  // Wait for a USB serial connection for up to 30 seconds
  waitFor(Serial.isConnected, 30000);
  Serial.print("connecting...");
  EEPROMStruct EEPROMRead;
  EEPROM.get(0,EEPROMRead);
  //dumpEEPROM();
  //println(EEPROMData.state);
  String header =EEPROMRead.header;
  Serial.println(header);
  
  if (header != "WindowCond_EEPROM") initEEPROM();
   Serial.println(EEPROMRead.header);
  EEPROM.get(0,EEPROMData);
  //dumpEEPROM();
    while (!myHumidity.begin())
        {
	    Particle.publish("HTU21D not found");
	    delay(1000);
	}

    while (!mlx.begin())
        {
	    //Particle.publish("MLX not found"); mlx doesn't check if it is working
	    delay(1000);
	}
                // Initialize  sensor.
    request.ip = IPAddress(192,168,0,37);
    request.port = hueHubPort;
}

void loop() {
    
  // If MQTT client is not connected then reconnect.
    if (!MQTTclient.isConnected())  reconnect();
    
    
   MQTTclient.loop();  // Call the loop continuously to establish connection to the server.
    
    //if (millis() - lastConnectionTime > postingInterval)
    if (Time.second() == 30) mqttpublish();
    
    delay(1000);
    Serial.println("Sending");
  //http.get(request, response, headers);
  //request.body = "{\"on\": true}";
int state = EEPROMData.state;
    switch (EEPROMData.state){
      
  /*case 0:
  {
      Serial.println("APP_STATE_INIT");
      //connected = clienttcp.connect();
      clienttcp.println("https://discovery.meethue.com");
       while (clienttcp.available()) {
            char c = clienttcp.read();
             Serial.print(c);
            #ifdef LOGGING
            Serial.print(c);
            #endif
            

            if (c == -1) {
                #ifdef LOGGING
                Serial.println("HttpClient>\tError: No data available.");
                #endif
                break;
            }
      
       }
  }
  break;
  */
    case APP_STATE_IP :
        {
        Serial.println(EEPROMData.hueUsername);
        if (EEPROMData.hueUsername != "Blank"){
            Serial.println(F("User Request"));
            request.path = "/api ";
            request.body = "{\"devicetype\":\"user#test\"}";
            //http.put("/api/BD0AB797F7/lights/32/state", "Content-Type: application/json", "{\"on\": true}");
            http.post(request, response, headers);
            delay(20);
        }
        
        Serial.println(F("CHECK Response"));
        Serial.print("Application>\tResponse status: ");
        Serial.println(response.status);
        Serial.print("Application>\tHTTP Response Body: ");
        Serial.println(response.body);
        Particle.publish(response.body);
        int l = response.body.length();
        char  data[l];
        Serial.println(l);
        response.body.toCharArray(data, l);
        for (int i =0;i<(l-1);i++)  data[i] = data[i+1]; //shorth by one
        parser.clear();
        parser.addString(data);
        Serial.println(data);
        if (!parser.parse()) {
		    Serial.println("APP_STATE_IP parsing failed test"); }else
	        {
            String username = parser.getReference().key("success").key("username").valueString();
            String error = parser.getReference().key("error").key("description").valueString();
            Serial.println(username);
                if (username != "") {
                    Serial.println(username.length());
                    username.toCharArray(EEPROMData.hueUsername, 41); //Only 40 are required but for some reason you need to add one
                     Serial.println(EEPROMData.hueUsername);
                    state = APP_STATE_RDY;
                    EEPROMData.state = APP_STATE_RDY;
                    EEPROM.put(0,EEPROMData);//writeEEPROM();
                    }
        Serial.println(error);
	        }
        
	}
	break;
	
	case APP_STATE_RDY :
	    {   
	        Serial.print("APP_STATE_READY");
	        String username ="";
	        for (int i =0;i < 40;i++){
	            username = username + EEPROMData.hueUsername[i];
	            //Serial.print(EEPROMData.hueUsername[i]);
	        }
            if ( fan == true){
                        request.body = "{\"on\":true}";
                        Serial.println("Fan On");
                        Particle.publish("Fan On");
                        }else{
                        request.body = "{\"on\":false}";
                        Serial.println("Fan Off");
                        Particle.publish("Fan Off");
                        }
                String lightnumber = "15";
                request.path = "/api/" + username + "/lights/" + lightnumber + "/state";
                Serial.println(request.path);
                //http.put("/api/BD0AB797F7/lights/32/state", "Content-Type: application/json", "{\"on\": true}");
                http.put(request, response, headers); 
                Serial.println(response.body);
                
                int l = response.body.length();
                char  data[l];
                response.body.toCharArray(data, l);
                Serial.println(l);
                for (int i =0;i<(l-1);i++)  data[i] = data[i+1]; //shorth by one
                for (int i =0;i<(l-1);i++)  Serial.print(data[i]);
                Serial.println(" ");
                parser.clear();
                parser.addString(data);
                Serial.println(data);
                if (!parser.parse()) {
		            Serial.println("APP_STATE_RDY parsing failed test"); }else
	            {
               
                String error = parser.getReference().key("error").key("description").valueString();
                Serial.println(error);
                if (error == "unauthorized user") {
                    Serial.println("New Hue User Key Required");
                    
                    state = APP_STATE_RDY;
                    EEPROMData.state = APP_STATE_IP;
                    EEPROM.put(0,EEPROMData);//writeEEPROM();
                    }
	            }
       
             }
             break;
	default:
	Serial.println("Switch Error");
	
}
	


}


void reconnect(){
    
     char clientID[9];
     
    Particle.publish("Attempting MQTT connection");
     // Generate ClientID
     for (int i = 0; i < 8; i++) {
         clientID[i] = alphanum[random(51)];
     }
     clientID[8]='\0';
        
     // Connect to the ThingSpeak MQTT broker.
     if (MQTTclient.connect(clientID,mqttUserName,mqttPass))  {
         Particle.publish("Conn:"+ String(server) + " cl: " + String(clientID)+ " Uname:" + String(mqttUserName));
     } else
     {
         Particle.publish("Failed to connect, Trying to reconnect in 5 seconds");
         delay(5000);
     } 
}

void mqttpublish() {
    // put your main code here, to run repeatedly:
    Particle.publish("Repeat");
    // MLX
    double AmbientTemp = mlx.readAmbientTempC(); 
    double win_temp = mlx.readObjectTempC(); 
    // HTU21
    double humd = myHumidity.readHumidity();
    double air_temp = myHumidity.readTemperature();
    double dewp = air_temp-((100-humd)/5);
    double delta_dewpVwintemp = win_temp-dewp;
    Particle.variable("Air_temp",air_temp);
    Particle.variable("Humidity",humd);
    Particle.variable("Win_temp",win_temp);
    Particle.variable("Dew",dewp);
    Particle.variable("DeltaDew",delta_dewpVwintemp);
    
    String Condensation_Flag;
    if (humd ==998| air_temp == 998){
        Particle.publish("Timeout I2C");
    }
    if (humd ==999| air_temp == 999){
        Particle.publish("CRC Error");
    }
   
    if (delta_dewpVwintemp  > 2)
    {
        fan = false;
    }
    else
    {
        fan = true;
    }
    // Create data string to send data to ThingSpeak.
    // Use these lines to publish to a channel feed,
    // which allows multiple fields to be updates simultaneously.
    // Comment these lines and use the next two to publish to a single channel field directly.
    String data = String("field1=" + String(humd) + "&field2=" + String(air_temp) + "&field3=" + String(win_temp)+ "&field4=" + String(dewp)+"&field5=" + Condensation_Flag);
    String topic = String("channels/"+String(channelID)+ "/publish/"+String(writeAPIKey));
    
    //String data=String(t);
    //String topic = String("channels/"+String(channelID)+ "/publish/fields/field1/"+String(writeAPIKey));
    
    MQTTclient.publish(topic,data);
    
    lastConnectionTime = millis();
}
