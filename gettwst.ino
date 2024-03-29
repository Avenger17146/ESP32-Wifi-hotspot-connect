
/*
 * 
 * INFO:
 * 0-31 contains the ssid of the user
 * 32 - 95 contains the password of the user.
 * 
 * USAGE:
 * 
 * 1. For Writing. ( Original data lies in wifi_pass and wifi_ssid )
 *    construct_charArrays(wifi_ssid, wifi_pass);
 *    write_to_EEPROM();
 *    
 * 2. For Debugging ( prints the stored data in EEPROM )
 *    read_from_EEPROM();
 *    
 * 3. For reading data ( gets the stored data from EEPROM and stores in ret_wifi_ssid & ret_wifi_pass)
 *    retrieve_from_EEPROM();
 *    
 */

// https://github.com/espressif/arduino-esp32  : basic esp32 drivers and libraries 

#include "EEPROM.h"       
#include "ESPAsyncWebServer.h"          //https://github.com/me-no-dev/ESPAsyncWebServer     
#include "WiFi.h"
#include <String.h>
#include <HardwareSerial.h>             
#include "esp_system.h"                         
#define EEPROM_SIZE 96    // 32 + 64 = 96 bytes.

/////////////////////////////////////////////////////////////////////////////Global Varaibles/////////////////////////////////////////////////////////////////////////////////////////////////

HardwareSerial Serialx(1);               //for communication between esp32 and stm32
WiFiClient client;                       //client for recieving requests from server
WiFiClient client1;                      //client for sending responses to the server 
AsyncWebServer server(80);               //Server for hotspot functionality, hosted at 192.168.4.1

String device_ID ="";
char servern[] = "103.231.8.232";
const char *ssd = "MyESP32AP";
const char *password = "abcd1234";
int status = WL_IDLE_STATUS;
int debug = 1;                            //debug on at 1, off at 0
String return_string= "";                 //strings is which response from server is stored
String get_string = "";
String url = "";
unsigned long lastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 10L * 1000L;      // delay between updates, in milliseconds
int rflag = 0;     //esp32 reset flag 

//javascript for server on the htspot
const char index_html[]  = "<html> <head> <title>Glassco SSID panel</title><script type='text/javascript'> var str_SSID = ''; var str_passwor = '';function setData() { str_SSID = document.getElementById('SSID').value; str_passwor = document.getElementById('pass').value; var req_string = '?ssid='+ str_SSID +'&pass='+str_passwor; var url = window.location.href; if (url.indexOf('?') == -1) url += req_string; else{ q_mark = url.indexOf('?'); url = url.substring(0, q_mark); url += req_string; } window.location.href = url; }</script> </head> <body> <input id = 'SSID' placeholder='SSID name'></input><br> <input id = 'pass' placeholder='password' type = 'password'></input><br> <button id='apply' onclick='setData()'>Apply Changes!</button> </body> </html>";

String Pssid = "";     //details from server 
String Ppass = "";

char Pssid_arr[32];
char Ppass_arr[64];

int addr = 0;

// Original String Data.
String wifi_ssid = "INDIA";
String wifi_pass = "abcd12345";

// Data retrieved
char ret_wifi_ssid[32];
char ret_wifi_pass[64];

// INTERNAL VARIABLES
// Converted String to Char array.
  char toCharSsid[32];
  char toCharPass[64];

// Data that will be written.
  char ssid[32];
  char pass[64];

/////////////////////////////////////////////////////////////////////////Functions/////////////////////////////////////////////////////////////////////////////////////////////////////////////

  

void read_from_EEPROM()  //to read data from eeprom
{
    if ( debug == 1 )Serial.println("Bytes read : ");
  for (int i = 0; i < EEPROM_SIZE; i++){
      if ( debug == 1 )Serial.print((char)byte(EEPROM.read(i)));
  }
    if ( debug == 1 )Serial.println();
}

void construct_charArrays(String s1, String s2)  //Converting string to char arrays as required by various functions
{
  s1.toCharArray(toCharSsid, 32);
  s2.toCharArray(toCharPass, 64);
  
  for (int i = 0 ; i < 32; i++){
    if (i < s1.length()) ssid[i] = toCharSsid[i];
    else ssid[i] = '\0';
  }

  for (int i = 0 ; i < 64; i++){
    if (i < s2.length()) pass[i] = toCharPass[i];
    else pass[i] = '\0';
  }
}

