/**
@file dawsBLERemDev.cpp
@author Paul Redhead on 3/2/2021.
@copyright (C) 2021 Paul Redhead
 
 This contains the code for the Bluetooth LE client access.
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

// following includes now seem to be pulled in elsewhere - cause warnings if done here
//#include "DiscoveredService.h"
//#include "DiscoveredCharacteristic.h"
//#include "CharacteristicDescriptorDiscovery.h"

#include "daws.h"
#include "dawsReporter.h"

#include "dawsBLE.h"
#include "dawsDiscCli.h"
#include "dawsRemAcc.h"

#include "dawsBLERemDev.h"


#define DEBUG false  ///< enable BLE debug output to IDE Monitor

/*
 ********************************************************
 definitions of variables declared as static in the class
 ********************************************************
 */



BLERemDev* BLERemDev::_activeRemDev = nullptr;  ///< The currently active connection

BLERemDev BLERemDev::_bleCon[MAX_REMOTE_CON];  // but only one used at the moment!


int BLERemDev::_bleConCount  = 0;
bool BLERemDev::_scanRepCBset = false;






/**
 @brief Construct the BLE client connection with given address
 
 Each client connection connects to a specifed server address (effectively pre-paired).  At the moment only one connection
 will be open at a time.  The first time the connection is opened after starting, service discovery is performed.  The address and address type are set up when the object it instantiated.
 
 @param remoteAdd - the hardware address of the server for this connection
 @param remoteAddType - the remote device's address type
 */
BLERemDev::BLERemDev(const uint8_t* remoteAdd,
                     const ble::peer_address_type_t remoteAddType)
{
    _peerAdd = ble::address_t(remoteAdd);   // save remote address
    _peerAddType = remoteAddType;           // save remote address type
//    _bleCore = bleCore;
    _countDA = 0;
    _localName = "unknown";
    _clientConState = CS_CONNECTABLE;  // available for connection
}

/**
 @brief Construct the BLE client connection without address
 
 Each client connection connects to a different server address. The address will be determined by scanning.
 At the moment only one connection
 will be open at a time.  The first time the connection is opened after starting, service discovery is performed.
 This constructs a void connection object.  It will be assigned to a specific remote device, as part of the scanning process.
 */
BLERemDev::BLERemDev()
{
    _peerAdd = ble::address_t();
//    _bleCore = bleCore;
    _countDA = 0;
    _clientConState = CS_INITIAL; // not yet scanned
}


/**
 @brief Setup
 
 Not used yet.
 */
void BLERemDev::setup()
{
}

/**
 @brief Provide the pointer to the active remote device
 
 This makes the current active remote device availble. Typically the current BLE peripheral peer.
 It returns nullptr if there is no device active.
 
 @return pointer to the current remote device.
 */
BLERemDev* BLERemDev::activeRemDev()
{
    return(_activeRemDev);
}




/**
 @brief Setup callback for advertising reports
 
 Set up the call back for managing advertising reports.  This need only be done once.  Scanning is done by the core
 and reports matching our service id are processed here.
 
 @note this is a static routine.
 */
void BLERemDev::setAdReporting()
{
    // call back is to a static routine - context not required
    BLEcore::instance().setScanEventCallback(_processScanReport);
}

/**
 @brief Connect to a remote device identified by index
 
 This connects to a remote device that has a connection record allocated as identified by the index.
 
 @note this is a static routine.
 */

bool BLERemDev::connectByIndex(int index)
{
    if ((index < _bleConCount) && (index >= 0))
    {
        return(_bleCon[index]._connect());
    }
    else
    {
        return(false);
    }
}

/**
 @brief Connect to a remote device identified by local name
 
 This connects to a remote device that has a connection record allocated as local name.
 
 @note this is a static routine.
 */

bool BLERemDev::connectByName(String& name)
{
    int i = 0;
    while ((getLocalNameByIndex(i) != name) &&
           (i < _bleConCount))
    {
        i++;
    }
    if (i < _bleConCount)
    {
        // we've found it
        return(_bleCon[i]._connect());
    }
    else
    {
        return(false);
    }
}


