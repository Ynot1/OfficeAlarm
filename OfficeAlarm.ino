#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>
#include "switch.h"
#include "UpnpBroadcastResponder.h"
#include "CallbackFunction.h"

/*
 * I/O allocation for the ESP32 board
 * IO5 - KEYPAD set/reset control
 * IO4 - Office PIR movement detected
 * IO15 - Garage Door Open
 * IO0?? - Hatch Open

 *  - Relay 1 - pulses on boot - dont use
 * IO14 - Relay 2 - Alarm Set/Unset state
 * IO12  - Relay 3 - Alarm Sounding
 * 1013 - relay 4
 * 
 * WatchDog Codes
 * 30 alarm set
 * 31 alarm unset
 * 32 in exit or entry  delay
 * 33 alarm sounding
 */

// prototypes
boolean connectWifi(); // router handed out 192.168.1.10 for this on an esp01 , and 192.168.1.218 for the 4 relay esp32 board

//on/off callbacks 
bool OfficeAlarmSetOn();
bool OfficeAlarmSetOff();
bool OfficeAlarmUnsetOn();
bool OfficeAlarmUnsetOff();
//bool OfficeAlarmRebootOn();
//bool OfficeAlarmRebootOff();
//bool AlarmSetLockout = LOW; // HIGH is lockout state
//bool AlarmUnsetSetLockout = LOW;

// Change this before you flash
const char* ssid = "A_Virtual_Information_Ext";
const char* password = "BananaRock";

boolean wifiConnected = false;

int AlarmSetLockoutCounter;
int AlarmSetLockoutCounterThreshold = 30;

UpnpBroadcastResponder upnpBroadcastResponder;

Switch *OfficeAlarmSet = NULL;
Switch *OfficeAlarmUnset = NULL;
Switch *OfficeAlarmboot = NULL;

bool isOfficeAlarmSetOn = false;
bool isOfficeAlarmUnsetOn = false;
//bool isOfficeAlarmBootOn = false;
bool OfficeAlarmSetState = true; // so it boots alarm unset
bool ExitDelayRequest = true; // so it boots alarm 
bool PrevExitDelayRequest = false;
//bool OfficeAlarmSoundingState = false;
//bool PrevOfficeAlarmSoundingState = false;
bool SetUnsetRead = false;
bool PrevSetUnsetRead = false;
bool GarageDoorRead = false;
bool PrevGarageDoorRead = false;
bool OfficePIRRead = false;
bool PrevOfficePIRRead = false;
bool HatchRead = false;
bool PrevHatchRead = false;
bool IntruderDetectedLatch  = LOW;

bool ExtendedEntryOnlyOnce = false;
bool SetUnsetRelayPinState = false;
bool PrevSetUnsetRelayPinState = false;
const int SetUnsetInputPin = 5;  // 
const int OfficePIRInputPin = 4;  // 
const int GarageDoorInputPin = 0;  // 
const int HatchInputPin = 15;  // Doesnt work???
const int SetUnsetRelayPin = 14;  // SetUnsetRelayPin pin (io14=relay2)
const int GPIO2 = 2;  // GPIO2 pin. used as LED Driver
const int SoundAlarmPin = 12; 

byte ExitDelayTimer = 0;
byte ExitDelayTimerThreshold = 200;// value of 30 is about 5secs. 200 is about 20 sec
byte AlarmSoundingTimer = 0;
byte AlarmSoundingTimerThreshold = 200;// value of 30 is about 5secs. 200 is about 20 sec
/*

GPIO1 (TXD) = unused exept for serial debug
Onboard relay = AlarmSet/Unset control
*/

byte VBNumber = 34; // 34 is the watchdog post value for the Office Alarm

const char* WatchDogHost = "192.168.1.60"; // ip address of the watchdog esp8266

