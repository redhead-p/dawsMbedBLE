/**
@file dawsDiscCli.cpp
@author Paul Redhead on 3/2/2021.
@copyright (C) 2021 Paul Redhead
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

#include "DiscoveredService.h"
#include "DiscoveredCharacteristic.h"
#include "CharacteristicDescriptorDiscovery.h"

#include "daws.h"
#include "dawsReporter.h"

#include "dawsBLE.h"
#include "dawsDiscCli.h"
#include "dawsRemAcc.h"
#include "dawsBLERemDev.h"






#define DEBUG false  ///< enable BLE debug output to IDE Monitor


const ReporterType DiscoveredAccCli::_type = RA_REP;

/**
 @brief Construct a discovered accessory
 
 This constructs the discovered accessory client as a bare template.  Detail is added during the discovery process.
 
 */
DiscoveredAccCli::DiscoveredAccCli() :Reporter(RA_REP)
{
    _serviceUUID = BLE_UUID_UNKNOWN;  // initially unknown until discovery undertaken
}
/**
 @brief Construct a discovered accessory with data
 
 This constructs the discovered accessory client with connection handle and UUID.  Further detail is added during the discovery process.
 
 */

DiscoveredAccCli::DiscoveredAccCli(ble::connection_handle_t ch, UUID uuid): Reporter(RA_REP)
{
    _connHandle = ch;
    _serviceUUID = uuid;
}



/**
 @brief Initialise the discovered service
 
 This adds information obtained as part of the discovery service to the discovered accessory server and sets
 the callback to be used when the server notifies a change of state
 
 @param ch - the connection handle for the peripheral remote device providing the service
 @param uuid - the service UUID
 
 */

void DiscoveredAccCli::initSvr(ble::connection_handle_t ch, UUID uuid)
{
    _connHandle = ch;
    _serviceUUID = uuid;

}

/**
@brief Get Reporter Type

 Retreives reporter type.

@return reporter identifier as an enum member.
*********************************/

ReporterType DiscoveredAccCli::getType()
{
    return(_type);
}


/**
 @brief Initialise Characteristics.
 
 This has two parts - reading the Accessory ID from the server and informing the service
 that we require notifications for state changes (updating the CCCD).
 
 If the remote device has already been discovered and we are reconnecting there is no
 need to read the ID again, but the CCCD still has to be updated.
 
 This routine initiates the actions required.  Call backs are used for action completion.
 
 @param connState - the connection state - if initial connect the ID read will be performed
 
 @return true if actions initiated OK, false if error
 */
bool DiscoveredAccCli::initCharacteristics(RemDevState_t connState)
{
    ble_error_t bleErr;

    if (connState == CS_CON_INIT)
    {
        // set up callback links for initial connection
        // no need to repeat on reconnection - should still be there!
        // first - callback to discovered accessory on notification
        _gattClient.onHVX
        ().add
        (
         GattClient::HVXCallback_t(this, &DiscoveredAccCli::_dataChange)
         );

        // initiate id read for next discovered accessory
        bleErr = _idDC.read();
#if DEBUG
        if (bleErr != BLE_ERROR_NONE)
        {
            Serial.print("Start DA read error(next):");
            Serial.println(bleErr);
        }
#endif
        return(bleErr == BLE_ERROR_NONE);
    }
    else
    {
        // reconnecting
        // issue read for the state characteristic
        return(_stateDC.read() == BLE_ERROR_NONE);
    }
    
}
/**
 @brief Update the Client Characteristic Configuration Descriptor
 
 This initiates a write to Characteristic Configuration Descriptor to mark that this client requires
 notifications when the state desciptor value is changed at the server.
 
 @return bool true if initiating the write was successfull.
 */


bool DiscoveredAccCli::doCCCDwrite()
{
    ble_error_t bleErr;
    GattAttribute::Handle_t h = _stateCCCDHandle;
  //  const uint16_t hvNotification = BLE_HVX_NOTIFICATION;
    uint16_t hvNotification = 1;
    
    if (h != GattAttribute::INVALID_HANDLE)
    {
        // if OK we will get a write callback
        bleErr = _gattClient.write(
                                   GattClient::GATT_OP_WRITE_REQ,
                                   _connHandle,
                                   h,
                                   sizeof(hvNotification),
                                   reinterpret_cast<uint8_t*>(&hvNotification));
#if DEBUG
        if (bleErr != BLE_ERROR_NONE)
        {
            Serial.print("Write CCCD error:");
            Serial.println(bleErr);
        }
        else
        {
            Serial.print("CCCD write issued - handles ");
            Serial.print(_connHandle);
            Serial.print(' ');
            Serial.println(h);
        }
#endif
    }
    return((h != GattAttribute::INVALID_HANDLE) &&
           (bleErr == BLE_ERROR_NONE));
}


