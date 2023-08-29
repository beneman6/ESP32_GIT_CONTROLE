#include <Arduino.h>
#include <esp_dmx.h>
#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <Gotham-Medium36.h>
#include <Gotham-Medium30.h>
#include <Gotham-Light26.h>

#define DMX_TX_PIN 26
#define DIGITAL_MICOPHONE_PIN 17
#define BACKGROUND_COLOR 0x31A6
#define LARGE_FONT_36 GothamMedium36
#define LARGE_FONT_30 GothamMedium30
#define MEDIUM_FONT_26 GothamLight26

volatile boolean buttonPressed = false;
char *ausgabe = new char[128];

#define ROTARY_ENCODER_A_PIN 25
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 4
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

bool lastMicrophoneState = false;

volatile u_int16_t dmxAdress = 1;
volatile u_int8_t dmxValue = 0;

uint16_t width;
volatile bool einenWertSenden;
uint8_t tabelle[512];
volatile bool gedrueckt;
volatile uint16_t encoderWert;
volatile bool neueDatenZuSenden;
uint16_t kanal = 0;

// instead of changing here, rather change numbers above
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
const dmx_port_t dmx_num = DMX_NUM_2;
dmx_packet_t dmx_packet;
uint8_t data[DMX_PACKET_SIZE];
uint16_t data_size;
TFT_eSPI mainScreen;

xTaskHandle mainMenueTaskHandle;
xTaskHandle senderMenueTaskHandle;
xTaskHandle multipleSenderMenueTaskHandle;
xTaskHandle automaticDMXTaskHandle;
xTaskHandle dmxTaskHandle;

TFT_eSprite displaySprite(&mainScreen);

uint16_t altePosistion = 0;
uint16_t neuePosistion = 0;

xSemaphoreHandle encoderMutex;
xSemaphoreHandle buttonMutex;

void mainMenueTask(void *parameter);
void senderMenueTask(void *parameter);
void multipleSenderMenueTask(void *parameter);
void dmxTask(void *parameter);
void automaticDMXTask(void *parameter);

uint8_t buffer[513];
char *ptr;
uint8_t i = 0;

void resetTabelle()
{

  for (uint16_t i = 0; i < sizeof(tabelle); i++)
  {
    tabelle[i] = 0;
  }
}

void IRAM_ATTR readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}
void rotaryEncoderFunction(uint16_t steps, bool circle)
{
  steps--;
  rotaryEncoder.setBoundaries(0, steps, circle);
  if (rotaryEncoder.encoderChanged())
  {
    Serial.print("Value: ");
    Serial.println(rotaryEncoder.readEncoder());
    encoderWert = rotaryEncoder.readEncoder();
  }
  if (rotaryEncoder.isEncoderButtonClicked())
  {
    static unsigned long lastTimePressed = 0; // Soft debouncing
    if (!(millis() - lastTimePressed < 500))
    {

      gedrueckt = true;
      lastTimePressed = millis();
      Serial.print("button pressed ");
      Serial.print(millis());

      Serial.println(" milliseconds after restart");
    }
  }
  else
  {
    gedrueckt = false;
  }
}

void drawItem(int32_t x, int32_t y, int32_t r, int32_t ir, int32_t w, int32_t h, uint32_t fg_color, uint32_t bg_color, const char *text)
{
  displaySprite.fillSmoothRoundRect(x, y, w, h, r, bg_color);
  displaySprite.drawSmoothRoundRect(x, y, r, ir, w, h, fg_color);
  displaySprite.drawString(text, (x + 5), (y + 5));
  if (bg_color != TFT_BLACK)
  {
    displaySprite.fillTriangle((x - 11), y, (x - 11), (y + h), (x - 3), (y + (h / 2.0)), bg_color);
  }
}
void drawMainMenue(uint8_t selected)
{
  displaySprite.unloadFont();
  displaySprite.fillSprite(BACKGROUND_COLOR);
  displaySprite.loadFont(LARGE_FONT_36);
  displaySprite.drawString("Hauptmenü", 5, 5);
  displaySprite.unloadFont();
  displaySprite.loadFont(MEDIUM_FONT_26);

  switch (selected)
  {
  case 0:
  {
    drawItem(20, 50, 4, 3, 280, 35, TFT_WHITE, TFT_BLUE, "Einen Wert Senden");
    drawItem(20, 95, 4, 3, 280, 35, TFT_WHITE, TFT_BLACK, "Auto Modus");
    drawItem(20, 140, 4, 3, 280, 35, TFT_WHITE, TFT_BLACK, "Viele Werte Senden");

    break;
  }
  case 1:
  {
    drawItem(20, 50, 4, 3, 280, 35, TFT_WHITE, TFT_BLACK, "Einen Wert Senden");
    drawItem(20, 95, 4, 3, 280, 35, TFT_WHITE, TFT_BLUE, "Auto Modus");
    drawItem(20, 140, 4, 3, 280, 35, TFT_WHITE, TFT_BLACK, "Viele Werte Senden");
    break;
  }
  case 2:
  {
    drawItem(20, 50, 4, 3, 280, 35, TFT_WHITE, TFT_BLACK, "Einen Wert Senden");
    drawItem(20, 95, 4, 3, 280, 35, TFT_WHITE, TFT_BLACK, "Auto Modus");
    drawItem(20, 140, 4, 3, 280, 35, TFT_WHITE, TFT_BLUE, "Viele Werte Senden");
    break;
  }
  }

  displaySprite.pushSprite(0, 0);
}

