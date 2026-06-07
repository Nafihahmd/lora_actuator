#include <Arduino.h>
#include <RadioLib.h>
#include <HardwareSerial.h>
#include <SPI.h>

#include "protocol.h"

HardwareSerial Serial(PA10, PA9);

// ------------------------------------------------------------
// SX1262 wiring
// ------------------------------------------------------------
// static const uint8_t PIN_NSS  = PB8;
// static const uint8_t PIN_BUSY = PA8;
// static const uint8_t PIN_RST  = PB7;
// static const uint8_t PIN_DIO1 = PA7;
// SPI1
#define LORA_SCK  PB3
#define LORA_MISO PB4
#define LORA_MOSI PB5

// LoRa Actuator Pin Definitions
// #define LORA_NSS  PB8
// #define LORA_BUSY PA8
// #define LORA_DIO1 PA7 
// #define LORA_RST  PB7

// LoRa Controller Pin Definitions
#define LORA_NSS  PB0
#define LORA_BUSY PB1
#define LORA_DIO1 PA0   // Shared with Wake Pin
#define LORA_RST  PB7


// Create LoRa instance using RadioLib (CS, IRQ/DIO1, RST, BUSY)
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Change as needed
static const uint8_t ACT_LED = PC14;
// ------------------------------------------------------------
// Receive flag
// ------------------------------------------------------------
volatile bool receivedFlag = false;

void setFlag(void)
{
    receivedFlag = true;
}

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------
void setup()
{
    pinMode(ACT_LED, OUTPUT);
    digitalWrite(ACT_LED, LOW);

    Serial.begin(115200);

    Serial.println();
    Serial.println("Actuator starting");
    SPI.setMOSI(LORA_MOSI);
    SPI.setMISO(LORA_MISO);
    SPI.setSCLK(LORA_SCK);
    SPI.begin();

    ConfigLoRa_t config;
    config.frequency = 865.0;

    int state = radio.begin(865.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0, false);

    if(state != RADIOLIB_ERR_NONE)
    {
        Serial.print("Radio init failed: ");
        Serial.println(state);

        while(1)
        {
            delay(1000);
        }
    }

    Serial.println("Radio init OK");

    radio.setPacketReceivedAction(setFlag);

    state = radio.startReceive();

    if(state != RADIOLIB_ERR_NONE)
    {
        Serial.print("startReceive failed: ");
        Serial.println(state);

        while(1)
        {
            delay(1000);
        }
    }

    Serial.println("Listening...");
}

// ------------------------------------------------------------
// Loop
// ------------------------------------------------------------
void loop()
{
    if(!receivedFlag)
    {
        return;
    }

    receivedFlag = false;

    uint8_t rxBuf[32];

    int packetLength =
        radio.getPacketLength();

    int state =
        radio.readData(
            rxBuf,
            packetLength);

    if(state != RADIOLIB_ERR_NONE)
    {
        Serial.print("RX error: ");
        Serial.println(state);

        radio.startReceive();
        return;
    }

    Serial.println("Packet received");

    if(packetLength != sizeof(CmdFrame))
    {
        Serial.print("Unexpected length: ");
        Serial.println(packetLength);

        radio.startReceive();
        return;
    }

    CmdFrame* frame =
        (CmdFrame*)rxBuf;

    if(frame->header.netId != PROTO_NET_ID)
    {
        Serial.println("Bad NetID");

        radio.startReceive();
        return;
    }

    if(frame->payload.type != FRAME_CMD)
    {
        Serial.println("Not CMD");

        radio.startReceive();
        return;
    }

    uint16_t crc =
        protocolCrc16(
            (uint8_t*)&frame->payload,
            sizeof(CmdPayload) - sizeof(uint16_t));

    if(crc != frame->payload.crc16)
    {
        Serial.println("CRC mismatch");

        radio.startReceive();
        return;
    }

    Serial.print("Counter: ");
    Serial.println(frame->payload.counter);

    Serial.print("Channel: ");
    Serial.println(frame->payload.channel);

    Serial.print("Command: ");
    Serial.println(frame->payload.command);

    if(frame->payload.command == CMD_TRIGGER)
    {
        digitalWrite(ACT_LED, HIGH);

        Serial.println("TRIGGER RECEIVED");
    }
    else if(frame->payload.command == CMD_STATUS)
    {
        Serial.println("STATUS REQUEST RECEIVED");
    }

    Serial.print("RSSI: ");
    Serial.println(radio.getRSSI());

    Serial.print("SNR: ");
    Serial.println(radio.getSNR());

    radio.startReceive();
}