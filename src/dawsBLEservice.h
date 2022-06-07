/**
@file dawsBLEservice.h
@author Paul Redhead on 3/2/2021.
@copyright (C) 2021 Paul Redhead
 */

//
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
//
//  Version 0.a First released version
//
//
//

#ifndef ____dawsBLEservice__
#define ____dawsBLEservice__


#define ID_ATTRIBUTE_COUNT 1  ///< number of attributes in ID characteristic
#define STATE_ATTRIBUTE_COUNT 1 ///< number of attributes in state characteristic
#define CMD_ATTRIBUTE_COUNT 1 ///< number of attributes in command characteristic
#define MAX_ACC_CHARACTERISTIC_COUNT 3  ///< id, command and state




/**
 @brief BLE Accessory Service
 
 This class provides the DAWS accessory controller BLE service to control points and other
 accessories.  Each instance of the service controls one point or other accessory.
 The service incorporates an ID characteristic which identifies the accessory to clients.  This id should be unique
 across all accessory device in a configuration. The service has
 
 - a command characteristic, set by the client to initiate an action, and
 
 - a status characteristic, used by the server to notify the client.
 
 This defines the service on the server (usually peripheral) side.  At the client the service and its characteristics are discovered.
 
 
 
 
@note the GATT server allows for multiple services with the same UUID.  The client must
 deal with this correctly and not assume that the UUID identifies the instance of the service.
 Once the id has been read to identify the service instance, the handle may be used for this.
 
 
 *********************************/

class BLEAccService: public Reporter, public GattService
{
public:
    BLEAccService(const char*);
    void setup() override;
    ReporterType getType() override;
    
    /**
     @brief
     
     This routine is called when when the value of the command characteristic is updated.
     It is a pure virtual routine and must be defined in an inheriting class.
     */
    virtual void doCommand(char) = 0;
    
    virtual void vSetup();
    ble_error_t updateState(PointState_t);
    void listHandles();

private:
    static const ReporterType _type; // reporter type
    ble::GattServer& _gattServer = BLE::Instance().gattServer();  // reference gattServer

    PointState_t _state;        // value for the state characteristic
    uint8_t _command;           // value for the command characteristic
    
    // characterisitics
    GattCharacteristic _stateCharacteristic;  // state characteristic
    GattCharacteristic _cmdCharacteristic;  // command characteristic
    GattCharacteristic _idCharacteristic;   // id characteristic
    
    const char *_accId;  // pointer to id characteristic value as passed to constructor
    size_t _accIdLen;  // length of id field excluding trailing null
    
    // id characteristic attributes
    GattAttribute _idFormat;  // specifies the id (e.g. UTF8)
    static const GattCharacteristic::PresentationFormat_t _idFormatField; // format
    GattAttribute _idUserDesc;     // user description for id characteristic
    static const char _idDescTxt[];  // id description text
    GattAttribute* _idAttributes[ID_ATTRIBUTE_COUNT];  // list of attributes (2!)
    GattAttribute _stateUserDesc;     // user description for state characteristic
    static const char _stateDescTxt[];  // id description text
    GattAttribute* _stateAttributes[STATE_ATTRIBUTE_COUNT];  // list of attributes (2!)
    GattAttribute _cmdUserDesc;     // user description for id characteristic
    static const char _cmdDescTxt[];  // id description text
    GattAttribute* _cmdAttributes[CMD_ATTRIBUTE_COUNT];  // list of attributes (2!)
    
    
    
    

    GattCharacteristic* _serviceCharacteristics[MAX_ACC_CHARACTERISTIC_COUNT];
    unsigned short _serviceCharCount; // count of attached characteristics
    

    void _dataWritten(const GattWriteCallbackParams*);
 
};

#endif /* defined(____dawsBLEservice__) */