long WatchDogCounterLoopThreshold = 200;// value of 30 is about 5secs. 200 is about 20 sec
long WatchDogLoopCounter = 0;
byte ExtendedEntryThreshold = 100;// value of 30 is about 5secs. 200 is about 20 sec
byte ExtendedEntryCounter = 0;

byte rel1ON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to serial for open relay 1 - set/unset alarm
byte rel1OFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay 1

//byte PanelBuzzerCountThreshold = 20;
//byte PanelBuzzerCount = (PanelBuzzerCountThreshold - 2);

//int PanelBuzzerSmoothCount = 0;
//int PanelBuzzerSmoothCountThreshold = 5;
//int PanelBuzzerSmoothCountMax = 10;



byte EntryDelayTimer = 0;
byte EntryDelayTimerThreshold = 200;
bool SoundAlarmState = 0;
bool PrevSoundAlarmState = 0;

void setup()
{
 
  pinMode(GPIO2, OUTPUT); 
     
  Serial.begin(115200);

  Serial.println("Booting Office Alarm Nov 2023...");
  //delay(2000);

  //flash fast a few times to indicate CPU is booting
  digitalWrite(GPIO2, LOW); 
  delay(100);    
  digitalWrite(GPIO2, HIGH);
  delay(100); 
  digitalWrite(GPIO2, LOW); 
  delay(100);    
  digitalWrite(GPIO2, HIGH);
  delay(100);   
  digitalWrite(GPIO2, LOW); 
  delay(100);   
  digitalWrite(GPIO2, HIGH);
  
  Serial.println("Booting Office Alarm Nov 2023...");
  delay(2000);   
  
  // Initialise wifi connection
  wifiConnected = connectWifi();
     
  if(wifiConnected){

  //flash slow a few times to indicate wifi connected OK
  digitalWrite(GPIO2, LOW); 
  delay(1000);    
  digitalWrite(GPIO2, HIGH);
  delay(1000); 
  digitalWrite(GPIO2, LOW); 
  delay(1000);    
  digitalWrite(GPIO2, HIGH);
   delay(1000); 
  digitalWrite(GPIO2, LOW); 
  delay(1000);    
  digitalWrite(GPIO2, HIGH);
    
    upnpBroadcastResponder.beginUdpMulticast();
    
    // Define your switches here. Max 10
    // Format: Alexa invocation name, local port no, on callback, off callback
    OfficeAlarmSet = new Switch("Enable Office Security", 72, OfficeAlarmSetOn, OfficeAlarmSetOff);
    OfficeAlarmUnset = new Switch("Disable Office Security", 73, OfficeAlarmUnsetOn, OfficeAlarmUnsetOff);
    //OfficeAlarmboot = new Switch("Office Security reboot", 74, OfficeAlarmBootOn, OfficeAlarmBootOff);

    Serial.println("Adding switches upnp broadcast responder");
    upnpBroadcastResponder.addDevice(*OfficeAlarmSet);
    upnpBroadcastResponder.addDevice(*OfficeAlarmUnset);
    //upnpBroadcastResponder.addDevice(*OfficeAlarmboot);
    
  }
       digitalWrite(GPIO2, HIGH); // turn off LED 

   //Serial.println("Making AlarmSoundingInputPin into an INPUT"); 
   //pinMode(AlarmSoundingInputPin, FUNCTION_3);
   //pinMode(AlarmSoundingInputPin, INPUT);
   



    Serial.println("Making SetUnsetInputPin into an INPUT"); // used to detect Office Alarm Set/Unset state
   pinMode(SetUnsetInputPin, FUNCTION_3);
   pinMode(SetUnsetInputPin, INPUT);

   Serial.println("Making OfficePIRInputPin into an INPUT"); // used to detect Office PIR Movement
   pinMode(OfficePIRInputPin, FUNCTION_3);
   pinMode(OfficePIRInputPin, INPUT);
  
    Serial.println("Making GarageDoorInputnPin into an INPUT"); // used to detect Garage Door Opening
   pinMode(GarageDoorInputPin, FUNCTION_3);
   pinMode(GarageDoorInputPin, INPUT);

   Serial.println("Making HatchInputPin into an INPUT"); // used to detect Hatch opening (doesnt work yet)
   pinMode(HatchInputPin, FUNCTION_3);
   pinMode(HatchInputPin, INPUT);
   
   Serial.println("Making SetUnsetRelayPin into an OUTPUT"); // used to drive Office Alarm Set/Unset state relay
   pinMode(SetUnsetRelayPin, FUNCTION_3);
   pinMode(SetUnsetRelayPin, OUTPUT);

   Serial.println("Making SoundAlarmPin into an OUTPUT"); // used to drive Office Alarm Sounding relay
   pinMode(SoundAlarmPin, FUNCTION_3);
   pinMode(SoundAlarmPin, OUTPUT);

   

   

}
     
