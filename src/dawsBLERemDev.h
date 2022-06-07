/**
@file dawsBLERemDev.h
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

#ifndef ____dawsBLERemDev__
#define ____dawsBLERemDev__


//#define ID_ATTRIBUTE_COUNT 1
//#define STATE_ATTRIBUTE_COUNT 1
//#define CMD_ATTRIBUTE_COUNT 1

#define MAX_DISCOVERED_ACCESSORY 4 ///< number of discovered accessories per connection
#define MAX_REMOTE_CON 5 ///< number of remote connections



/**
 @brief The BLE  connection to a remote device
 
 This provides the BLE central connection interface for an accessory remote device peripheral
 (e.g. points or other devices) under BLE control.
 The connection can support multiple services.  Either end may provide/access services although generally the peripheral
 provides services which central accesses as a client.
 For DAWS we expect a single instance of the generic gap service and multiple instances of the DAWS accessory service.
 
 At the moment we work on the basis that there will only be one client connection
 open at a time.  This is as the current connection should always
 be within range whereas other potential connections may not be.
 
 Discovery is performed the first time a connection is made to a remote BLE peripheral server after power on.  Discovery is
 not re-performed on re-connection.
 
 Discovery is event driven using mbed BLE call backs.
 
 @todo The remote device name characteristic is read but not saved yet.  Setting it correctly at the remote server end seems
 problematic. To be fixed.  
 
 */

class BLERemDev
{
public:
    BLERemDev(const uint8_t*, const ble::peer_address_type_t);
    BLERemDev();
    void setup();

    static void setAdReporting();
    static bool connectByIndex(int);
    static bool connectByName(String&);
    static String& getLocalNameByIndex(int);
    static void disconnect();
    static int getFoundCount();
    static BLERemDev* activeRemDev();

    
    String& getLocalName();

    //ble::connection_handle_t getConnHandle();
    
private:

    ble::GattClient& _gattClient = BLE::Instance().gattClient();  // reference gattClient
    
    bool _connect();
    void _disconn();
    

    void _initServiceDiscovery(const ble::connection_handle_t);
    void _serverDisconnected(const ble::DisconnectionCompleteEvent&);
    void _serviceDiscovered(const DiscoveredService *);
    void _characDiscovered(const DiscoveredCharacteristic *);
    void _dataRead(const GattReadCallbackParams*);
    void _dataWritten(const GattWriteCallbackParams*);
    void _discoveryTermination(const ble::connection_handle_t);
    void _doNextDA();
    void _descripsDone();
    
    // fields set up from scan reports
    String _localName;
    ble::address_t _peerAdd;  // remote address
    ble::peer_address_type_t _peerAddType; // remote address type
    
    
    static void _processScanReport(const ble::AdvertisingReportEvent&);
    void _queueRemAccReps(EventType, const int);
    
    static BLERemDev* _activeRemDev;  // pointer to active client connection

    
    ble::connection_handle_t _connHandle;  // con handle
    
    RemAccessory* _remAcc;
    RemAccessory* _firstRemAcc;

    //unsigned int _nextDA; // next element in DA array to be processed
    unsigned int _countDA;  // number of DAs on this connection
    
    
 
    
    //BLEcore* _bleCore;    // BLE core handler
    RemDevState_t _clientConState;  // state variable
    

    
    DiscoveredCharacteristic _devNameCharac; // part of the generic GAP service
    
    // variables related to service discovery on the current connection
    
    UUID _serviceUUID;  // service UUID of the service currently undergoing discovery
    
    // array of remote devices as found during scans.
    
    static BLERemDev _bleCon[];  // but only one used at the moment!
    static int _bleConCount; // number connections in discovered by scanning
    static bool _scanRepCBset;  // set to indicate callback has been set up
};


#endif /* defined(____dawsBLERemDev__) */
