/**
@file dawsDiscCli.h
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

#ifndef ____dawsDiscCli__
#define ____dawsDiscCli__


/**
 @brief The client accessory as discovered as part of BLE service discovery.
 
 This provides the central BLE client connection interface for  accessory services (e.g. points or other devices) under BLE control
 on peripheral accessory controllers.
 
 Each client instance is associated with a BLE peripheral service instance on a one to one basis.
 The client expects to find three characteristics for each accessory.  In addition to the id characteristic which uniquely,
 identfies the accessory and by naming convention identifies the type, there is the command characteristic,
 used by the client to initiate accessory actions (e.g. throw a point) and the state characteristic used to
 inform to client of current state.
 
 This class holds the accessory information as discovered before the id of the accessory is read and
 therefore before it can be linked
 to an accessory as configured.
 
 Once the accessory has been discovered, it is retained if the connection is closed. Re-discovery is not performed
 when the connection is re-established.  The accessory persists until power off.  It is assumed that the service will not
 change.
 
 Once the value associated with the ID characteristic is known, the accessory can be associated with the relevant
 Remote Accessory object which holds the rest of the information and provides the API for the client side applications.
 
 @see RemAccessory
 */

class DiscoveredAccCli: public Reporter
{
public:
    DiscoveredAccCli();
    DiscoveredAccCli(ble::connection_handle_t, UUID);
    ReporterType getType() override;
    void initSvr(ble::connection_handle_t, UUID);
    bool initCharacteristics(RemDevState_t);
    //BLERemDev* getConnection();
    UUID getServUUID();
    void setConServUUID(UUID);
    uuid_t saveCharacteristic(const DiscoveredCharacteristic*);
    ble_error_t readId();
    ble_error_t readState();
    
    GattAttribute::Handle_t idValueHandle();
    //GattAttribute::Handle_t stateCCCDHandle();
    GattAttribute::Handle_t stateValueHandle();
    
    bool writeCommand(const uint8_t);
    ble_error_t processDescrips(mbed::Callback<void()>);
    bool dataWritten(const GattWriteCallbackParams*);
    ble::connection_handle_t getConnHandle();
    bool doCCCDwrite();
    

    virtual void newState(PointState_t) = 0;  ///< state has changed  - inheriting class must process it
    virtual void setState(PointState_t) = 0;  ///<  state has become available - inheriting class must save it

 
private:
    static const ReporterType _type; // reporter type
    ble::GattClient& _gattClient = BLE::Instance().gattClient();  // reference gattClient
    // discovered service paramaters (shallow copy is not supported)
    UUID _serviceUUID;
    ble::connection_handle_t _connHandle;  // connection handle
    
    // standard mbed BLE callbacks for discription discovery
    // discription discovery complete
    void _descripDisc(const CharacteristicDescriptorDiscovery::DiscoveryCallbackParams_t*);
    void _ddDone(const CharacteristicDescriptorDiscovery::TerminationCallbackParams_t*);

    mbed::Callback<void()> _descripsDoneCB; // call back executed when complete here

    // discovered service characteristics (shallow copies)
    DiscoveredCharacteristic _idDC;
    DiscoveredCharacteristic _stateDC;
    DiscoveredCharacteristic _commandDC;
    GattAttribute::Handle_t _stateCCCDHandle;  // state characteristic's CCCD handle
    
    void _dataChange(const GattHVXCallbackParams*);
    void _dataRead(const GattReadCallbackParams*);
};






#endif /* defined(____dawsDiscCli__) */
