/**
@file dawsBLE.h
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

#ifndef ____dawsBLE__
#define ____dawsBLE__


//#define ID_ATTRIBUTE_COUNT 1
//#define STATE_ATTRIBUTE_COUNT 1
//#define CMD_ATTRIBUTE_COUNT 1
//#define MAX_ACC_CHARACTERISTIC_COUNT 9

#define MAX_ID_SIZE 10 ///< maximum size for identifier strings


/**
 @brief Enumerated list of client connection states
 
 This enumerates the connection states as relevant to the client (central) end.
 */
enum RemDevState_t :byte
{
    CS_INITIAL,  ///< connection null - address & id not scanned yet
    CS_CONNECTABLE,  ///< have local name and address but not yet connected and characteristics not  discovered yet
    CS_CON_FIRST, ///< connected - first service
    CS_CON_DISC, ///< connected - discovery in progress
    CS_CON_INIT, ///< connected - initial reads and set up notifications etc
    CS_RECON_INIT, ///< re-connected - set up notifications only
    CS_CONNECTED, ///< connected and discovery complete
    CS_DISCONNECTING, ///< local disconnect command issued
    CS_DISCON,       ///< disconnected but discovered characteristics retained.
    CS_ERR  ///< not connected - last connection attempt failed.
};


/**
 @brief Characteristic UUIDs
 
 An enumerated list of characteristic UUIDs of interest.  The UUID values are either specific to DAWS or common
 to all BLE services, for which the short values are used.  Common BLE UUIDs are defined in
 <blecommon.h>.

 */

enum uuid_t :byte
{
    ID_UUID         = 0, ///< daws service identifier (read only)
    STATE_UUID      = 1, ///< daws state variable (notify)
    CMD_UUID        = 2, ///< daws command (read/write)
    MAX_UUID        = 3 ///< boundary value for size etc
    
    
};



/**
 @brief Bluetooth Low Energy (BLE).
 
 This class acts as a controller for the BLE device and GAP functions for peripheral  and central devices.  For peripheral devices
 (e.g.  DAWS accessory controllers) this is taken as advertising and providing services.  For central devices (e.g. DAWS locomotives)
 this provides scanning and
 filtering.
 
 It inherits from the GAP EventHandler class overriding virtual functions therein.
 

 
 It uses the Mbed BLE API.
 
 
 *********************************/

class BLEcore: public ble::Gap::EventHandler, public Reporter
{
public:
    BLEcore(const char*, events::EventQueue*, bool);
    BLEcore(const char*, events::EventQueue*, bool, mbed::DigitalOut*);
    void setup() override;
    ReporterType getType() override;
    void startBLE();
    bool scan();
    int getConnectionCount();
    
    static BLEcore& instance();
    static UUID getUUID(uuid_t);
    static uuid_t matchUUID(UUID);
    static UUID getServUUID();
    
    // virtual GAP routines declared here and defined in code
    void onAdvertisingEnd(const ble::AdvertisingEndEvent&) override;
    void onConnectionComplete(const ble::ConnectionCompleteEvent&) override;
    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) override;
    void onAdvertisingReport(const ble::AdvertisingReportEvent&) override;
    void onScanTimeout(const ble::ScanTimeoutEvent&) override;

    
    void setConnectionCompleteCallback
    (mbed::Callback<void(const ble::connection_handle_t)>);
    void setDisconnectionCompleteCallback
    (mbed::Callback<void(const ble::DisconnectionCompleteEvent&)>);
    void setScanEventCallback
    (mbed::Callback<void(const ble::AdvertisingReportEvent&)>);
    
    static void printUUID(UUID u);
    
    
private:
    rtos::Thread _bleTaskThread;
    static const ReporterType _type; // reporter type
    BLE& _ble = BLE::Instance();  // reference to the BLE device
    ble::Gap& _gap = _ble.gap();  // reference to GAP functions
    events::EventQueue* _evQp;    // pointer the event queue
    static BLEcore* _thisBLEcore; // pointer to the single BLE core instance
    mbed::DigitalOut* _ledp;      // pointer to the BLE on led
    
    bool _setupDone;              // to ensure setup not run twice
    const char* _devName;         // the name of this device (as advertised)
    
    uint16_t _conCount;           // number of connections
    
    // callback for handling central connection
    mbed::Callback<void(ble::connection_handle_t)> _onCentralConnect;
    // callback for handling central disconnection
    mbed::Callback<void(const ble::DisconnectionCompleteEvent&)> _onCentralDisconnect;
    // callback for handling scan detected advertising report
    mbed::Callback<void(const ble::AdvertisingReportEvent&)> _onScanAdReport;

    
    static const char _pointServUUID[]; // point server uuid
    static const char _idUUID[]; // id characteristic uuid
    static const char _stateUUID[]; // state characteristic uuid
    static const char _cmdUUID[];   // command characteristic uuid
    static const char* _characUUID[];  // array of UUIDs

    
    bool _periMode;                  // true if in peripheral mode (i.e advertising etc)
    
    void _onInitComplete(BLE::InitializationCompleteCallbackContext *);
    
    void _scheduleBLEevents(BLE::OnEventsToProcessCallbackContext *);
    
    uint8_t _advBuffer[ble::LEGACY_ADVERTISING_MAX_SIZE]; // advertising data buffer
};


#endif /* defined(____dawsBLE__) */