void write_to_EEPROM()  //to store the ssid and pass recieved to eeprom
{
    if ( debug == 1 )Serial.println("Started writing");
  for (int i = 0 ; i < EEPROM_SIZE; i++){
    if (i < 32){
       if ( debug == 1 ) Serial.print(String(ssid[i]));
      EEPROM.write(addr, byte(ssid[i]));
      addr = addr + 1;
    }
    else {
        if ( debug == 1 )Serial.print(String(pass[i-32]));
      EEPROM.write(addr, byte(pass[i-32]));
      addr = addr + 1;
    }
  }

// return back to the first position.
    addr = 0;
    EEPROM.commit();
    if ( debug == 1 )Serial.println("Writing ended");
}

void retrieve_from_EEPROM()    //Read ssid and pass stored in eeprom
{
    if ( debug == 1 )Serial.println("Retrieving...");
  for (int i = 0; i < EEPROM_SIZE; i++){
    if (i < 32){
      ret_wifi_ssid[i] = (char)byte(EEPROM.read(i));
    }
    else{
      ret_wifi_pass[i-32] = (char)byte(EEPROM.read(i));
    }
  }
    if ( debug == 1 )Serial.println("Retrieved.");

  print_charArray(ret_wifi_ssid);
    if ( debug == 1 )Serial.println(" && ");
  print_charArray(ret_wifi_pass);
}

void print_charArray(char* arr)  //to print contents of a char array
{
  int i = 0;
  while (arr[i] != '\0')
  {
   i++;
  }
for (int j = 0; j < i; j++)
  {
     if ( debug == 1 )Serial.print(arr[j]);
  }
 if ( debug == 1 )Serial.println();
}



String getMacAddress() // Get MAC address for WiFi station
{
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  return String(baseMacChr);
}



void httpAck()   //sending response to the server
{
  client1.stop();
  if ( debug == 1 )Serial.println("http url is ");
  if ( debug == 1 ) Serial.println(url);
  if (client1.connect(servern, 80)) 
  {
    client1.println("GET glassco/pro/replyfromdevice.php?device_id="+device_ID+"reply="+url);
    if ( debug == 1 ) Serial.println("GET glassco/pro/replyfromdevice.php?device_id="+device_ID+"reply="+url);
    client1.println("Host: 103.231.8.232");
    client1.println("User-Agent: ArduinoWiFi/1.1");
    client1.println("Connection: close");
    client1.println();
    url = "";
  }
  else 
  {
    // Error message.
     if ( debug == 1 ) Serial.println("connection failed");
     status = WiFi.begin(ret_wifi_ssid, ret_wifi_pass);
  }
}

void httpRequest()  // sending GET request to the server.
{    
    client.stop();
    if (client.connect(servern, 80)) 
    {
      client.println("GET /glassco/pro/readcmd.php?device_id="+device_ID);
      client.println("Host: 103.231.8.232");
      client.println("User-Agent: ArduinoWiFi/1.1");
      client.println("Connection: close");
      client.println();
      lastConnectionTime = millis();
   } 
  else 
  {
    // Error message.
      if ( debug == 1 )Serial.println("connection failed");
      status = WiFi.begin(ret_wifi_ssid, ret_wifi_pass);
  }
}


void setup() {

  Serial.begin(115200);   //for debugging purposes 
  device_ID = getMacAddress();    //unique devide id aka mac address
  Serialx.begin(9600,SERIAL_8E1,22,23);    //for communication with Stm32 protocal as stated, pin no 22, 23 as serial pins
  if (!EEPROM.begin(EEPROM_SIZE)){ Serial.println("failed to initialise EEPROM"); delay(1000000);}
  WiFi.softAP(ssd, password);   //initializing the hotspot
 
    if ( debug == 1 )Serial.println();
    if ( debug == 1 )Serial.print("IP address: ");
    if ( debug == 1 )Serial.println(WiFi.softAPIP());

  retrieve_from_EEPROM();   //check for stored credentials
  int flag1 = 0;    //flags for cheching if password or ssid are empty
  int flag2 = 0;

//  checking for empty strings
  for (int i = 0 ; i < 32 ; i++)
  {
    if (ret_wifi_ssid[i] != '\0')
    {
      flag1 = 1;
      break;
    }
  }

    for (int i = 0 ; i < 32 ; i++)
    {
    if (ret_wifi_pass[i] != '\0')
    {
      flag2 = 1;
      break;
    }
  }

  int count = 0;      //for limiting the the time taken to attempt to the wifi
  if (flag1 != 0 && flag2 != 0)
  {
    if ( debug == 1 )
    Serial.println("SSid found from eeprom");
    while (status != WL_CONNECTED && count < 5)
    {
      if ( debug == 1 )
      Serial.print("Attempting to connect to SSID: ");
      if ( debug == 1 )
      Serial.println(ret_wifi_ssid);
      count++;
      status = WiFi.begin(ret_wifi_ssid, ret_wifi_pass);
      delay(10000);
    }
  }
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)     //server for recieving ssid and pass from user
  {
    request->send(200, "text/html", index_html);
    if ( debug == 1 ) Serial.println("here");
    if (request->getParam(0) != NULL && request->getParam(1)!=NULL) 
    {
      AsyncWebParameter* p1 = request->getParam(0);
      AsyncWebParameter* p2 = request->getParam(1);
      if ( debug == 1 )Serial.println("here3");
      Pssid = p1->value();
      Ppass = p2->value();
      if (Pssid != NULL && Ppass != NULL && !Pssid.equals("") && !Ppass.equals(""))
      if (Pssid.length() > 0 && Ppass.length() > 0)
      {
        construct_charArrays(Pssid, Ppass);
        write_to_EEPROM();
        int county = 0;
        Pssid.toCharArray(Pssid_arr, 32);
        Ppass.toCharArray(Ppass_arr, 64);
        status = WiFi.begin(Pssid_arr, Ppass_arr);
        while (status != WL_CONNECTED && county < 50 ) 
        {
          //cli();  
          if ( debug == 1 )Serial.print("Attempting to connect to SSID: ");
          Pssid.toCharArray(Pssid_arr, 32);
          Ppass.toCharArray(Ppass_arr, 64);
          rflag = 1;
          county++;
          if ( debug == 1 )Serial.println(Pssid);
          status = WiFi.begin(Pssid_arr, Ppass_arr);
          delay(100);
        }
  
      }
      //sei();
    }
    else
    {
      if ( debug == 1 )Serial.println("Wait for request to be non empty");
    }
  });
  server.begin();
}