void loop()
{
// Monitor the keypad for a set/unset command
  SetUnsetRead = digitalRead(SetUnsetInputPin); //

  if (SetUnsetRead == LOW) {
      if (PrevSetUnsetRead == HIGH){ 
        Serial.println("SetUnset has just toggled");  
        
        ExitDelayRequest = !ExitDelayRequest; // toggle the Exit Delay Request state

          if (ExitDelayRequest == LOW) {
            
              //ExtendedEntryCounter = 0;
              //ExtendedEntryOnlyOnce = false;
                           
            }
              if (PrevExitDelayRequest == HIGH){ 
                Serial.println("Office Alarm has just entered Exit Delay via keypad");
                
                

              }
          }       
          
              
          if (ExitDelayRequest == HIGH) {
               if (PrevExitDelayRequest == LOW){ 
                //Serial.println("Office Alarm has just Unset via keypad ");
                //digitalWrite(SetUnsetRelayPin, LOW); // turn off relay 2
                //ExtendedEntryCounter = 0;
                //ExtendedEntryOnlyOnce = false;
                VBNumber = 31; // Office Alarm Unset code
                ProxyPost();
                Serial.println("Office Alarm has just Unset via keypad  - sending notification to Watchdog");
              }
          } 
              
      }   // end  SetUnsetRead  == LOW
 

  PrevSetUnsetRead  = SetUnsetRead; // for comparison next pass


//Monitor the GargeDoor for opening
   GarageDoorRead = digitalRead(GarageDoorInputPin); //

  if (GarageDoorRead == LOW) {
      if (PrevGarageDoorRead == HIGH){ 
        Serial.println("GarageDoor has just opened");  
              
      }      
  }

  PrevGarageDoorRead  = GarageDoorRead; // for comparison next pass

//Monitor the OfficePIR for movement
   OfficePIRRead = digitalRead(OfficePIRInputPin); //

  if (OfficePIRRead == LOW) {
      if (PrevOfficePIRRead == HIGH){ 
        Serial.println("OfficePIR has just registered movement");  
              
      }      
  }

  PrevOfficePIRRead  = OfficePIRRead; // for comparison next pass

//Monitor the Hatch for opening
        HatchRead = digitalRead(HatchInputPin); //

  if (HatchRead == LOW) {
      if (PrevHatchRead == HIGH){ 
        Serial.println("Hatch has just opened");  
              
      }      
  }

  PrevHatchRead  = HatchRead; // for comparison next pass
  
 
// check for when Office alarm unsets
 if (ExitDelayRequest == HIGH) {
     if (PrevExitDelayRequest == LOW){ 
                Serial.println("Office Alarm has just Unset");
                ExitDelayTimer = 0; // dump Exit Delay timer
                
     }
 } 


 

// Monitor ExitDelayRequest and maintain the exit delay timer


if (ExitDelayRequest == LOW) { // ExitDelay is expiring or has expired
    //Serial.println("ExitDelay is expiring or has expired");
    ExitDelayTimer = ExitDelayTimer +1;
    
    
    if (ExitDelayTimer >= ExitDelayTimerThreshold) {
    //Serial.println("ExitDelay has expired"); 
    ExitDelayTimer = ExitDelayTimerThreshold;
    

     SetUnsetRelayPinState = HIGH;
     digitalWrite(SetUnsetRelayPin, SetUnsetRelayPinState); // set relay 2
     
     //Serial.println("Exit Delay expired Setting Relay #2");

     if (SetUnsetRelayPinState == HIGH) { // Alarm is set
        if (PrevSetUnsetRelayPinState == LOW) { // and it wasnt last pass

        VBNumber = 30; // Office Alarm Set code
        ProxyPost();
        Serial.println("ExitDelay has just expired - sending notification of Alarm Set to Watchdog"); 
        }
     }
           
    
    }
    else {
      SetUnsetRelayPinState = LOW; 
      digitalWrite(SetUnsetRelayPin, SetUnsetRelayPinState); // Unset relay 2
      //ExitDelayTimer = 0;
    //Serial.println("ReSetting Relay #2");
    }

//ExitDelayTimer = 0;
//byte ExitDelayTimerThreshold = 200;// value of 30 is about 5secs. 200 is about 20 sec
 
  
}
else {
    SetUnsetRelayPinState = LOW;
    digitalWrite(SetUnsetRelayPin, SetUnsetRelayPinState); // Unset relay 2 
    //ExitDelayTimer = 0;
//Serial.println("ReSetting Relay #2");
}


PrevExitDelayRequest = ExitDelayRequest; // edge detection of ExitDelayRequest state
PrevSetUnsetRelayPinState = SetUnsetRelayPinState; // edge detection


// Monitor Entry Zones 

 if (SetUnsetRelayPinState == HIGH && IntruderDetectedLatch == LOW) { // Alarm is set and we are not already in entry delay
      // Here if alarm is set
      if (GarageDoorRead == LOW || OfficePIRRead == LOW) {
      // Here if alarm is set and an intruder is detected
      IntruderDetectedLatch = HIGH;
      Serial.println("Intruder Detected Starting Entry delay countdown"); 
      
      }
    
 }
 else {
  
 }

// Monitor Non Entry Zones 


//  Maintain Entry Delay and Sound Alarm

if (IntruderDetectedLatch == HIGH) {
    // here if Entry Delay needs incrementing or has reached terminal count;
    EntryDelayTimer = EntryDelayTimer +1;
    if (EntryDelayTimer > EntryDelayTimerThreshold) {
        EntryDelayTimer = EntryDelayTimerThreshold;
        SoundAlarmState = HIGH; // sound the alarm
        digitalWrite(SoundAlarmPin, SoundAlarmState); // Sound the alarm (relay #3)
        if (SoundAlarmState == HIGH && PrevSoundAlarmState == LOW) {
          // Here if Alarm has just started sounding
          Serial.println("Entry Delay Expired, Sounding Alarm"); 
          VBNumber = 33; // Office Alarm Sounding code
          ProxyPost();
        }
         
    }
  
}

PrevSoundAlarmState = SoundAlarmState;

// detect intruder alarm just occured and send proxy 32

// Stop Alarm after Alarm Duration, reset intruder latch, dump timers ect

if (SoundAlarmState == HIGH){
    // here if alarm is sounding
    AlarmSoundingTimer = AlarmSoundingTimer +1;
    if (AlarmSoundingTimer > AlarmSoundingTimerThreshold) {
        // here if alarm has been sounding long enough
        AlarmSoundingTimer = AlarmSoundingTimerThreshold;
         Serial.println("Alarm Sounding Delay Expired, Silencing Alarm");
         SoundAlarmState = LOW; // silence the alarm
        digitalWrite(SoundAlarmPin, SoundAlarmState); // Silence the alarm (relay #3)
        AlarmSoundingTimer = 0;
        IntruderDetectedLatch = LOW; // wait for next entry event
    }
}



 
  if(wifiConnected){
     //Serial.println("XXX4 ");
     // digitalWrite(GPIO2, LOW); // turn on LED with voltage Low
      upnpBroadcastResponder.serverLoop();
     //Serial.println("XXX5 ");
      //OfficeAlarmboot->serverLoop();
      OfficeAlarmUnset->serverLoop();
      OfficeAlarmSet->serverLoop();
      //Serial.println("XXX6 ");
   }

   WatchDogLoopCounter = WatchDogLoopCounter +1;
   //Serial.println(WatchDogLoopCounter);
   if (WatchDogLoopCounter > WatchDogCounterLoopThreshold) {
    //Serial.println("XXX6");
        //PanelBuzzerCount = (PanelBuzzerCountThreshold - 4);
        WatchDogLoopCounter = 0;
        VBNumber = 34; // Office Alarm watchdog code
        WatchDogPost();
        //Serial.println("XXX7 ");
   }


// Loop delay 
  delay(100); 
  
} // end Void Loop


