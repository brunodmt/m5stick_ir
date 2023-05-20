
#include <M5StickCPlus.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLEUtils.h>
#include <BLEUUID.h>
#include <BLE2902.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

// General constants
const uint32_t kDelayMs = 100;

// IR Constants
const uint16_t kIrRecvPin = 33; // IR Receiver Pin - M5StickC Plus integrated
const uint16_t kIrSendPin = 9;  // IR Emitter Pin - M5 IR Unit

const uint64_t kIrCodePower = 0x8E7629D;
const uint64_t KIrCodeVolDn = 0x8E7E21D;
const uint64_t KIrCodeVolUp = 0x8E7609F;

const uint64_t kIrRcvCodeVolDn = 0x8E7807F;
const uint64_t kIrRcvCodeVolUp = 0x8E700FF;

// Bluetooth Constants
const uint32_t kBleRestartDelayMs = 500;
const BLEUUID kGattServiceUuid = BLEUUID("6f404fdb-0000-4451-b2e5-907610d7a798");
const BLEUUID kGattCharactUsbVUuid = BLEUUID("6f404fdb-0001-4451-b2e5-907610d7a798");
const BLEUUID kGattCharactSendUuid = BLEUUID("6f404fdb-0002-4451-b2e5-907610d7a798");

// Power constants
const float kUsbVThresh = 100;

// IR Vars
IRrecv irRecv(kIrRecvPin);
IRsend irSend(kIrSendPin);
decode_results irResults;
bool irCmdAvailable = false;

// Bluetooth Vars
BLEServer *pBleServer = NULL;
BLEAdvertising *pBleAdvertising = NULL;
BLECharacteristic *pCharacteristicUsbV = NULL;
BLECharacteristic *pCharacteristicSend = NULL;
bool bleConn = false;
bool bleConnPrev = false;
RTC_DATA_ATTR static uint8_t bleSeq;

// Power Vars
uint16_t usbV = 0; // Voltage

// Bluetooth Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConn = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      bleConn = false;
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // Trigger a power code, regardless of the value written
      irSend.sendNEC(kIrCodePower);
    }
};

void setAdvData(BLEAdvertising *pAdvertising) {
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

    oAdvertisementData.setFlags(0x06); // BR_EDR_NOT_SUPPORTED | General Discoverable Mode

    std::string strServiceData = "";
    strServiceData += (char)0x06; // Length(6Byte)
    strServiceData += (char)0xff; // AD Type 0xFF: Manufacturer specific data
    strServiceData += (char)0xff; // Test manufacture ID low byte
    strServiceData += (char)0xff; // Test manufacture ID high byte
    strServiceData += (char)bleSeq; // sequence number
    strServiceData += (char)(usbV & 0xff); // Lower byte of Voltage
    strServiceData += (char)((usbV >> 8) & 0xff); // Upper byte of Voltage

    oAdvertisementData.addData(strServiceData);
    pAdvertising->setAdvertisementData(oAdvertisementData);
}

void setup() {
  M5.begin(true, true, true);
  M5.Axp.SetLDO2(false); // Turn off the screen (just the backlight...)

  irSend.begin();
  irRecv.enableIRIn();

  // Send a code when turning on, as this will be triggered by the TV turning on
  irSend.sendNEC(kIrCodePower);

  BLEDevice::init("m5-ir-remote");
  pBleServer = BLEDevice::createServer();  // Create a server
  pBleServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pBleServer->createService(kGattServiceUuid);

  pCharacteristicUsbV = pService->createCharacteristic(kGattCharactUsbVUuid,
                                                       BLECharacteristic::PROPERTY_READ
                                                       | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicUsbV->addDescriptor(new BLE2902()); // Needed for automatic notify

  pCharacteristicSend = pService->createCharacteristic(kGattCharactSendUuid, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicSend->setCallbacks(new MyCharacteristicCallbacks()); // Needed to listen "writes"

  pService->start();
  pBleAdvertising = pBleServer->getAdvertising();
  pBleAdvertising->addServiceUUID(kGattServiceUuid);
  setAdvData(pBleAdvertising);
  pBleAdvertising->start();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasReleased()) {
    irSend.sendNEC(kIrCodePower);
  }

  irCmdAvailable = irRecv.decode(&irResults);
  if (irCmdAvailable) {
    switch (irResults.value) {
      case kIrRcvCodeVolDn:
        irSend.sendNEC(KIrCodeVolDn);
        break;
      case kIrRcvCodeVolUp:
        irSend.sendNEC(KIrCodeVolUp);
        break;
      default:
        break;
    }
    irRecv.resume();
  }

  usbV = (uint16_t)(M5.Axp.GetVBusVoltage() * 100);

  if (bleConn) {
      pCharacteristicUsbV->setValue(usbV);
      pCharacteristicUsbV->notify();
  } else {
    setAdvData(pBleAdvertising);
    if (bleConnPrev) {
      delay(kBleRestartDelayMs);
      pBleServer->startAdvertising();
    }
  }

  bleConnPrev = bleConn;

  if (usbV < kUsbVThresh) {
    irSend.sendNEC(kIrCodePower);
    delay(kDelayMs);
    M5.Axp.PowerOff();
  }

  delay(kDelayMs);
}