/**
 @brief Save a discovered characteristic.
 
 If the discovered characteristic UUID matches one those known to us and identifies the discovered characteristic
 as being one that is of interest, it is saved as part of the discovered
 accessory.   The characteristics saved are the id, the state and command.  Discovered characteristics are saved as
 shallow copies.
 
 @param c - pointer to the discovered characteristic.
 
 @return the index of the discovered characteristic UUID in the match table
 */
uuid_t DiscoveredAccCli::saveCharacteristic(const DiscoveredCharacteristic* c)
{
    
    uuid_t u = BLEcore::matchUUID(c->getUUID()); // retrieve enumerated index
#if DEBUG
    Serial.print("   index:");
    Serial.print(u);
    Serial.print(" handle:");
    Serial.println(c->getDeclHandle());

    
    
#endif
    switch (u)
    {
        case ID_UUID:
            _idDC = *c;
#if DEBUG
            if (!c->getProperties().read())
            {
                Serial.println("State Characteristic should be read enabled");
            }
#endif

            break;
            
        case STATE_UUID:
            _stateDC = *c;
#if DEBUG
            if (!c->getProperties().notify())
            {
                Serial.println("State Characteristic should be notify enabled");
            }
#endif
            break;
            
        case CMD_UUID:
            _commandDC = *c;
#if DEBUG
            if (!c->getProperties().write())
            {
                Serial.println("Command Characteristic should be write enabled");
            }
#endif
            break;
            
        default:
#if DEBUG
            Serial.print("UUID not matched ");
            BLEcore::printUUID(c->getUUID());
#endif
            break;
    }
    return(u);
}

/**
 @brief Read Id of discovered accessory
 
 This issues the BLE command to read the value of the ID characteristic as discovered and held as part of the
 discovered accessory.
 
 @return BLE error code - BLE_ERROR_NONE if OK
 */
ble_error_t DiscoveredAccCli::readId()
{
    return(_idDC.read());
}

/**
 @brief Read State of discovered accessory
 
 This issues the BLE command to read the value of the state characteristic as discovered and held as part of the
 discovered accessory.
 
 @return BLE error code - BLE_ERROR_NONE if OK
 */
ble_error_t DiscoveredAccCli::readState()
{
    return(_stateDC.read());
}


/**
 @brief Exposes the id value handle
 
 This returns the value handle of the id discovered characteristic as held by the discovered accessory
 
 @return id value handle
 */
GattAttribute::Handle_t DiscoveredAccCli::idValueHandle()
{
    return(_idDC.getValueHandle());
}

/**
 @brief Data written callback
 
 This  checks the connection and characteristic handles to confirm that the callback is intended for this connection.  At the moment there are no additional actions
 here.
 
 @return true if the callback relates to this connection else false
 */

bool DiscoveredAccCli::dataWritten(const GattWriteCallbackParams* cbp)
{
    bool found = false;
    // this might not be for me!
    if (cbp->connHandle == _connHandle)
    {
        // it's the right connection
        found =  ((cbp->handle == _stateCCCDHandle) ||
                (cbp->handle == _commandDC.getValueHandle()) ||
                  (cbp->handle == _idDC.getValueHandle()) ||     // not writeable but mine
                  (cbp->handle == _stateDC.getValueHandle()));   // not writeable but mine
    }
    return(found);
}

/**
 @brief Characteristic value changed callback (HVX)
 
 A characteristic value has changed at the server and the change has
 been pushed to us here.  The characteristic has to have notify or indicate
 properties set. We have to have requested notifications or indications as
 appropriate.  All notifications will callback here. We have to check it's for
 one of our characteristics - at the moment just the state characteristic
 
 @param cbp - pointer to the structure holding parameters as passed to the callback routing
 */

void DiscoveredAccCli::_dataChange(const GattHVXCallbackParams* cbp)
{
    ble_error_t bleErr;
#if DEBUG
    Serial.print("Change - id:");
    Serial.print( getId());
    Serial.print(" handles con:");
    Serial.print( cbp->connHandle);
    Serial.print(" attr:");
    Serial.println(cbp->handle);
    Serial.print("Rx len:");
    Serial.println(cbp->len);
    for (int x = 0; x < cbp->len; x++)
    {
        if (*(cbp->data + x) < 16)
        {
            Serial.print('0');  // print leading 0
        }
        Serial.print(*(cbp->data + x), HEX);
    }
    Serial.println();
#endif
    // at the moment we only expect notifications for state changes
    
    if ((cbp->connHandle == _connHandle) &&  // check it's our connection
        (cbp->handle == _stateDC.getValueHandle())) // and state value changed
    {
        //
        newState((PointState_t)(*(cbp->data)));  // execute virtual function
        
    }

}