bool OfficeAlarmSetOn() {
    Serial.println("Request to Set Office Alarm received ");

      //if (ExitDelayRequest == LOW) { // only change relay if Office Alarm is currently Unset
          //sometimes alexa sends this request again about 2 secs later which turned the alarm off again on the second request
          // we need to lockout multiple turn on requests that are received in quick succession
          // or maybe, just extend the pulse duration ? (was 1 sec) - didnt work...
          //if (AlarmSetLockout == LOW) { // only allows set routine to run once, initally needed the alarm off request to release this
            // but this gave rise to problems if the alarm was set via alexa and reset via keypads or RF remote.
            // changed to reset automatically after 5 secs
             
            
            //AlarmSetLockout = HIGH; // set the lockout
            
            ExitDelayRequest = LOW; // set the alarm state

                  
                      
                        Serial.println("Office Alarm  has just entered Set State via Alexa");
                        
                       //digitalWrite(SetUnsetRelayPin, HIGH); // turn on relay 2
                       //ExtendedEntryCounter = 0;
                       //ExtendedEntryOnlyOnce = false;
                       //VBNumber = 30; // Office Alarm Set code
                       //ProxyPost();
                       //Serial.println("ExitDelay has just expired - sending notification to Watchdog");
                                             
 
              //}
            //}
      //else {
          //Serial.println("Office Alarm is already Set - not pulsing relay!");
      //}
         
    isOfficeAlarmSetOn = false;    
    return ExitDelayRequest;
} // end OfficeAlarmSetOn()

