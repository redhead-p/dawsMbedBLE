/**
@file dawsBLEservice.cpp
@author Paul Redhead on 3/2/2021.
@copyright (C) 2021 Paul Redhead
 
 This file contains the server and service related elements for BLE (but not advertising which is in the core module)
 
 */
//  This file is part of DAWS.
//  DAWS is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  DAWS is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

//  You should have received a copy of the GNU General Public License
//  along with DAWS.  If not, see <http://www.gnu.org/licenses/>.


/*!
 */
#include <limits.h>
#include <Arduino.h>

//#define BLE_ROLE_BROADCASTER true
#include <mbed.h>
#include <ble/BLE.h>
#include <Gap.h>
#include <GattServer.h>
#include <GattCallbackParamTypes.h>


#include "daws.h"
#include "dawsReporter.h"

#include "dawsBLE.h"
#include "dawsBLEservice.h"



#define DEBUG false  ///< enable BLE debug output to IDE Monitor



const  ReporterType BLEAccService::_type = ACC_REP;
const char BLEAccService::_idDescTxt[] = "Id";
const char BLEAccService::_stateDescTxt[] = "State";
const char BLEAccService::_cmdDescTxt[] = "Command";

const GattCharacteristic::PresentationFormat_t BLEAccService::_idFormatField =
{ GattCharacteristic::BLE_GATT_FORMAT_UTF8S,
    0,
    0,
    0x01,
    0
};


/**
 @brief BLE Accessory Service Constructor
 
 This constructs an instance of the BLE Accessory Service.  There will be one of these for each accessory.
 The data structures for service and characteristics are constructed here.
 
 @param accId - this is the accessory id for accessory service.    It is unique across the system (layout)
 
 
 */


BLEAccService::BLEAccService (const char* accId) :

Reporter(ACC_REP),


// build id user description attribute
_idUserDesc(UUID(BLE_UUID_DESCRIPTOR_CHAR_USER_DESC),
                                     (uint8_t*)_idDescTxt,
                                     (uint16_t)sizeof(_idDescTxt) - 1,
                                     (uint16_t)sizeof(_idDescTxt) - 1,
                                     false),
// build id format attribute
_idFormat(UUID(BLE_UUID_DESCRIPTOR_CHAR_PRESENTATION_FORMAT),
                                (uint8_t*)&_idFormatField,
                                (uint16_t)sizeof(GattCharacteristic::PresentationFormat_t),
                                (uint16_t)sizeof(GattCharacteristic::PresentationFormat_t),
                                false),


// build array of id characteristic attributes
_idAttributes{&_idUserDesc},  // id format ommitted as doesn't seem to work
//_idAttributes{&_idUserDesc, &_idFormat},

// build id characteristic
_idCharacteristic(
                  BLEcore::getUUID(ID_UUID),     // uuid
                  (uint8_t*)accId,    // value
                  strlen(accId), // truncate to remove null
                  //strlen(accId),
                  (strlen(accId) > MAX_ID_SIZE)?MAX_ID_SIZE:strlen(accId), // truncate if too long
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ,
                  _idAttributes,
                  ID_ATTRIBUTE_COUNT,
                  false
                  ),

// build state user description
_stateUserDesc(UUID(BLE_UUID_DESCRIPTOR_CHAR_USER_DESC),
               (uint8_t*)_stateDescTxt,
               (uint16_t)strlen(_stateDescTxt),
               (uint16_t)strlen(_stateDescTxt),
               false),


// build array of state characteristic attributes
_stateAttributes{&_stateUserDesc},

// build state characteristic

_stateCharacteristic
{
        BLEcore::getUUID(STATE_UUID),     // uuid
        (uint8_t*)&_state,    // value
        sizeof(PointState_t), // size of value
        sizeof(PointState_t),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY,
        _stateAttributes,
        STATE_ATTRIBUTE_COUNT,
        false
        
},

// build command user description
_cmdUserDesc(UUID(BLE_UUID_DESCRIPTOR_CHAR_USER_DESC),
               (uint8_t*)_cmdDescTxt,
               (uint16_t)strlen(_cmdDescTxt),
               (uint16_t)strlen(_cmdDescTxt),
               false),


// build array of state characteristic attributes
_cmdAttributes{&_cmdUserDesc},
// build command characteristic
_cmdCharacteristic
{
        BLEcore::getUUID(CMD_UUID),     // uuid
        (uint8_t*)&_command,    // command
        sizeof(uint8_t), // size of value
        sizeof(uint8_t),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE,
        _cmdAttributes,  // same attributes as state (i.e. user name
        CMD_ATTRIBUTE_COUNT,
        false
        
},




