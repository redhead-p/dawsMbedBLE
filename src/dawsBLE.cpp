/**
@file dawsBLE.cpp
@author Paul Redhead on 3/2/2021.
@copyright (C) 2021 Paul Redhead
 
 This file contains the BLE DAWS components common to peripheral (e.g. static accessories) and central
 (e.g. mobile loco etc) components.
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
#include <mbed.h>
#include <ble/BLE.h>
#include <Gap.h>
#include <AdvertisingDataParser.h>

#include "daws.h"
#include "dawsReporter.h"
#include "dawsBLE.h"



#define DEBUG false ///< enable BLE debug output to IDE Monitor

// long UUIDs uniquely assigned to this project are defined here
// short UUIDs allocated as defined in the relevant Bluetooth documentation
// are embedded in code
// n.b. the uniqueness of these long UUIDs is assumed as result of their generation
// mechanism - they are not registered in anyway.
// version 4 uuids got from www.uuidgenerator.net 28/1/2021
const char BLEcore::_pointServUUID[] =
    "875e6ef1-7e3f-4e57-86e1-9a921002b8e9";  ///< point service UUID
const char BLEcore::_idUUID[] =
    "8dbe4bf8-b166-4d52-bd7e-56cd5eb6c246"; ///< id characteristic UUID
const char BLEcore::_stateUUID[] =
    "068a007d-9f09-49f0-907c-2d54178147b8"; ///< state characteristic UUID
const char BLEcore::_cmdUUID[] =
    "3d59437d-265e-4698-9b4f-3852e8ed2b33"; ///<command characteristic UUID

const char* BLEcore::_characUUID[] = {BLEcore::_idUUID,
    BLEcore::_stateUUID, BLEcore::_cmdUUID};


const ReporterType BLEcore::_type = BLE_REP;

BLEcore* BLEcore::_thisBLEcore = nullptr;








/**
 @brief Construct the core BLE object
 
 This constructs the driver for core BLE in peripheral mode or central mode.
 In peripheral mode advertising is enabled.
 Services may be enabled.
 
 @param devName - the device name if advertising
 @param evqp - pointer to the mbed event queue
 @param periMode - if true run in peripheral mode
 
 */

BLEcore::BLEcore(const char* devName, events::EventQueue* evqp, bool periMode):
_bleTaskThread(BLE_PRIORITY), Reporter(BLE_REP)
{
    _devName = devName;
    _evQp = evqp;
    _periMode = periMode;
    _onCentralConnect = nullptr;
    _onCentralDisconnect = nullptr;
    _setupDone = false;
    
    _ledp = nullptr;

    
    if (_thisBLEcore == nullptr)
    {
        _thisBLEcore = this;  // so we have a pointer to access non static methods
    }
#if DEBUG
    else
    {
        Serial.println(F("Attempt to create a BLE core driver"));
    }
#endif

}

/**
 @brief Construct the core BLE object with indicator LED
 
 This constructs the driver for core BLE in peripheral mode or central mode.
 In peripheral mode advertising is enabled.
 Services may be enabled.  The LED is on when one or more connections are open.
 
 @param devName - the device name if advertising
 @param evqp - pointer to the mbed event queue
 @param periMode - if true run in peripheral mode
 @param ledp - pointer to the mbed digital output for the LED
 
 */

BLEcore::BLEcore(const char* devName, events::EventQueue* evqp, bool periMode,
                 mbed::DigitalOut* ledp):
_bleTaskThread(BLE_PRIORITY), Reporter(BLE_REP)
{
    _devName = devName;
    _evQp = evqp;
    _periMode = periMode;
    _onCentralConnect = nullptr;
    _onCentralDisconnect = nullptr;
    _setupDone = false;
    
    _ledp = ledp;

    
    if (_thisBLEcore == nullptr)
    {
        _thisBLEcore = this;  // so we have a pointer to access non static methods
    }
#if DEBUG
    else
    {
        Serial.println(F("Attempt to create a BLE core driver"));
    }
#endif

}

/**
 @brief Setup BLE
 
 The setup routine sets this object as having the callback for scheduling events and having gap event handling functions.
 
 This is done before services are specified and added.  To ensure this setup has to be run explicitly before services are
 done.  It checks to see if already been run, so the reporter based setup won't cause problems.
 

 */

