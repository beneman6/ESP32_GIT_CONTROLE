#include <Arduino.h>
#include <esp_dmx.h>

#define DMX_RX_PIN 32
#define DMX_INPUT_PIN 32

const dmx_port_t dmx_num = DMX_NUM_2;
dmx_packet_t dmx_packet;
uint8_t data[DMX_PACKET_SIZE];
uint16_t data_size;



void dmxReceived();
char* buffer = new char[256];
char* ptr;
uint8_t i = 0;

void setup() {
  dmx_set_pin(dmx_num,DMX_PIN_NO_CHANGE,DMX_RX_PIN,DMX_PIN_NO_CHANGE);

  // ...use the default DMX configuration...
  dmx_config_t config = DMX_CONFIG_DEFAULT;

  // ...and then install the driver!
  dmx_driver_install(dmx_num, &config, DMX_INTR_FLAGS_DEFAULT);
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  Serial.printf("Serial Receiver Initialized\n");
  
}

void loop() {
  data_size = dmx_receive(dmx_num,&dmx_packet,DMX_TIMEOUT_TICK);
  
  Serial.printf("Groesse des Packets: %d\n",data_size);
  //if(dmx_packet.err == ESP_OK){
    if(data_size > 0){
    dmx_read(dmx_num,&data,dmx_packet.size);
    for(uint16_t i = 0; i<DMX_PACKET_SIZE;i++){
      if(data[i] != 0)
      Serial.printf("Inhalt des Packets an Stelle %d: %d\n",i,data[i]);
    }
    
  //}
  //else{
   // Serial.printf("Es gab einen Fehler\n");
  //}
    }
  //delay(pdMS_TO_TICKS(1000));
}


void dmxReceived(){

  
}

