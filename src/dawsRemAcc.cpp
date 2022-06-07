/**
@file dawsRemAcc.cpp
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





/**
 @brief Construct the Remote Accessory object using an id
 
 This constructs the remote accessory using the supplied id.  Deprecated.
 @param id - Accessory id as string.
 */
RemAccessory::RemAccessory(char* id)
{
    if (strlen(id) < MAX_ID_SIZE)
    {
    strcpy(_remAccId, id);
    }
    else
    {
        strncpy(_remAccId, id, MAX_ID_SIZE - 1);
        _remAccId[MAX_ID_SIZE - 1] = '\0';
    }
    _reportedState = P_UNAVAIL;  // we won't be connected yet!
}

/**
 @brief Construct the Remote Accessory object
 
 This constructs the remote accessory using the suppled connection handle and service UUID.
 Further information is added during discovery.
 
 @param ch - connection handle
 @param uuid - The service UUID.
 */
RemAccessory::RemAccessory(ble::connection_handle_t ch, UUID uuid):
DiscoveredAccCli(ch, uuid)
{
    _reportedState = P_UNAVAIL;  // we won't be fully discovered yet!
}


/**
 @brief Find Remote Accessory by its id
 
 This searches the through the reporter linkage looking for a remote accessory with the supplied id. If not found
 nullptr is returned.
 
 @param id - the remote accessory id as a String
 
 @return pointer the remote accessory
 */

RemAccessory* RemAccessory::findRemAccById(const String id)
{
    Reporter* nextReporter = Reporter::getFirstReporter();
    while ((nextReporter != nullptr) && // not last and
           // match conditions: type is Remote Accessory
           // strings match
           ((nextReporter->getType() != RA_REP) || //
            (String(((RemAccessory*)nextReporter)->getRemAccId()) == id)))
    {
        // no match - look at next one
        nextReporter = nextReporter->getNextReporter();
    }
    return ((RemAccessory*)nextReporter);
}

/**
 @brief Set the point
 
 This permits the client (central) to initate a point movement at the server (peripheral), by requesting
 a change to the required state. The associated discovered accessory forwards the command by changing
 the command attribute value.
 
 This is processed asynchonously.  The server will change the state to INDETERMINATE when movement starts and
 the requested state when movement completes.
 
 @param pCom - the required point state.
 
 @return true if command accepted - false if rejected
 */
bool RemAccessory::setPoint(PointPos_t pCom)
{
    bool done;
    if (_reportedState == P_UNAVAIL)
    {
        done = false;  // don't attempt write if no connection
    }
    else
    {
        switch (pCom)
        {
            case POINT_NORMAL:
            case POINT_REVERSE:
                _cmd = pCom;
                done = writeCommand(pCom);
                break;
            default:
                done = false;
#if DEBUG
                Serial.println("Attempt to set invalid pState");
#endif
                break;
        }
    }
    return(done);
}

/**
 @brief set the point state to an updated value
 
 This function is intended to be used as a callback from the discovered accessory to update
 the point's state when the discovered accessory is notified of a change to the state chararcteristic.
 
 It updates the state and raises a report
 
 @param newState - the updated state characteristic value as notified
 */
void RemAccessory::newState(PointState_t newState)
{
    _reportedState = newState;
    queueReport(RA_STATE_CHANGE, newState);
}

/**
 @brief set the point state to an initial value
 
 This function is intended to be used as a callback from the discovered accessory to update
 the point's state when the discovered accessory is first read following connection or when connection to
 remote service is lost. It doesn't raise a report
 as we don't when the state actually changed!
 
 @param newState - the updated state characteristic value as read
 */
void RemAccessory::setState(PointState_t newState)
{
    _reportedState = newState;
}

/**
 @brief get the point state
 
 This function returns the last known state of a remotely connected point.
 E.g one that is physically connected a remote accessory controller connected via BLE.
 
 @note the remote device is not interrogated to obtain this. 
 
 @return the point state as an enumeration PointState_t
 */

PointState_t RemAccessory::getState()
{
    return(_reportedState);
}
/**
 @brief get Remote Acc ID
 This exposes the Remote Acc id string as set.
 */
const char* RemAccessory::getRemAccId()
{
    return _remAccId;
}
/**
 @brief set teh Remote Acc ID
 This sets the Remote Acc id as determined by BLE discovery.
 The string is truncated if too long.
 
 @param id - pointer the id string
 @param len - length of the id string
 */
void RemAccessory::setRemAccId(const uint8_t* id, uint16_t len)
{
    // if string is too long (it shouldn't be) it's truncated
    memcpy(_remAccId, id, (len >= MAX_ID_SIZE)?MAX_ID_SIZE - 1:len);
    _remAccId[(len >= MAX_ID_SIZE)?MAX_ID_SIZE - 1:len] = '\0';  // terminate string
}