void BLEcore::setup()
{
    ble_error_t bleErr;
    if (_setupDone)
    {
        return;   // we don't want to do this twice
    }
    _setupDone = true;
    
#if DEBUG
    Serial.begin(115200);
    while (!Serial)
    {
        delay(100);
        
    }
    if(_periMode)
    {
        Serial.println("BLE periperal starting");
    }
    else
    {
        Serial.println("BLE central starting");
    }
#endif
    
    // set up the 'on events to process' callback
    _ble.onEventsToProcess(
                           BLE::OnEventsToProcessCallback_t(
                                                            this,
                                                            &BLEcore::_scheduleBLEevents
                                                            )
                           );
    
    _gap.setEventHandler(this);
    
    // start the thread to dispatch BLE middleware tasks
    _bleTaskThread.start(callback(_evQp, &events::EventQueue::dispatch_forever));
#if DEBUG

        Serial.println("BLE setup complete");

#endif

}

/**
@brief Get Reporter Type

 Retreives reporter type.

@return reporter identifier as an enum member.
*********************************/

ReporterType BLEcore::getType()
{
    return(_type);
}


/**
 @brief Start Bluetooth
 
 This performs the BLE initilisation
 */
void BLEcore::startBLE()
{
    ble_error_t bleErr;
    // not sure about this - should be part of daws specific server - now removed!
    // set the server device name characteristic value
    bleErr = _ble.gattServer().write(
                                     3,
                                     (uint8_t*)_devName,
                                     strlen(_devName),
                                     false);
    
#if DEBUG
    if (bleErr != BLE_ERROR_NONE)
    {
        
        Serial.print("Set device name attribute :");
        Serial.println(bleErr);
    }
#endif
    
    /* mbed will call on_init_complete when when ble is ready */
    bleErr = _ble.init(this, &BLEcore::_onInitComplete);
    if (bleErr != BLE_ERROR_NONE)
    {
#if DEBUG
        Serial.print("Init call fail: ");
        Serial.println(bleErr);
    }
    else
    {
        Serial.println("Init started");
#endif
    }

}


/**
 @brief Get reference to BLE core controller object
 
 This returns a reference to the BLE core controller.
 There is only one.
 
 @note This is a static routine
 
 @return reference to BLE core controller.
 */

BLEcore& BLEcore::instance()
{
    return ((BLEcore&)(*_thisBLEcore));
}

/**
 @brief get the number of open connections
 
 This exposes the count of how many connections  are open.
 
 @return the connection count
 
 */

int BLEcore::getConnectionCount()
{
    return(_conCount);
}

/* Schedule processing of events from the BLE middleware in the event queue. */
void BLEcore::_scheduleBLEevents(BLE::OnEventsToProcessCallbackContext *context)
{
    _evQp->call(mbed::Callback<void()>(&context->ble, &BLE::processEvents));
}

//Gap call backs
/**
 @brief Advertising end call back
 
 Handle the end of advertising.
 
 @note This overrides the virtual routine in the GAP interface.
 
 @param event - Advertising end event.
 */
void BLEcore::onAdvertisingEnd(const ble::AdvertisingEndEvent& event)
{
#if DEBUG
    if(_periMode)
    {
    Serial.print("Advertising stopped. Created ");
    Serial.print(event.getCompleted_events());
    Serial.print(" events.");
    
    Serial.println(event.isConnected()?" Now connected":" Not connected");
    }
    else
    {
        Serial.println("Unexpected advertising end event");
    }
#endif
}
/**
 @brief Retrieve the UUID for the given index
 
 This returns the UUID at the given index.
 @param u - the index to the UUID required as an enumeration.
 @note as the index is provided as an enumeration it is not checked.
 @return the UUID
 */
UUID BLEcore::getUUID(uuid_t u)
{
    return UUID(_characUUID[u]);
}
/**
 @brief Match UUID
 The provided UUID is matched against the core list of UUIDs in use.
 MAX_UUID is returned if no match.
 
 @param uuid - the UUID to be matched
 
 @return the index number found or MAX_UUID
 */
uuid_t BLEcore::matchUUID(UUID uuid)
{
    int u = 0;
    
    while ((UUID(_characUUID[u]) != uuid) && (u < MAX_UUID))
    {
        u++;
    }
    return((uuid_t)u);
}

/**
 @brief Exposes the service UUID
 
 This returns the service UUID.  At the moment, apart from generic BLE services, there is only one
 service - the accessory service although there may be many instances.
 
 @return the accessory (point) service UUID
 */
UUID BLEcore::getServUUID()
{
    return UUID(_pointServUUID);
}

/**
 @brief Set the central connect callback.
 
 This sets the callback to the client to be executed when a client initiated (central) connection
 completes.
 
 @param cb - the callback to be executed.
 */
void BLEcore::setConnectionCompleteCallback
 (mbed::Callback<void(const ble::connection_handle_t)> cb)
{
    _onCentralConnect = cb;
}
                                            
/**
 @brief Set the central disconnect callback.
 
 This sets the callback to the client to be executed when a client (central) disconnection
 completes.
 
 @param cb - the callback to be executed.
 */