/**
 @brief Process characteristic descriptions
 
 This discovers additional metadata regarding a characteristic.  We only do this for characteristics where we
 expect to receive notification when the value changes at the server as we need the handle for the CCCD to
 request the notifications.
 
 @param cb - the callback to be executed once characteristic description discovery is complete.
 
 @return the BLE error code (always none)
 */
ble_error_t DiscoveredAccCli::processDescrips
(
 mbed::Callback<void()> cb
 )
{
    _descripsDoneCB = cb;  // save callback for when we're done
    _stateCCCDHandle = GattAttribute::INVALID_HANDLE;  // set invalid
    _stateDC.discoverDescriptors
    (
     CharacteristicDescriptorDiscovery::DiscoveryCallback_t
     (
      this,
      &DiscoveredAccCli::_descripDisc),
     CharacteristicDescriptorDiscovery::TerminationCallback_t
     (
      this,
      &DiscoveredAccCli::_ddDone)
     );
    
    return(BLE_ERROR_NONE);
}




/**
 @brief Save  the service UUID
 
 This saves the service UUID for this accessory server
 
 @param u - service UUID
 */
void DiscoveredAccCli::setConServUUID(UUID u)
{
  //  _clientCon = con;  // save owning connection
    _serviceUUID = u;
}
/**
 @brief Exposes the service UUID
 
 This exposes the service UUID associated with this Discovererd Accessory.
 
 @return the service UUID
 */
UUID DiscoveredAccCli::getServUUID()
{
    return(_serviceUUID);
}
/**
 @brief Write command to a discovered accessory
 
 This sends a command to a discovered accessory.  The command is sent by writing to the
 value of the accessory's command characteristic. The write is initiated here.  The server response to
 the write is processed later via the connection 'onDataWritten' callback.
 
 @param cmd - command character to be written
 
 @return true if write initiated successfully
 
 */
bool DiscoveredAccCli::writeCommand(const uint8_t cmd)
{
    ble_error_t bleErr;
    bleErr = _commandDC.write(1, &cmd);
#if DEBUG
    if (bleErr != BLE_ERROR_NONE)
    {
        Serial.print("Write command error:");
        Serial.println(bleErr);
    }
#endif
    return(bleErr == BLE_ERROR_NONE);
}

void DiscoveredAccCli::_descripDisc
(
 const CharacteristicDescriptorDiscovery::DiscoveryCallbackParams_t* cbp)
{
#if DEBUG
    Serial.print("Found descriptor: parent UUID ");
    BLEcore::printUUID(cbp->characteristic.getUUID());
    Serial.print("     UUID ");
    BLEcore::printUUID(cbp->descriptor.getUUID());
#endif
    // is this the CCCD?
    if (cbp->descriptor.getUUID() == UUID(BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG))
    {
        // save the handle so we can write to it
        _stateCCCDHandle = cbp->descriptor.getAttributeHandle();
#if DEBUG
        Serial.print("     CCCD handle ");
        Serial.println(_stateCCCDHandle);
#endif
    }
}

void DiscoveredAccCli::_ddDone
 (const CharacteristicDescriptorDiscovery::TerminationCallbackParams_t* cbp)
{
#if DEBUG
    if (cbp->status != BLE_ERROR_NONE)
    {
        Serial.print("Descriptor discovery termination status ");
        Serial.println( cbp->status);
    }
    if (_stateCCCDHandle == GattAttribute::INVALID_HANDLE)
    {
        Serial.print("State characteristic CCCD not found");
        
    }
#endif
    if (_stateCCCDHandle != GattAttribute::INVALID_HANDLE)
    {
//        if(!_doCCCDwrite())
        if(_stateDC.read() != BLE_ERROR_NONE)
        {
            // if error finished discovery for this write
            _descripsDoneCB();
        }
    }
    
}

/**
 @brief Expose the connection handle
 
 This make the connection handle of the remote device that hosts this disoverered service available.
 @return the connection handle for the associated remote connection.
 */

ble::connection_handle_t DiscoveredAccCli::getConnHandle()
{
    return(_connHandle);
}


/**
 @brief Expose the value handle for the state characteristic
 
 This makes the handle for the value attribute of the state characteristic available.
 @return the handle of the state characteristic value.
 */
GattAttribute::Handle_t DiscoveredAccCli::stateValueHandle()
{
    return(_stateDC.getValueHandle());
}


