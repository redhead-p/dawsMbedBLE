/**
@file dawsRemAcc.h
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

#ifndef ____dawsRemAcc__
#define ____dawsRemAcc__



/**
 @brief The remote accessory
 
 This provides the application level interface to a remote accessory, providing BLE client access to the server side accessory.
 
 At the moment this allows for point control.  It may be extended to allow for any accessory that follows the id, command & state model.

 
 @see DiscoveredAccCli
 */


class RemAccessory: public DiscoveredAccCli
{
public:
    RemAccessory();
    RemAccessory(char*);
    RemAccessory(ble::connection_handle_t, UUID);

    const char* getRemAccId();
    void setRemAccId(const uint8_t*, uint16_t);

    bool setPoint(PointPos_t);
    void newState(PointState_t) override;
    void setState(PointState_t) override;
    PointState_t getState();
    
    static RemAccessory* findRemAccById(const String);
    
private:
    char _remAccId[MAX_ID_SIZE];  // name of the associated Remote Accessory service
    
    PointPos_t _cmd;         // last received command
    PointState_t _reportedState;    // the reported state
};



#endif /* defined(____dawsRemAcc__) */