void BLEcore::setDisconnectionCompleteCallback
 (mbed::Callback<void(const ble::DisconnectionCompleteEvent&)> cb)
{
    _onCentralDisconnect = cb;
}

/**
 @brief Set the scan event callback.
 
 This sets the callback to processed when a remote device offering a service of interest is detected
 
 @param cb - the callback to be executed.
 */
void BLEcore::setScanEventCallback
 (mbed::Callback<void(const ble::AdvertisingReportEvent&)> cb)
{
    _onScanAdReport = cb;
}


/**
 @brief Connection complete call back
 
 Handle the new connection.  The connection is always client initiated.  If we are client side, we will have initiated the
 connect and the client needs to be informed of the event.
 
 If we are server side, the client will write or initiate notifications.  These will be handled by the service.
 
 @note This overrides the virtual routine in the GAP interface.

 @param event - BLE connection complete  event.
 */

void BLEcore::onConnectionComplete(const ble::ConnectionCompleteEvent& event)
{
    ble_error_t bleErr = event.getStatus();

    
#if DEBUG
    if (bleErr != BLE_ERROR_NONE)
    {
        Serial.print("Connection fail: ");
        Serial.println(bleErr);
    }
    else
    {
        Serial.println("Connection complete ");
    }
#endif
    if (bleErr == BLE_ERROR_NONE)
    {
        if ((_conCount++ == 0) && (_ledp != nullptr))  // opening the first connection
        {
            *_ledp = 0;     // turn led on
        }
        
        
        if ((event.getOwnRole() == ble::connection_role_t::CENTRAL) &&
            (_onCentralConnect != nullptr))
        {
            // execute call back (to connection object)
            _onCentralConnect(event.getConnectionHandle());
            queueReport(BLE_CONNECTED, event.getConnectionHandle());
        }
    }
    
}

/**
 @brief Disconnection complete call back
 
 Restart advertising if peripheral.  If there's a central connection callback, invoke it.
 The called back routine must check that it's for it!
 
 @note This overrides the virtual routine in the GAP interface.

 
 @param event - Disconnection complete  event.
 
 */
void BLEcore::onDisconnectionComplete(const ble::DisconnectionCompleteEvent& event)
{
    ble_error_t bleErr;
    if ((--_conCount == 0) && (_ledp != nullptr))   // if closing the last connection
    {
        *_ledp = 1;     // turn led off
    }
#if DEBUG

    Serial.println("Disconnected ");
    Serial.println((const int)event.getReason());

#endif
    if (_periMode)
    {
#if DEBUG
        Serial.println(_gap.isAdvertisingActive(ble::LEGACY_ADVERTISING_HANDLE)?
                       "Still advertising":"Advertising is stopped");
#endif
        bleErr = _gap.startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
#if DEBUG
        
        if (bleErr != BLE_ERROR_NONE)
        {
            Serial.print("Start advertising fail: ");
            Serial.println(bleErr);
            while(true){}
        }
        else
        {
            Serial.println("Advertising restarted");
        }
#endif
    }
    if (_onCentralDisconnect != nullptr)
    {

        _onCentralDisconnect(event);    // execute call back
        queueReport(BLE_DISCONNECTED, event.getConnectionHandle());
    }

    
}


// init complete start advertising if needed
void BLEcore::_onInitComplete(BLE::InitializationCompleteCallbackContext *params)
{

    ble_error_t bleErr;
    ble::AdvertisingParameters advParams
    (
     ble::advertising_type_t::CONNECTABLE_UNDIRECTED
     // try default times
     //ble::adv_interval_t(ble::millisecond_t(1000))  // this is the minimum time
     );
    ble::AdvertisingDataBuilder advDataBuilder
    (_advBuffer, ble::LEGACY_ADVERTISING_MAX_SIZE);
    
    const UUID suuid[] = {UUID(_pointServUUID)};
    
    //mbed::Span<const UUID> uuidSpan(suuid,1);
    
    
    
    if (params->error != BLE_ERROR_NONE) {
#if DEBUG
        Serial.print("Init completion fail: ");
        Serial.println(params->error);
#endif
    }
    else
    {
#if DEBUG
        ble::address_t macAdd;
        ble::own_address_type_t addType;
        
        Serial.print("Init complete - mac address ");
        _gap.getAddress(addType, macAdd);
        Serial.print(addType.value());
        Serial.print(' ');
        for (int i = 0; i < 6; i++)
        {
            if (macAdd[i] < 16)
            {
                Serial.print('0');
            }
            Serial.print(macAdd[i], HEX);
            if (i < 5)
            {
                Serial.print(':');
            }
            else
            {
                Serial.println();
            }
        }
#endif
        if(_periMode)
        {
            // set default flags - discoverable and only BLE capable
            advDataBuilder.setFlags();
            advDataBuilder.setName(_devName);
            advDataBuilder.setLocalServiceList(suuid,false);
            bleErr = _gap.setAdvertisingParameters(
                                                   ble::LEGACY_ADVERTISING_HANDLE,
                                                   advParams
                                                   );
#if DEBUG
            if (bleErr != BLE_ERROR_NONE)
            {
                Serial.print("Set Adv Params fail: ");
                Serial.println(bleErr);
                while(true){}
            }
#endif
            
            _gap.setAdvertisingScanResponse(
                                            ble::LEGACY_ADVERTISING_HANDLE,
                                            advDataBuilder.getAdvertisingData()
                                            );
            
             _gap.setAdvertisingPayload(
             ble::LEGACY_ADVERTISING_HANDLE,
             advDataBuilder.getAdvertisingData()
             );
             
            // start advertising
            bleErr = _gap.startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
#if DEBUG
            if (bleErr != BLE_ERROR_NONE)
            {
                Serial.print("Start advertising fail: ");
                Serial.println(bleErr);
                while(true){}
            }
#endif
        }
    }
}