/**
 @brief Return number of known connections
 
 This returns the number of known client connections that have found by scanning.
 
 @return - the number of client connections.
 
 @note this is a static routine.
 */
int BLERemDev::getFoundCount()
{
    return(_bleConCount);
}

/**
 @brief Disconnnect the currently connected device
 
 This disconnects from the currently connected device.
 
 @note this is a static routine.
 */
void BLERemDev::disconnect()
{
    if (_activeRemDev != nullptr)
    {
        _activeRemDev->_disconn();
    }
}

/**
 @brief Get the device's local name
 
 Exposes the local name of the peer for the connection
 
 @return the local name as a string.
 */
String& BLERemDev::getLocalName()
{
    return(_localName);
}

/**
 @brief Get the device's local name by index
 
 Exposes the local name of the peer for the connection at the given index.
 
 @param i - connection index number
 @return the local name as a string.
 */
String& BLERemDev::getLocalNameByIndex(int i)
{
    return(_bleCon[i]._localName);
}

/**
 @brief Initiate connection to BLE server
 
 This inintiates a client connection to a BLE server (e.g accessory server) at the address
 supplied when constructed or as discovered by scanning
 
 @return true if connect initiatied without error
 */
bool BLERemDev::_connect()
{
    ble_error_t bleErr;
    //uint8_t conAdd[] = {0xDD, 0x51, 0x51, 0x99, 0xE3, 0xCE};
    using namespace ble;
 //   ble::address_t foundAddress;
 //   ble::peer_address_type_t foundAddressType;
#if DEBUG
        Serial.print(_localName);
        Serial.print(' ');
#endif
    // check ok to connect
    if ((_clientConState == CS_CONNECTABLE) ||
        (_clientConState == CS_DISCON ) ||
        (_clientConState == CS_ERR))
    {
        // initiate the connection - the connection process runs asynchronously
        // a callback is set up to monitor for completion.
        bleErr = BLE::Instance().gap().connect
        (
         _peerAddType,
         //peer_address_type_t::RANDOM,
         _peerAdd,
         //address_t(conAdd),
         ConnectionParameters()
         .setScanParameters(
                            phy_t::LE_1M,
                            scan_interval_t(millisecond_t(500)),
                            scan_window_t(millisecond_t(250))
                            )
         .setConnectionParameters(
                                  phy_t::LE_1M,
                                  conn_interval_t(millisecond_t(100)),
                                  conn_interval_t(millisecond_t(200)),
                                  slave_latency_t(0),
                                  supervision_timeout_t(millisecond_t(1000))
                                  )
         .setOwnAddressType(own_address_type_t::RANDOM)
         );
        
        if (bleErr != BLE_ERROR_NONE)
        {
            // initiation failed - parameter error or similar
#if DEBUG
            Serial.print("Connect fail: ");
            Serial.println(bleErr);
#endif
            return(false);
        }
        else
        {
            // connection initiated - set callback to pick up result
            BLEcore::instance().setConnectionCompleteCallback
            (
             mbed::callback(
                            this, &BLERemDev::_initServiceDiscovery));
            // and disconnection when it occurs
            BLEcore::instance().setDisconnectionCompleteCallback
            (
             mbed::callback(
                            this, &BLERemDev::_serverDisconnected));
            _activeRemDev = this;
#if DEBUG
            Serial.println("Connect initiated");
#endif
            return(true);
        }
    }
    else
    {
        // not a valid state to connect
#if DEBUG
        Serial.print("Wrong state:");
        Serial.println(_clientConState);
#endif
        
        return(false);
    }
}



/**
 @brief Disconnect from the  BLE server
 
 This terminates a client connection to a BLE server (e.g accessory server)
 */

