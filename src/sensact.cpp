#include "sensact.h"

Sensact::Sensact() {
    this->_wire = NULL;
}

Sensact::Sensact(int sda, int scl, int bus) {
    this->_bus = bus;
    this->_scl = scl;
    this->_sda = sda;

    this->_wire = new TwoWire(bus);
}

bool Sensact::init() {    
    if ( this->_wire == NULL ) {
#ifdef DEBUG
        Serial.println("[Sensact] Initialized without I2C support");
#endif
        return true;
    }

    if (this->_wire->begin(TAP_I2C_SDA, TAP_I2C_SCL) ) {
#ifdef DEBUG
        Serial.println("[Sensact] Succesfully initialized I2C bus");
#endif 
        return true;
    } else {
#ifdef DEBUG
        Serial.println("[Sensact] Failure initializing I2C bus");
#endif 
        return false;
    }
}

bool Sensact::initServo(int pin) {
    if ( this->i2c_tap_servo != NULL ) {
#ifdef DEBUG
        Serial.println("[Sensact] Servo already initialized on I2C bus");
#endif 
        return false;
    }

    if ( this->tap_servo != NULL ) {
#ifdef DEBUG
        Serial.println("[Sensact] Servo already initialized");
#endif 
        return false;
    }

    this->tap_servo = new Servo();

    if ( this->tap_servo->attach(pin) == 0 ) {
#ifdef DEBUG
        Serial.println("[Sensact] Servo attached");
#endif 
        return true;
    } 
    
    // destroy tap servo and return
    delete this->tap_servo;
    this->tap_servo = NULL;
#ifdef DEBUG
    Serial.println("[Sensact] Servo attach failure");
#endif 
    return false;
}

bool Sensact::scanI2CAddress(int address) {
    if ( this->_wire == NULL ) {
#ifdef DEBUG
        Serial.println("[Sensact] I2C bus not initialized");
#endif
        return false;
    }

    this->_wire->beginTransmission(address);
    delay(2);
    int error = this->_wire->endTransmission();
    switch ( error ) {
        case 0:
#ifdef DEBUG
            Serial.println("[Sensact] device detected on bus");
#endif
            return true;
        case 2:
#ifdef DEBUG
            Serial.println("[Sensact] device not detected on bus");
#endif
            return false;
        default:
#ifdef DEBUG
            Serial.println("[Sensact] I2C bus error");
#endif
            return false;
    }
}

bool Sensact::initServo(int address, int pin) {
    if ( this->i2c_tap_servo != NULL ) {
#ifdef DEBUG
        Serial.println("[Sensact] servo already initialized");
#endif
        return false;
    }

    if ( ! this->scanI2CAddress(address) ) {
#ifdef DEBUG
        Serial.println("[Sensact] tap servo not found on bus");
#endif
        return false;
    }
    
    this->i2c_tap_servo = new I2CServo(this->_wire,address);
    
    if ( this->i2c_tap_servo->attach(pin) ) {
#ifdef DEBUG
        Serial.println("[Sensact] servo attached");
#endif
        return true;
    }

    // on failure, delete servo
#ifdef DEBUG
    Serial.println("[Sensact] Failed to initialize tap servo on I2C");
#endif
    delete this->i2c_tap_servo;
    this->i2c_tap_servo = NULL;
    return false;
}    


bool Sensact::initNFC() {
    if ( this->pn532 != NULL ) {
#ifdef DEBUG
        Serial.println("[Sensact] NFC already initialized");
#endif
        return false;
    }

    if ( ! this->scanI2CAddress(PN532_I2C_ADDRESS) ) {
#ifdef DEBUG
        Serial.println("[Sensact] PN532 not found on bus");
#endif
        return false;
    }

    this->pn532 = new Adafruit_PN532(-1,-1,this->_wire);
    if ( ! this->pn532->begin() ) {
#ifdef DEBUG
        Serial.println("[Sensact] Begin PN532 returned an error");
#endif

        delete this->pn532;
        this->pn532 = NULL;

        return false;
    }

    uint32_t versiondata = this->pn532->getFirmwareVersion();
    if (!versiondata) {
#ifdef DEBUG
        Serial.println("[Sensact] Failed to get version data");
#endif

        delete this->pn532;
        this->pn532 = NULL;

        return false;
    }

#ifdef DEBUG
    Serial.println("[Sensact] detected PN53x");
#endif

        // // Got ok data, print it out!
        // Serial.print("Found chip PN53x");
        // Serial.println((versiondata >> 24) & 0xFF, HEX);
        // Serial.print("Firmware ver. ");
        // Serial.print((versiondata >> 16) & 0xFF, DEC);
        // Serial.print('.');
        // Serial.println((versiondata >> 8) & 0xFF, DEC);

    if ( ! this->pn532->SAMConfig() ) {
#ifdef DEBUG
        Serial.println("[Sensact] SAMConfig failed on PN53x");
#endif

        delete this->pn532;
        this->pn532 = NULL;

        return false;

    }
    return true;
}

void Sensact::writeServo(int deg) {
    if ( this->i2c_tap_servo ) {
        this->i2c_tap_servo->write(deg);
    } else if ( this->tap_servo ) {
        this->tap_servo->write(deg);
#ifdef DEBUG
    } else {
        Serial.println("[Sensact] no servo available for writing");
#endif
    }
}

bool Sensact::isServoAvailable() {
    if ( this->i2c_tap_servo ) {
        return true;
    }
    if ( this->tap_servo ) {
        return true;
    }
    return false;    
}

bool Sensact::isNFCAvailable() {
    if ( this->pn532 ) {
        return true;
    }
    return false;
}



bool Sensact::readNFC(int timeout,void (*cb)(int), void (*result)(int,const char *)) {
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; 
    uint8_t uidLength;
    uint8_t success = 0;
    
    if ( ! this->pn532 ) {
        cb(SENSACT_NFC_CB_UNAVAILABLE);
        return false;
    }
    
    if ( timeout > 0 ) {
        success = this->pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, timeout);
    } else {
        success = this->pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
    }

    if ( ! success ) {
        cb(SENSACT_NFC_CB_NOREAD);
        return false;
    }

    if ((uidLength != 7) && (uidLength != 4)) {
        cb(SENSACT_NFC_CB_INCOMPATIBLE);
        return false;
    } 
  
    if (!this->pn532->ntag424_isNTAG424()) {
        cb(SENSACT_NFC_CB_NO_NTAG424);
        return false;
    }

    uint8_t bytesread = this->pn532->ntag424_ISOReadFile(this->nfcbuffer,sizeof(this->nfcbuffer));
    this->nfcbuffer[bytesread] = 0;
#ifdef DEBUG
    Serial.printf("Bytes read = %d\n",bytesread);
#endif 
 
    if ( bytesread == 0 ) {
        cb(SENSACT_NFC_CB_NO_BYTES);
        return false;
    }
 
#ifdef DEBUG
    Serial.println((char *)(this->nfcbuffer));
#endif 
    cb(SENSACT_NFC_CB_READ_SUCCESS);
    result(bytesread,(const char *)this->nfcbuffer);
    return true;
 
}