/**
 @brief Start a scanning sequence.
 
 
 Scanning happens repeatedly and is defined by:
 - The scan interval which is the time (in 0.625us) between each scan cycle.
 - The scan window which is the scanning time (in 0.625us) during a cycle.
 If the scanning process is active, the local device sends scan requests
 to discovered peer to get additional data.
 
 Active scanning is not used.  Scanning is performed for a fixed period.
 
 Remote devices detected during scanning are reported  via onAdvertisingReport().
 */
bool BLEcore::scan()
{
    
    ble_error_t bleErr;
    

    bleErr = _gap.setScanParameters
    (
     ble::ScanParameters
     (
      ble::phy_t::LE_1M,   // scan on the 1M PHY
      ble::scan_interval_t(100),
      ble::scan_window_t(100),
      false
      )
     );
    
    if (bleErr!= BLE_ERROR_NONE)
    {
#if DEBUG
        printf("Scan parameter error %d\n", bleErr);
#endif
        return(false);
    }
    bleErr = _gap.startScan(ble::scan_duration_t(1000));
#if DEBUG
    if (bleErr!= BLE_ERROR_NONE)
    {
        Serial.print("Start scan fail: ");
        Serial.println(bleErr);
    }
    else
    {
        Serial.println("Scan started");
    }
#endif
    queueReport(BLE_SCAN_START, 0);
    return(bleErr == BLE_ERROR_NONE);
}




/**
 @brief Advertising report received call back
 
  An advertising report has been received as result of a scan. If a device offering the accessory service is found
 (for the first time) its details will be added to the list known devices and it will become available for connection.
 Details are not written to persistant storage so
 scanning must be performed as required.
 
 @note This overrides the virtual routine in the GAP interface.

 
 @param event - BLE advertising report event
 
  */

void BLEcore::onAdvertisingReport(const ble::AdvertisingReportEvent& event)
{
    if (_onScanAdReport != nullptr)  // if call back set
    {
        // the scan event is usally managed by the remote connection system
        _onScanAdReport(event);  // forward event for processing
    }
#if DEBUG
    else
    {
        Serial.println("Scan report - but no callback set");
    }
#endif
}

/**
 @brief Scanning period ended

 Scanning has terminated as result of a timeout.  A report is generated.
 
 @note This overrides the virtual routine in the GAP interface.

 
 @param event - BLE scan timeout event
 */

void BLEcore::onScanTimeout(const ble::ScanTimeoutEvent& event)
{
#if DEBUG
    Serial.println("Scan time out");
#endif
    queueReport(BLE_SCAN_DONE, 0);
}

/**
 @brief Print a UUID
 
 The UUID is printed in the standard format - either long or short as appropriate.
 Serial is assumed to be available. This should only be called if DEBUG or UI is enabled in the calling code.
 
 @param u - UUID to be printed
 
 @note This is a static function.
 */

void BLEcore::printUUID(UUID u)
{
    uint8_t ul = u.getLen(); // get length of uuid
    const uint8_t* up = u.getBaseUUID();
    for (uint8_t z = ul; z > 0; z--)
    {
        switch (z)
        {
            case 12:
            case 10:
            case 8:
            case 6:
                Serial.print('-');
                break;
            default:
                break;
        }
        if (*(up + z - 1) < 16)
        {
            Serial.print('0');
        }
        Serial.print(*(up + z - 1), HEX);
    }
    Serial.println();
}