void setup()
{

  pinMode(DIGITAL_MICOPHONE_PIN, INPUT);
  Serial.begin(115200);
  Serial.printf("Serial Receiver Initialized\n");
  for (uint16_t i = 0; i < 513; i++)
  {
    buffer[i] = 0;
  }

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);

  rotaryEncoder.setBoundaries(0, 2, false); // minValue, maxValue, circleValues true|false (when max go to min and vice versa)

  /*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
  rotaryEncoder.disableAcceleration();
  encoderMutex = xSemaphoreCreateMutex();
  buttonMutex = xSemaphoreCreateMutex();

  dmx_set_pin(dmx_num, DMX_TX_PIN, DMX_PIN_NO_CHANGE, DMX_PIN_NO_CHANGE);

  // ...use the default DMX configuration...
  dmx_config_t config = DMX_CONFIG_DEFAULT;

  // ...and then install the driver!
  Serial.printf("Hat es geklappt: %d\n", dmx_driver_install(dmx_num, &config, DMX_INTR_FLAGS_DEFAULT));
  // put your setup code here, to run once:

  mainScreen.begin();
  mainScreen.setRotation(3);
  mainScreen.fillScreen(BACKGROUND_COLOR);
  mainScreen.setTextSize(1);
  mainScreen.setSwapBytes(true);

  displaySprite.createSprite(320, 240);
  displaySprite.loadFont(LARGE_FONT_36);

  width = displaySprite.textWidth("DMX Controller");
  uint8_t pos = (320 - width) / 2;
  displaySprite.drawString("DMX Controller", pos, 100);
  displaySprite.pushSprite(0, 0);

  delay(pdMS_TO_TICKS(4000));

  dmx_write_slot(dmx_num, 0, 0x00);
  dmx_write_slot(dmx_num, 1, 255);
  dmx_send(dmx_num, DMX_PACKET_SIZE);

  delay(pdMS_TO_TICKS(4000));

  xTaskCreate(mainMenueTask, "Main Menue Task", 2048, NULL, 2, &mainMenueTaskHandle);
  xTaskCreate(senderMenueTask, "Sender Menue Task", 2048, NULL, 2, &senderMenueTaskHandle);
  xTaskCreate(multipleSenderMenueTask, "Multiple Sender Menue Task", 2048, NULL, 2, &multipleSenderMenueTaskHandle);
  xTaskCreate(dmxTask, "DMX Task", 2048, NULL, 2, &dmxTaskHandle);
  xTaskCreate(automaticDMXTask, "Automatic DMX Task", 2048, NULL, 2, &automaticDMXTaskHandle);
}

void loop()
{
}

void mainMenueTask(void *parameter)
{

  for (;;)
  {
    rotaryEncoderFunction(3, false);

    if (gedrueckt)
    {
      gedrueckt = false;
      switch (encoderWert)
      {
      case 0:
      {
        vTaskResume(senderMenueTaskHandle);
        vTaskSuspend(NULL);
        break;
      }
      case 1:
      {
        vTaskResume(automaticDMXTaskHandle);
        vTaskSuspend(NULL);
        break;
      }
      case 2:
      {
        resetTabelle();
        vTaskResume(multipleSenderMenueTaskHandle);
        vTaskSuspend(NULL);
        break;
      }
      }
    }

    drawMainMenue(encoderWert);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
void senderMenueTask(void *parameter)
{
  vTaskSuspend(NULL);
  for (;;)
  {
    displaySprite.unloadFont();
    displaySprite.fillSprite(BACKGROUND_COLOR);
    displaySprite.loadFont(LARGE_FONT_30);
    displaySprite.drawString("DMX Senden", 4, 4);
    displaySprite.unloadFont();
    displaySprite.loadFont(MEDIUM_FONT_26);
    displaySprite.drawString("DMX Wert auswählen", 8, 40);
    displaySprite.drawSmoothRoundRect(20, 70, 5, 3, 80, 40, TFT_WHITE);
    displaySprite.setCursor(35, 80);
    displaySprite.printf("%3d", dmxValue);
    displaySprite.drawString("DMX Adresse auswählen", 8, 119);
    displaySprite.drawSmoothRoundRect(20, 145, 5, 3, 80, 40, TFT_WHITE);
    displaySprite.setCursor(35, 155);
    displaySprite.printf("%3d", dmxAdress);
    displaySprite.drawSmoothRoundRect(35, 203, 4, 2, 110, 35, TFT_WHITE);
    displaySprite.drawString("Senden", 43, 210);
    displaySprite.drawSmoothRoundRect(175, 203, 4, 2, 110, 35, TFT_WHITE);
    displaySprite.drawString("Zurück", 185, 210);

    rotaryEncoderFunction(4, false);

    switch (encoderWert)
    {
    case 0:
    {
      displaySprite.drawSmoothRoundRect(20, 70, 5, 3, 80, 40, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        gedrueckt = false;
        displaySprite.setCursor(35, 80);
        displaySprite.drawSmoothRoundRect(20, 70, 5, 3, 80, 40, TFT_ORANGE);
        displaySprite.pushSprite(0, 0);
        rotaryEncoderFunction(256, true);
        encoderWert = dmxValue;
        rotaryEncoder.setEncoderValue(dmxValue);

        do
        {
          vTaskDelay(pdMS_TO_TICKS(50));
          dmxValue = encoderWert;
          displaySprite.fillRect(38, 75, 50, 30, BACKGROUND_COLOR);
          displaySprite.setCursor(35, 80);
          displaySprite.printf("%3d", dmxValue);
          displaySprite.pushSprite(0, 0);
          rotaryEncoderFunction(256, true);
        } while (!gedrueckt);
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
      }

      break;
    }
    case 1:
    {
      displaySprite.drawSmoothRoundRect(20, 145, 5, 3, 80, 40, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        gedrueckt = false;
        displaySprite.setCursor(35, 155);
        displaySprite.drawSmoothRoundRect(20, 145, 5, 3, 80, 40, TFT_ORANGE);
        displaySprite.pushSprite(0, 0);
        rotaryEncoderFunction(512, true);
        encoderWert = (dmxAdress - 1);
        rotaryEncoder.setEncoderValue((dmxAdress - 1));
        do
        {
          vTaskDelay(pdMS_TO_TICKS(50));
          dmxAdress = (encoderWert + 1);
          displaySprite.fillRect(38, 150, 50, 30, BACKGROUND_COLOR);
          displaySprite.setCursor(35, 155);
          displaySprite.printf("%3d", dmxAdress);
          displaySprite.pushSprite(0, 0);
          rotaryEncoderFunction(512, true);
        } while (!gedrueckt);
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
      }
      break;
    }
    case 2:
    {
      displaySprite.drawSmoothRoundRect(35, 203, 4, 2, 110, 35, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        // senden
        Serial.printf("DMX Value: %d, DMX Adresse: %d\n", dmxValue, dmxAdress);
        neueDatenZuSenden = true;
        einenWertSenden = true;
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
        vTaskResume(mainMenueTaskHandle);
        vTaskSuspend(NULL);
      }

      break;
    }
    case 3:
    {
      displaySprite.drawSmoothRoundRect(175, 203, 4, 2, 110, 35, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
        vTaskResume(mainMenueTaskHandle);
        vTaskSuspend(NULL);
      }

      break;
    }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
void dmxTask(void *parameter)
{

  for (;;)
  {
    while (!neueDatenZuSenden)
    {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    Serial.printf("Es werden neue Daten gesendet\n");
    neueDatenZuSenden = false;
    dmx_wait_sent(dmx_num, portMAX_DELAY);
    dmx_write(dmx_num, buffer, DMX_PACKET_SIZE);
    dmx_write_slot(dmx_num, 0, 0x00);
    if (einenWertSenden)
    {
      Serial.printf("Es ein Wert gesendet: %d\n", dmxValue);
      dmx_write_slot(dmx_num, dmxAdress, dmxValue);
      einenWertSenden = false;
    }
    else
    {
      Serial.printf("Es werden mehrere Werte gesendet\n");
      dmx_write_offset(dmx_num, 1, tabelle, 512);
    }

    dmx_send(dmx_num, DMX_PACKET_SIZE);
  }
}
void automaticDMXTask(void *parameter)
{
  vTaskSuspend(NULL);
  for (;;)
  {

    displaySprite.fillSprite(TFT_BLACK);
    displaySprite.loadFont(LARGE_FONT_30);
    displaySprite.drawString("Auto Modus", 4, 4);
    displaySprite.unloadFont();
    displaySprite.loadFont(MEDIUM_FONT_26);
    displaySprite.drawSmoothRoundRect(175, 203, 4, 2, 110, 35, TFT_WHITE);
    displaySprite.drawString("Zurück", 185, 210);
    displaySprite.unloadFont();
    rotaryEncoderFunction(1, false);
    if (gedrueckt)
    {
      vTaskResume(mainMenueTaskHandle);
      vTaskSuspend(NULL);
    }
    if (digitalRead(DIGITAL_MICOPHONE_PIN) == HIGH)
    {

      static unsigned long lastTimeActivated = 0; // Soft debouncing
      if (!(millis() - lastTimeActivated < 100))
      {
        
        
        dmx_write_slot(dmx_num, 0, 0x00);
        dmx_write_slot(dmx_num, 1, 255);
      }
      else{
        
        
        dmx_write_slot(dmx_num, 0, 0x00);
        dmx_write_slot(dmx_num, 1, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
void multipleSenderMenueTask(void *parameter)
{
  vTaskSuspend(NULL);
  for (;;)
  {
    displaySprite.unloadFont();
    displaySprite.fillSprite(BACKGROUND_COLOR);
    displaySprite.loadFont(LARGE_FONT_30);
    displaySprite.drawString("DMX Senden", 4, 4);
    displaySprite.unloadFont();
    displaySprite.loadFont(MEDIUM_FONT_26);
    displaySprite.drawString("DMX Wert auswählen", 8, 40);
    displaySprite.drawSmoothRoundRect(20, 70, 5, 3, 80, 40, TFT_WHITE);
    displaySprite.setCursor(35, 80);
    displaySprite.printf("%3d", tabelle[kanal]);
    displaySprite.drawString("DMX Adresse auswählen", 8, 119);
    displaySprite.drawSmoothRoundRect(20, 145, 5, 3, 80, 40, TFT_WHITE);
    displaySprite.setCursor(35, 155);
    displaySprite.printf("%3d", (kanal + 1));
    displaySprite.drawSmoothRoundRect(35, 203, 4, 2, 110, 35, TFT_WHITE);
    displaySprite.drawString("Senden", 43, 210);
    displaySprite.drawSmoothRoundRect(175, 203, 4, 2, 110, 35, TFT_WHITE);
    displaySprite.drawString("Zurück", 185, 210);

    rotaryEncoderFunction(4, false);

    switch (encoderWert)
    {
    case 0:
    {
      displaySprite.drawSmoothRoundRect(20, 70, 5, 3, 80, 40, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        gedrueckt = false;
        displaySprite.setCursor(35, 80);
        displaySprite.drawSmoothRoundRect(20, 70, 5, 3, 80, 40, TFT_ORANGE);
        displaySprite.pushSprite(0, 0);
        rotaryEncoderFunction(256, true);

        do
        {
          vTaskDelay(pdMS_TO_TICKS(50));
          tabelle[kanal] = encoderWert;
          displaySprite.fillRect(38, 75, 50, 30, BACKGROUND_COLOR);
          displaySprite.setCursor(35, 80);
          displaySprite.printf("%3d", tabelle[kanal]);
          displaySprite.pushSprite(0, 0);
          rotaryEncoderFunction(256, true);
        } while (!gedrueckt);
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
      }

      break;
    }
    case 1:
    {
      displaySprite.drawSmoothRoundRect(20, 145, 5, 3, 80, 40, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        gedrueckt = false;
        displaySprite.setCursor(35, 155);
        displaySprite.drawSmoothRoundRect(20, 145, 5, 3, 80, 40, TFT_ORANGE);
        displaySprite.pushSprite(0, 0);
        rotaryEncoderFunction(512, true);
        encoderWert = (kanal);
        rotaryEncoder.setEncoderValue(kanal);
        do
        {
          vTaskDelay(pdMS_TO_TICKS(50));
          tabelle[kanal] = encoderWert;
          displaySprite.fillRect(38, 150, 50, 30, BACKGROUND_COLOR);
          displaySprite.setCursor(35, 155);
          displaySprite.printf("%3d", (kanal + 1));
          displaySprite.pushSprite(0, 0);
          rotaryEncoderFunction(512, true);
        } while (!gedrueckt);
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
      }
      break;
    }
    case 2:
    {
      displaySprite.drawSmoothRoundRect(35, 203, 4, 2, 110, 35, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        // senden
        Serial.printf("DMX Value: %d, DMX Adresse: %d\n", dmxValue, dmxAdress);
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
        vTaskResume(mainMenueTaskHandle);
        vTaskSuspend(NULL);
      }

      break;
    }
    case 3:
    {
      displaySprite.drawSmoothRoundRect(175, 203, 4, 2, 110, 35, TFT_BLUE);
      displaySprite.pushSprite(0, 0);
      if (gedrueckt)
      {
        encoderWert = 0;
        rotaryEncoder.setEncoderValue(0);
        vTaskResume(mainMenueTaskHandle);
        vTaskSuspend(NULL);
      }

      break;
    }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}