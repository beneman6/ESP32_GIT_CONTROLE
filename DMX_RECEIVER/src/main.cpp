#include <Arduino.h>
#include <esp_dmx.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "ESP32-AP";
const char *password = "123456789";

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

String header;
String response;

#define DMX_RX_PIN 32
#define DMX_INPUT_PIN 32

const dmx_port_t dmx_num = DMX_NUM_2;
dmx_packet_t dmx_packet;
uint8_t data[DMX_PACKET_SIZE];
volatile uint16_t data_size;
volatile uint8_t dataBufferTwo[DMX_PACKET_SIZE];

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char *buffer = new char[256];
char *ptr;
uint8_t i = 0;

xTaskHandle dmxTaskHandle;
xTaskHandle wifiTaskHandle;

void dmxTask(void *parameter);
void wifiTask(void *parameter);

void onConnect();

void setup()
{

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  delay(pdMS_TO_TICKS(100));

  Wire.setPins(25, 26);
  Wire.begin();

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.setTextWrap(true);
  display.clearDisplay();
  display.fillScreen(BLACK);
  display.setTextColor(WHITE);
  display.setCursor(20, 10);
  display.printf("DMX Controller");
  display.display();
  dmx_set_pin(dmx_num, DMX_PIN_NO_CHANGE, DMX_RX_PIN, DMX_PIN_NO_CHANGE);

  // ...use the default DMX configuration...
  dmx_config_t config = DMX_CONFIG_DEFAULT;

  // ...and then install the driver!
  dmx_driver_install(dmx_num, &config, DMX_INTR_FLAGS_DEFAULT);
  // put your setup code here, to run once:

  Serial.begin(115200);
  Serial.printf("Serial Receiver Initialized\n");
  server.on("/", onConnect);

  server.begin();

  delay(pdMS_TO_TICKS(2000));

  // xTaskCreate(wifiTask, "WiFi Task", 2048, NULL, 2, &wifiTaskHandle);
  xTaskCreate(dmxTask, "DMX Task", 2048, NULL, 2, &dmxTaskHandle);
}

void loop()
{
  server.handleClient();
  delay(pdMS_TO_TICKS(10));
}

void dmxTask(void *parameter)
{
  for (;;)
  {
    data_size = dmx_receive(dmx_num, &dmx_packet, portMAX_DELAY);

    Serial.printf("Groesse des Packets: %d\n", data_size);
    display.setCursor(0, 0);
    display.fillScreen(BLACK);
    display.printf("Packetgroesse: %d\n", data_size);
    display.display();
    delay(pdMS_TO_TICKS(1500));
    // if(dmx_packet.err == ESP_OK){
    if (data_size > 0)
    {
      dmx_read(dmx_num, &data, dmx_packet.size);
      for (uint16_t i = 0; i < DMX_PACKET_SIZE; i++)
      {
        dataBufferTwo[i] = data[i];
        if (data[i] != 0)
        {
          Serial.printf("Inhalt des Packets an Stelle %d: %d\n", i, data[i]);
          display.setCursor(0, 0);
          display.fillScreen(BLACK);
          display.printf("Inhalt des Packets\nan Stelle %d: %d\n", i, data[i]);
          display.display();
          delay(pdMS_TO_TICKS(1000));
        }
      }

      //}
      // else{
      // Serial.printf("Es gab einen Fehler\n");
      //}
    }
  }
}
void wifiTask(void *parameter)
{
  for (;;)
  {
  }
}
void onConnect()
{

  response += "<!DOCTYPE html> <html>\n";
  response += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  response += "<title>DMX</title>\n";
  response += "<style> html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;} </style></head>\n";
  response += "<body><header><h1>DMX Receiver</h1></header>\n";
  response += "<h2>Tabelle der letzeten DMX Nachricht</h2>\n";

  if (data_size > 0)
  {
    response += "<ol>\n";
    for (uint16_t i = 0; i < DMX_PACKET_SIZE; i++)
    {
      response += "<li>";
      response += dataBufferTwo[i];
      response += "</li>\n";
    }
    response += "</ol>\n";
  }
  else
  {
    response += "<p>Es wurde noch kein Packet empfangen </p>\n";
  }
  response += "</body>\n";
  response += "</html>\n";

  server.send(200, "text/html", response);
}