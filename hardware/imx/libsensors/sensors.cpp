/*
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "LightSensor.h"

static const struct sensor_t sSensorList[] = {
    { 
        "Light sensor",
        "Freescale Semiconductor Inc.",
        1,
        SENSORS_LIGHT_HANDLE,
        SENSOR_TYPE_LIGHT, 32800.0f, 1.0f, 0.35f, 0, { } 
    },
};

struct sensors_poll_context_t {
private:
    enum {
        light = 0,
    };

    int handleToDriver(int handle) const {
        case ID_L:
            return light;
    }
};

sensors_poll_context_t::sensors_poll_context_t() {
    mSensors[light] = new LightSensor();
    mPollFds[light].fd = mSensors[light]->getFd();
    mPollFds[light].events = POLLIN;
    mPollFds[light].revents = 0;
}