bool OfficeAlarmSetOff() { 
    
    Serial.println("Request to Set Office Alarm received ");
    Serial.println("This should never happen");
                  
    isOfficeAlarmSetOn = false;
    return ExitDelayRequest;
} // end OfficeAlarmSetOff()

bool OfficeAlarmUnsetOn() {
    Serial.println("Request to Unset Office Alarm received ");

      //if (ExitDelayRequest == HIGH) { // only pulse relay if Office Alarm is currently Set
                    
              // AlarmSetLockout = LOW; // reset the lockout for the turn on function
              // this in asymetric and doesnt have a lockout for preventing multiple offs like the on function
              // becasue the alarm unsets immediatly and prevents any subsequent requests from alexa as being
              // processed as on commands.
              // I think.

               ExitDelayRequest = HIGH; // Unset the alarm state
               
                //Serial.println("Office Alarm has just entered Unset State via Alexa ");
                //digitalWrite(SetUnsetRelayPin, LOW); // turn off relay 2
                ExtendedEntryCounter = 0;
                ExtendedEntryOnlyOnce = false;
                VBNumber = 31; // Office Alarm Unset code
                ProxyPost();
                Serial.println("Office Alarm has just entered Unset State via Alexa - sendng notication to watchdog ");
                       
      //}
      //else {
          //Serial.println("Office Alarm is already Unset, not pulsing relay...");
      //}
      
    isOfficeAlarmUnsetOn = false;
    return ExitDelayRequest;
} // end OfficeAlarmUnsetOn()