void BLERemDev::_disconn()
{
    ble_error_t bleErr;
    bleErr = BLE::Instance().gap().disconnect
    (
     _connHandle,  ble::local_disconnection_reason_t::USER_TERMINATION
     );
    _activeRemDev = nullptr;
    if (bleErr == BLE_ERROR_NONE)
    {
        _clientConState = CS_DISCONNECTING; // disconnect in progress
    }
    else
    {
        _clientConState = CS_ERR;
    }
}
 
/**
 @brief Process the report resulting from a scan.
 
 Each time a periphal device is detected during a scan this function is called back to process the report.
 
 The scan may report the same device more than once.
 
 If the detected device is advertising our service and has not been previously reported it is added to the connection
 array.
 
 @note This is a static function
 */

void BLERemDev::_processScanReport(const ble::AdvertisingReportEvent& event)
{
    //ble_error_t bleErr;
    ble::AdvertisingDataParser advParser(event.getPayload());
    
    UUID::LongUUIDBytes_t foundUUID128;  // 128 bit representation of UUID
    UUID foundUUID = UUID();             // empty UUID
    String localName;
    int i;
    
    while (advParser.hasNext())
    {
        
        ble::AdvertisingDataParser::element_t field = advParser.next();
        size_t len = field.value.size();
        switch (field.type.value())
        {
            case ble::adv_data_type_t::COMPLETE_LOCAL_NAME:
                for (size_t x = 0; x < len; x++)
                {
                    localName.concat((char)field.value[x]);
                }
                break;
                
            case ble::adv_data_type_t::INCOMPLETE_LIST_128BIT_SERVICE_IDS:
            case ble::adv_data_type_t::COMPLETE_LIST_128BIT_SERVICE_IDS:
                
                for (size_t x = 0; x < len; x++)
                {
                    foundUUID128[x & 0xf] = field.value[x];
                }
                foundUUID = UUID(foundUUID128, UUID::LSB);
                break;
            default:
                // not interested in this field type
                break;
        } // end switch(field.type.value())
    }
    // all fields parsed

    if (foundUUID == BLEcore::getServUUID())
    {
#if DEBUG
        const ble::address_t& peerAddress = event.getPeerAddress();
        Serial.print("Accessory service peer found ");
        Serial.println(localName);
        Serial.print('\t');
        BLEcore::printUUID(foundUUID);
        Serial.print('\t');
        for (int i = 0; i < 6; i++)
        {
            if (peerAddress[i] < 16)
            {
                Serial.print('0');
            }
            Serial.print(peerAddress[i], HEX);
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
        // see if already known - if not set up the next free connection record
        // with local name, address and address type as determined from scan
        i = 0;
        while ((i < _bleConCount) && (localName != _bleCon[i]._localName))
        {
            i++;
        }
        if ((localName != _bleCon[i]._localName) && _bleConCount < MAX_REMOTE_CON)
        {
            // it's one not seen before and we have room for it
            _bleCon[i]._localName = localName;
            _bleCon[i]._peerAdd = event.getPeerAddress();
            _bleCon[i]._peerAddType = event.getPeerAddressType();
            _bleCon[i]._clientConState = CS_CONNECTABLE;
            _bleConCount++;  // increment number of known connections
            BLEcore::instance().queueReport(BLE_PEER_FOUND, i);
#if DEBUG
            Serial.print("New connection added ");
            Serial.println(i);
#endif
        }
    }
}
//******************
// this is the call back for when
// the connection has been made.
//
//
//
void BLERemDev::_initServiceDiscovery(const ble::connection_handle_t ch)
{
    switch (_clientConState)
    {
        case CS_CONNECTABLE:
        case CS_ERR:
            // service discovery has not been performed for this connection yet
            // or needs to be redone
            _connHandle = ch;
            //_nextDA = 0;  // first discovered accessory is next to be allocated
#if DEBUG
            Serial.println("Starting service discovery");
#endif
            
            // set callbacks to point at this connection
            // we will only have one connection discovering at a time
            //
            // the first stage is to discover services and their characteristics
            // this follows the tree structure i.e the callbacks return a service and
            // its characteristics before moving to the next service.
            _gattClient.onServiceDiscoveryTermination
            (
             ServiceDiscovery::TerminationCallback_t(
                                                     this,
                                                     &BLERemDev::_discoveryTermination
                                                     ));
            


            // start service discovery - callbacks are set to monitor progress
            _gattClient.launchServiceDiscovery
            (
             ch,
             ServiceDiscovery::ServiceCallback_t(
                                                 this,
                                                 &BLERemDev::_serviceDiscovered
                                                 ),
             ServiceDiscovery::CharacteristicCallback_t(
                                                        this,
                                                        &BLERemDev::_characDiscovered
                                                        ),
             UUID(BLE_UUID_UNKNOWN),  // matching service UUID
             UUID(BLE_UUID_UNKNOWN)     // matching characteristic UUID
             );
            
            _clientConState = CS_CON_FIRST; // Looking for first real service
            break;
            
        case CS_DISCON:
            // disconnected so reconnecting
            // discovery was done the first time
            // refresh callback
            // also need to set up notifications again
#if DEBUG
            Serial.println("Reconnecting : service discovery skipped");
            if (ch != _connHandle)
            {
                Serial.println("Reconnecting : connection handle changed");
            }
#endif
            _connHandle = ch;
            _remAcc = _firstRemAcc;
            _clientConState = CS_RECON_INIT; // set reconnect initialisation
            
            if(!_remAcc->initCharacteristics(_clientConState))
            {
                // if CCCD write fails do the next one
                _doNextDA();
            }
            
            // otherwise wait for write completion callback
            break;
            
        default:
#if DEBUG
            Serial.print("Service discovery invalid state:");
            Serial.println(_clientConState);
#endif
            break;
    }
}
// server disconnect callback - disconnect may have been issued locally,
// remotely or as result of
// a communications failure
void BLERemDev::_serverDisconnected(const ble::DisconnectionCompleteEvent& event)
{
    if (event.getConnectionHandle() == _connHandle)
    {
        
#if DEBUG
        Serial.print(_localName);
        Serial.print(" - Server Disconnected. Reason 0x");
        Serial.println(event.getReason().value(), HEX);
#endif
        _clientConState = CS_DISCON;
        // clear callbacks
        BLEcore::instance().setConnectionCompleteCallback(nullptr);
        BLEcore::instance().setDisconnectionCompleteCallback(nullptr);
        _queueRemAccReps(RA_DISCONNECTED, event.getReason().value());
    }
#if DEBUG
    else
    {
        Serial.println("Disconnect call back - wrong handle");
    }
#endif
}




//************************************************
//
// Service discovered callback
//
//************************************************
void BLERemDev::_serviceDiscovered(const DiscoveredService *service)
{
    // making a service shallow copy using the = operator is disabled in 6.9 and later
    // so we need to explicitly retain any service info needed later
    // see if the UUID is for a service we are interested in
    _serviceUUID = service->getUUID();
    if (_serviceUUID == BLEcore::getServUUID()) // is this a point service
    {
        // save the discovered service UUID and associate with this connection
        // remote accessories are created on the heap but never deleted so
        // heap fragmentation shouldn't be a problem
        _remAcc = new RemAccessory(_connHandle, _serviceUUID);
#if DEBUG
    Serial.print("Found Accessory Service:\n\t");
        BLEcore::printUUID(_serviceUUID);
#endif

        if (_clientConState == CS_CON_FIRST)  // This is the first service of interest
        {
            //_nextDA = 0;  // static next points to the next free record
            _countDA = 1;  //
            _firstRemAcc = _remAcc; // save the first one
            _clientConState = CS_CON_DISC;
        }
        else
        {
            _countDA++;
        }
    }
}

//************************************************
//
// Characteristic discovered callback
//
//************************************************

void BLERemDev::_characDiscovered(const DiscoveredCharacteristic *characteristic)
{
#if DEBUG
    Serial.print("Found characteristic:\n\t");
    BLEcore::printUUID(characteristic->getUUID());
    Serial.print("\tHandles D:");
    Serial.print(characteristic->getDeclHandle());
    Serial.print(" V:");
    Serial.print(characteristic->getValueHandle());
    Serial.print(" E:");
    Serial.println(characteristic->getLastHandle());
#endif
    if (_serviceUUID == UUID(BLE_UUID_GAP))
    {
        // this is generic GAP service
        if (characteristic->getUUID() == UUID(BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME))
        {
            // this characteristic's value is the name of the remote device
            _devNameCharac = *characteristic;
        }
    }
    else if (_serviceUUID == BLEcore::getServUUID())
    {
        // this is an accessory service
        // the characteristic will be saved if it's one of interest
        _remAcc->saveCharacteristic(characteristic);
    }
}


//************************************************
//
// Service discovery termination callback
//
//************************************************
//
// discovery of services and characteristics is complete
//
// we now read characteristic values to determine details about the found
// services.
// for each service we need to read its id characteristic and the current state characteristic
// if the characteristic has a CCCD, this has to be discovered and written
// to initiate notifications.
void BLERemDev::_discoveryTermination(const ble::connection_handle_t)
{
    ble_error_t bleErr;
    // set up data read and write callbacks
    _gattClient.onDataRead(
                           ble::ReadCallback_t(
                                               this,
                                               &BLERemDev::_dataRead
                                               ));
    // data written callback
    _gattClient.onDataWritten
    (
     ble::WriteCallback_t(this, &BLERemDev::_dataWritten));
    
    
    // first to be read is the device name
    bleErr = _devNameCharac.read();
    if (bleErr == BLE_ERROR_NONE)
    {
        _clientConState = CS_CON_INIT;
    }
    else
    {
        _clientConState = CS_ERR;
    }
#if DEBUG
    Serial.println("Discovery terminated.");
    Serial.print(_countDA);
    Serial.println(" accessory service(s) found.");
    
    if (bleErr == BLE_ERROR_NONE)
    {
        Serial.println("Reading initial values");
    }
    else
    {
        Serial.print("Start DevName read error:");
        Serial.println(bleErr);
    }
#endif
}
//************************************************
//
// Data read callback
//
//************************************************
//
// a data read has returned a result
// this could be as part of the initial service analysis or
// an application request.
// the handle identifies the characteristic for which a value has been
// read from the server.
// state variables indicte why the read was initiated
//
void BLERemDev::_dataRead(const GattReadCallbackParams* cbp)
{
    ble_error_t bleErr;
#if DEBUG
    if (cbp->status == BLE_ERROR_NONE)
    {
        Serial.print("Read - handles con:");
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
    }
#endif

    if (cbp->handle == _devNameCharac.getValueHandle())
    {
        // device name characteristic as requested at end of discovery
        // ****** not saved yet - setting it correctly at server end
        // seems problematic
        // - we use the one as returned by the scan!
        // now start reading accessories ids etc
        _remAcc = _firstRemAcc; // back to the first one
        //bleErr = _remAcc->readId();  // initiate the read for the first one
        if(!_remAcc->initCharacteristics(_clientConState))
        {

#if DEBUG
        //if (bleErr != BLE_ERROR_NONE)
        {
            Serial.print("Start DA read error(first):");
            Serial.println(bleErr);
        }
#endif
        }
    }
    // else see if accessory related id. characteristic we're expecting
    else if (cbp->handle == _remAcc->idValueHandle())
    {
        _remAcc->setRemAccId(cbp->data, cbp->len);
        bleErr = _remAcc->processDescrips
        (
         mbed::callback(this, &BLERemDev::_descripsDone)
         );

        if (bleErr != BLE_ERROR_NONE)
        {
#if DEBUG
            Serial.print("Start DA read state error:");
            Serial.println(bleErr);
#endif
        }

        _remAcc->queueReport(RA_DISCOVERED, 0);
    }
    else if (cbp->handle == _remAcc->stateValueHandle())
        // result of state read.  Use it to set the point state
    {
        _remAcc->setState((PointState_t)*(cbp->data));
        
        
        // now attempt the CCCD write so set interest in notifications
        if(!_remAcc->doCCCDwrite())
        {
#if DEBUG
            Serial.println("CCCD write fail");
#endif
            _clientConState = CS_ERR;
        }
    }
    
}



//************************************************
//
// data written callback
//
// a previous write request has terminated
//************************************************
void BLERemDev::_dataWritten(const GattWriteCallbackParams* cbp)
{
    //ble_error_t bleErr;
    //int i = 0;
    //bool daFound = false;
    Reporter* nextReporter = Reporter::getFirstReporter();
#if DEBUG
    Serial.print("Written - handles con:");
    Serial.print( cbp->connHandle);
    Serial.print(" attr:");
    Serial.println(cbp->handle);
    Serial.print(" status:");
    Serial.println(cbp->status);
    Serial.print(" error:");
    Serial.println(cbp->error_code);
#endif
    // pass data written event to all the accessories until one accepts it
    
    
    while ((nextReporter != nullptr) &&
           ((nextReporter->getType() != RA_REP) ||
            (!(((RemAccessory*)nextReporter)->dataWritten(cbp)))))
    {
        // not taken - look at next one
        nextReporter = nextReporter->getNextReporter();
    }
    if (((_clientConState == CS_CON_INIT) || // initial connection init or
         (_clientConState == CS_RECON_INIT)) && // re-connection init
        (nextReporter == (Reporter*) _remAcc)) // and it's the one being dealt with
    {
        
        _doNextDA();  // final action on this DA completed
    }
    // else do action for normal write complete - at the moment nothing
    // specific action may have been taken by the accessory already
}

//************************************************
//
// move on to the next Discovered Accessory
// and intiate first action on it
// This relies on the reporter chaining to be serial - any found after the first will need processing
// whereas any before it will not need processing
//************************************************
void BLERemDev::_doNextDA()
{
#if DEBUG
    Serial.print("Next DA - done ");
    Serial.print((char)_remAcc->getType());
    Serial.println(_remAcc->getId());
#endif
    Reporter* nextReporter = _remAcc->getNextReporter();  // start search from the one just done
    
    while ((nextReporter != nullptr) &&
           ((nextReporter->getType() != RA_REP) ||
            (!(((RemAccessory*)nextReporter)->initCharacteristics(_clientConState)))))
    {
        // unable to initiate processing characteristics - skip to next
        nextReporter = nextReporter->getNextReporter();
    }
    
    
    if (nextReporter  == nullptr)
    {
        // setting up discovered accessories complete
        // queue connected report for each of the remote accessories on this connection
        // but we do this last to ensure stack is now idle
        _queueRemAccReps(RA_CONNECTED, _connHandle);

#if DEBUG
        Serial.println("All DAs done");
#endif
        _clientConState = CS_CONNECTED; // set connected
        // and report that all service interrogation and setup complete
        BLEcore::instance().queueReport(BLE_SERVICES_AVAIL, 0);
    }
    else
    {
        _remAcc = (RemAccessory*)nextReporter;
#if DEBUG
        
        Serial.print("Next DA - found ");
        Serial.print((char)_remAcc->getType());
        Serial.println(_remAcc->getId());
#endif
    }
    // otherwise wait for appropriate action complete callback
}


// only called if description discovery terminated with error
void BLERemDev::_descripsDone()
{
#if DEBUG
        Serial.println("Descrips Done");
#endif
}
    
void BLERemDev::_queueRemAccReps(EventType repType, const int info)
{
    // search for the remote corresponding to this discovered accessory
    Reporter* nextReporter = Reporter::getFirstReporter();
    while (nextReporter != nullptr)
    {
        if ((nextReporter->getType() == RA_REP) &&
            (((RemAccessory*)nextReporter)->getConnHandle() ==
             _connHandle))
        {
            if(repType == RA_DISCONNECTED)
            {
                ((RemAccessory*)nextReporter)->setState(P_UNAVAIL);
            }
            nextReporter->queueReport(repType, info);
        }
            
            // no match - look at next one
        nextReporter = nextReporter->getNextReporter();
    }
}
