#include <Arduino.h>
#include <esp_dmx.h>
#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>

#define DMX_TX_PIN 32

boolean buttonPressed = false;
uint16_t displayBuffer[2];
char *ausgabe = new char[128];

#define ROTARY_ENCODER_A_PIN 16
#define ROTARY_ENCODER_B_PIN 17
#define ROTARY_ENCODER_BUTTON_PIN 4
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// instead of changing here, rather change numbers above
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
const dmx_port_t dmx_num = DMX_NUM_2;
dmx_packet_t dmx_packet;
uint8_t data[DMX_PACKET_SIZE];
uint16_t data_size;
TFT_eSPI mainScreen;

xTaskHandle displayTaskHandle;

uint16_t altePosistion = 0;
uint16_t neuePosistion = 0;

xSemaphoreHandle encoderMutex;
xSemaphoreHandle buttonMutex;

void displayTask(void *parameter);
void pressureInterrupt();

void dmxReceived();
char *buffer = new char[256];
char *ptr;
uint8_t i = 0;

void IRAM_ATTR readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}

void setup()
{
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  bool circleValues = false;
  rotaryEncoder.setBoundaries(0, 19, circleValues); // minValue, maxValue, circleValues true|false (when max go to min and vice versa)

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
  dmx_driver_install(dmx_num, &config, DMX_INTR_FLAGS_DEFAULT);
  // put your setup code here, to run once:

  mainScreen.begin();
  mainScreen.setRotation(3);
  mainScreen.fillScreen(TFT_BLACK);
  mainScreen.setFreeFont(4);
  mainScreen.setTextSize(14);

  Serial.begin(115200);
  Serial.printf("Serial Receiver Initialized\n");

  xTaskCreate(displayTask, "Display Task", 2048, NULL, 2, &displayTaskHandle);
}

void loop()
{
}

void dmxReceived()
{
}
void displayTask(void *parameter)
{

  for (;;)
  {
    if (rotaryEncoder.encoderChanged())
    {
      Serial.print("Value: ");
      Serial.println(rotaryEncoder.readEncoder());
      displayBuffer[0] = rotaryEncoder.readEncoder();
    }
    if (rotaryEncoder.isEncoderButtonClicked())
    {
      static unsigned long lastTimePressed = 0; // Soft debouncing
      if (millis() - lastTimePressed < 500)
      {
        displayBuffer[1] = 0;
      }
      else
      {
        displayBuffer[1] = 1;
        lastTimePressed = millis();
        Serial.print("button pressed ");
        Serial.print(millis());
        Serial.println(" milliseconds after restart");
      }
    }
    sprintf(ausgabe, "Aktueller Wert: %d\nKnopf gedrueckt: %d", displayBuffer[0], displayBuffer[1]);
    mainScreen.drawString(ausgabe, 10, 10);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void pressureInterrupt()
{
  xSemaphoreTakeFromISR(buttonMutex, NULL);
  buttonPressed = true;
  xSemaphoreGiveFromISR(buttonMutex, NULL);
}