void loop()  // put your main code here, to run repeatedly:
{
 if ( (status != WL_CONNECTED)&& rflag == 1 )  //rebooting esp if stuck in a connecting loop
 {
  Serial.flush();
  Serialx.flush();
  esp_restart();
 }
 if ( (status != WL_CONNECTED) )   //in case the connection breaks 
 {
    if ( debug == 1 )
    Serial.print("Attempting to connect to SSID: ");
    if ( debug == 1 )
    Serial.println(ret_wifi_ssid);
    status = WiFi.begin(ret_wifi_ssid, ret_wifi_pass);
    delay(10000);
 }
 
   if ( !(status != WL_CONNECTED) )
   {
      if ( debug == 1 )Serial.print("SSID : " + Pssid + "\n");
      if ( debug == 1 )Serial.print("Pass : " + Ppass + "\n");
      get_string = "";
      while (client1.available()) //getting response after submitting the response if any ( just a failsafe )
      { 
        char c = client1.read();
        get_string = get_string + c;
      }
     if ( debug == 1 )Serial.print("get_string ");
     if ( debug == 1 )Serial.println(get_string);
     
     while (client.available()) // getting the response from server (commands to be executed in stm )
    { 
      char c = client.read();
      return_string = return_string + c;
    }
    int j = 0, k = 0, i = 1;  //variables to help in parsing of recieved request
    String Request;
    char request[10];
    if ( debug == 1 )Serial.print("return_string ");
    if ( debug == 1 )Serial.println(return_string);
    if (return_string.charAt(return_string.length()-1) !='#' && return_string.length()> 4)  //adding ## to the end of request to ease parsing
    {
      return_string+= "##";
    }
    while ( j < return_string.length()-4 && return_string.length() > 4 )
    {
      k = return_string.indexOf("##",k+1);
    if (  k == -1 )   //if request is empty orend of string is reached
    {
        if ( debug == 1 )Serial.println("## not found");
        break;
    }  
    Serialx.flush();  
    //parsing commands i.e. string manipulation or splitting
    Request = return_string.substring(j+4,k);
    if ( debug == 1 )Serial.println(Request);
    Request.trim();
    Request = Request + " \r ";
    if ( debug == 1 )Serial.println(Request);
    Request = Request + "\n";
    Request.toCharArray(request,Request.length());
    if ( debug == 1 )print_charArray(request);
    Serialx.write(request);       // Requesting for the first input value from the STM.
    Serialx.write('\n');
    Serialx.flush();
    delay(200);                   //sufficient delay to prevent any hangs     
    char response[1000];
    String Response;
    int resd = Serialx.available();
    Serialx.readBytesUntil('\n',response,resd);        // Getting the response from the STM.
    Response = String(response);
    Response.trim();
    url = url + "##" + i + "#" + Response ;   //creating url of response
    i++;
    j = k;
    delay(200);    //sufficient delay to prevent any hangs     
  }
  if ( !url.equals(""))
  {
     httpAck();
  }
   
  if (millis() - lastConnectionTime > postingInterval)    //checking if time to ping the server
  {
    url = "";
    return_string = "";
    httpRequest();
  }
 }
}