// build array of characteristics
_serviceCharacteristics{
    &_idCharacteristic,
    &_stateCharacteristic,
    &_cmdCharacteristic,
},

// and finally the service
GattService(BLEcore::getServUUID(),      // UUID
            _serviceCharacteristics,// list of service characteristics
            3)                      // number of service characteristics
            
{
    _accId = accId;
    _accIdLen = strlen(accId);
    _idUserDesc.allowWrite(false);
    _state = P_UNKNOWN;             // initial state server side is unknown
}

/**
 @brief Setup the BLE service for this accessory.
 
 This sets the service up. It
 -  adds the service to the server, and
 -  adds a callback which is executed when data are written.
 */
void BLEAccService::setup()
{
    // add this service to the server
    _gattServer.addService(*this);
    
    // register the write call back routine
    _gattServer.onDataWritten().add(
                                    ble::WriteCallback_t(
                                                         this,
                                                         &BLEAccService::_dataWritten)
                                    );

    vSetup();  // invoke the setup in the derrived class if present
}


/**
 @brief Virtual function for derrived class setup.
 
 This is used if the derrived class doesn't have its own vSetup.
 
 */
void BLEAccService::vSetup()
{
    
}



/**
@brief Get Reporter Type

 Retreives reporter type.

@return reporter identifier as an enum member.
*********************************/

ReporterType BLEAccService::getType()
{
    return(_type);
}


/**
 @brief List the handles for this service.
 
This lists the handles associated with the id, state and command characteristics.

@note this is for diagnostic use only.  It should only be called from within DEBUG or UI code
 */

void BLEAccService::listHandles()
{
    const char * descrip[] = {_idDescTxt, _stateDescTxt, _cmdDescTxt};
    Serial.print("Service handle ");
    Serial.println(getHandle());
    Serial.print("Characteristics :");
    Serial.println(getCharacteristicCount());
    
    for (int x = 0; x < MAX_ACC_CHARACTERISTIC_COUNT; x++)
    {
        GattAttribute* ga;
        GattAttribute::Handle_t h;
        GattCharacteristic* gc = _serviceCharacteristics[x];
        //h = gc->getValueHandle();
        Serial.print(descrip[x]);
        Serial.print("\tproperties ");
        Serial.println(gc->getProperties());
        GattAttribute& gar = gc->getValueAttribute();
        Serial.print("\t handle ");
        Serial.print(gar.getHandle());
        Serial.print(' ');
        BLEcore::printUUID(gar.getUUID());
        
        uint8_t yMax = gc->getDescriptorCount();
        for (uint8_t y = 0; y < yMax; y++)
        {
            ga = gc->getDescriptor(y);
            Serial.print("\t handle ");
            Serial.print(ga->getHandle());
            Serial.print(' ');

            BLEcore::printUUID(ga->getUUID());
        }
    }
}


/**
@brief post the updated state to the client
 
 This writes the updated state characteristic value which will notify the client as long as it has enabled
 notifications for this characteristic.  Usually called as result of a received accessory command.
 A command may generate more
 than one update, e.g on starting and completing a point movement.
 
 @param newState - the updated state
 
 @return the BLE error code
 */
ble_error_t BLEAccService::updateState(PointState_t newState)
{
    ble_error_t bleErr;
    _state = newState;
    bleErr = _gattServer.write(_stateCharacteristic.getValueHandle(),
                                                (const uint8_t*)&_state,
                                                (uint16_t)sizeof(_state),
                                                false);
    queueReport(ACC_STATE_CHANGE, newState);
#if DEBUG
    if (bleErr != BLE_ERROR_NONE)
    {
        Serial.print("Server Write error ");
        Serial.println(bleErr);
    }
#endif
    return(bleErr);
}

// Data written function for callback. Called back when the client
// has written (to any characteristic value on any service).
// We need to check that the handle
// relates to this service.

void BLEAccService::_dataWritten(const GattWriteCallbackParams* cbp)
{
#if DEBUG
    Serial.print("Data written by client - ");
    Serial.print("Handles con:");
    Serial.print( cbp->connHandle);
    Serial.print(" attr:");
    Serial.println(cbp->handle);
    Serial.print("Rx len:");
    Serial.println(cbp->len);
    for (int x = 0; x < cbp->len; x++)
    {
        if (*(cbp->data + x) < 16)
        {
            Serial.print('0');
        }
        Serial.print(*(cbp->data + x), HEX);
    }
    Serial.println();
#endif
    if ((_cmdCharacteristic.getValueHandle() == cbp->handle) &&
        (cbp->len == 1))
    {
        // it's our command characteristic handle and a single character
        doCommand(*(cbp->data));  // call command processor in inheriting class
    }
}