bool OfficeAlarmUnsetOff() {  

    Serial.println("Request to Unset Office Alarm received (SW#2 Off)");
    Serial.println("This should never happen");
  
  isOfficeAlarmUnsetOn = false;
  return ExitDelayRequest;
} // end OfficeAlarmUnsetOff()


//bool OfficeAlarmBootOn() {
    //Serial.println("Request to reboot Office Alarm controller received SW#3 On");

 //ESP.restart(); hangs..
 //ESP.reset();
      
    //isOfficeAlarmBootOn = false;
    //return isOfficeAlarmBootOn;
//}

//bool OfficeAlarmBootOff() {  

    //Serial.println("Request to reboot Office Alarm controller received (SW#3 Off)");
    //Serial.println("This should never happen");

  
  //isOfficeAlarmBootOn = false;
  //return ExitDelayRequest;
//}

// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi Network");

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.print(".");
    if (i > 10){
      state = false;
      break;
    }
    i++;
  }
  
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed. Bugger");
  }
  
  return state;
} // end connectwifi

void ProxyPost() {
TwitchLED();
// assumes VBNumber set to desired VB call to be made
// 32 Exit Delay
// 32 Entry Delay
// 30 is Office Alarm has Seted
// 31 is Office Alarm is Unset
// 33 is Office Alarm Sounding
  Serial.print("Requesting POST to WatchDog ");
  Serial.println(VBNumber);

  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(WatchDogHost, httpPort)) {
    Serial.println("connection failed");
    return;
  }

String data = "";

   
   
   // Send request to the server:
   client.println("POST / HTTP/1.1");
   //Serial.println("VB button"+(String(VBNumber))+" request sent");
   client.println("Host: ProxyRequest" +(String(VBNumber))); // this endpoint value gets to the server and is used to transfer the identity of the calling slave
   client.println("Accept: */*"); // this gets to the server!
   client.println("Content-Type: application/x-www-form-urlencoded");
   client.print("Content-Length: ");
   client.println(data.length());
   client.println();
   client.print(data);
  
   delay(500); // Can be changed
  if (client.connected()) { 
    client.stop();  // DISCONNECT FROM THE SERVER
  }
  Serial.println();
  Serial.println("closing connection");
  delay(1000);
}// end ProxyPost

void WatchDogPost() {

TwitchLED();

// assumes VBNumber set to desired VB call to be made

// 34 is Office Alarm watchdog

  Serial.print("Requesting POST to WatchDog ");
  Serial.println(VBNumber);

  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(WatchDogHost, httpPort)) {
    Serial.println("connection failed");
    return;
  }

String data = "";

   
   
   // Send request to the server:
   client.println("POST / HTTP/1.1");
   //Serial.println("VB button"+(String(VBNumber))+" request sent");
   client.println("Host: WatchDog Endpoint" +(String(VBNumber))); // this endpoint value gets to the server and is used to transfer the identity of the calling slave
   client.println("Accept: */*"); // this gets to the server!
   client.println("Content-Type: application/x-www-form-urlencoded");
   client.print("Content-Length: ");
   client.println(data.length());
   client.println();
   client.print(data);
  
   delay(500); // Can be changed
  if (client.connected()) { 
    client.stop();  // DISCONNECT FROM THE SERVER
  }
  Serial.println();
  Serial.println("closing connection");
  delay(1000);
}// end WatchDogPost

void TwitchLED () {

  pinMode(GPIO2, OUTPUT); // switch to an output
  
  digitalWrite(GPIO2, LOW);
  delay(10);
  digitalWrite(GPIO2, HIGH);

  pinMode(GPIO2, INPUT); // switch back to an input
}
