#include <SoftwareSerial.h>
#include "VoiceRecognitionV3.h"
#include <EtherCard.h>
/**        
  Connection
  Arduino    VoiceRecognitionModule
   2   ------->     TX
   3   ------->     RX
*/
VR myVR(2,3);    // 2:RX 3:TX, you can choose your favourite pins.

uint8_t records[7]; // save record
uint8_t buf[64];

// MAC Address должен быть уникальным в вашей сети. Можно менять. 
static byte mymac[] = { 
  0x5A,0x5A,0x5A,0x5A,0x5A,0x5A };
  
// ip статический / постоянный Address нашей Web страницы.  
static byte myip[] = { 
  192,168,0,150 };

  // Буфер, чем больше данных на Web странице, тем больше понадобится значения буфера.
byte Ethernet::buffer[300];
BufferFiller bfill;

// Пин подключения LED
int led = 7;

// Переменная для фиксации изменений.
boolean PinStatus;

#define onRecord    (1)
#define offRecord   (0)

void printSignature(uint8_t *buf, int len)
{
  int i;
  for(i=0; i<len; i++){
    if(buf[i]>0x19 && buf[i]<0x7F){
      Serial.write(buf[i]);
    }
    else{
      Serial.print("[");
      Serial.print(buf[i], HEX);
      Serial.print("]");
    }
  }
}

//-------------

const char http_OK[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/html\r\n"
"Pragma: no-cache\r\n\r\n";

const char http_Found[] PROGMEM =
"HTTP/1.0 302 Found\r\n"
"Location: /\r\n\r\n";

const char http_Unauthorized[] PROGMEM =
"HTTP/1.0 401 Unauthorized\r\n"
"Content-Type: text/html\r\n\r\n"
"<h1>401 Unauthorized</h1>";

//------------

// Делаем функцию для оформления нашей Web страницы. 
void homePage()
{
  bfill.emit_p(PSTR("$F"
    "<meta charset = 'utf-8'>"
    "<title>Локальное управление светом</title>" 
    "Свет: <a href=\"?ArduinoPIN1=$F\">$F</a><br />"),   
  http_OK,
  PinStatus?PSTR("off"):PSTR("on"),
  PinStatus?PSTR("<font color=\"red\"><b>Выключен</b></font>"):PSTR("<font color=\"green\">Включёен</font>"));
}

void printVR(uint8_t *buf)
{
  Serial.println("VR Index\tGroup\tRecordNum\tSignature");

  Serial.print(buf[2], DEC);
  Serial.print("\t\t");

  if(buf[0] == 0xFF){
    Serial.print("NONE");
  }
  else if(buf[0]&0x80){
    Serial.print("UG ");
    Serial.print(buf[0]&(~0x80), DEC);
  }
  else{
    Serial.print("SG ");
    Serial.print(buf[0], DEC);
  }
  Serial.print("\t");

  Serial.print(buf[1], DEC);
  Serial.print("\t\t");
  if(buf[3]>0){
    printSignature(buf+4, buf[3]);
  }
  else{
    Serial.print("NONE");
  }
  Serial.println("\r\n");
}

void setup()
{
  /** initialize */
  myVR.begin(9600);
  
  Serial.begin(115200);

  // По умолчанию в Библиотеке "ethercard" (CS-pin) = № 8, то не пишем 10 после mymac
  // if (ether.begin(sizeof Ethernet::buffer, mymac) == 0).
  // Если меняем мин на другой то: Меняем (CS-pin) на 10.
  //if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0)

  if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0);

  if (!ether.dhcpSetup()); 

  // Выводим в Serial монитор IP адрес который нам автоматический присвоил наш Router. 
  // Динамический IP адрес, это не удобно, периодический наш IP адрес будет меняться. 
  // Нам придётся каждый раз узнавать кой адрес у нашей страницы.
  ether.printIp("My Router IP: ", ether.myip); // Выводим в Serial монитор IP адрес который нам присвоил Router. 

  // Здесь мы подменяем наш динамический IP на статический / постоянный IP Address нашей Web страницы.
  // Теперь не важно какой IP адрес присвоит нам Router, автоматический будем менять его, например на "192.168.1.222". 
  ether.staticSetup(myip);

  ether.printIp("My SET IP: ", ether.myip); // Выводим в Serial монитор статический IP адрес. 
  //-----
    //Изначально свет выключен при старте программы
     digitalWrite(led, LOW);
    pinMode(led,OUTPUT); 
    PinStatus = false; 

     if(myVR.clear() == 0){
    Serial.println("Recognizer cleared.");
  }else{
    Serial.println("Not find VoiceRecognitionModule.");
    Serial.println("Please check connection and restart Arduino.");
    while(1);
  }
  
  if(myVR.load((uint8_t)onRecord) >= 0){
    Serial.println("onRecord loaded");
  }
  
  if(myVR.load((uint8_t)offRecord) >= 0){
    Serial.println("offRecord loaded");
  }
}


void loop()
{

  delay(1); // Дёргаем микроконтроллер.

  word len = ether.packetReceive();   // check for ethernet packet / проверить ethernet пакеты.
  word pos = ether.packetLoop(len);   // check for tcp packet / проверить TCP пакеты.

  if (pos) {
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /", data, 5) != 0) {
      bfill.emit_p(http_Unauthorized);
    }
    else {
      data += 5;
      if (data[0] == ' ') {       
        homePage(); // Return home page Если обнаружено изменения на станице, запускаем функцию.
        digitalWrite(led ,PinStatus);
      }

      // "16" = количество символов "?ArduinoPIN1=on ".
      else if (strncmp("?ArduinoPIN1=on ", data, 16) == 0) {
        PinStatus = true;        
        bfill.emit_p(http_Found);
      }
      //------------------------------------------------------  


      else if (strncmp("?ArduinoPIN1=off ", data, 17) == 0) {
        PinStatus = false;        
        bfill.emit_p(http_Found);
      }
      //---------------------------


      else {
        // Page not found
        bfill.emit_p(http_Unauthorized);
      }
    }
    ether.httpServerReply(bfill.position());    // send http response
  }

  
  int ret;
  ret = myVR.recognize(buf, 50);
  if(ret>0){
    switch(buf[1]){
      case onRecord:
        /** turn on LED */
        digitalWrite(led, HIGH);
        PinStatus = true;
        break;
      case offRecord:
        /** turn off LED*/
        digitalWrite(led, LOW);
        PinStatus = false;
        break;
      default:
        Serial.println("Record function undefined");
        break;
    }
    /** voice recognized */
    printVR(buf);
  }

  
}
