/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2011-2012 Freescale Semiconductor, Inc.
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include "LightSensor.h"

#define SENSOR_DEBUG 1

#ifdef SENSOR_DEBUG
#define DEBUG(format, ...) ALOGD((format), ## __VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

/*****************************************************************************/
LightSensor::LightSensor()
    : SensorBase(NULL, "lightsensor-level"),
    mEnabled(1),
    mInputReader(4),
    mHasPendingEvent(false),
    mThresholdLux(10)
{
    char  buffer[PROPERTY_VALUE_MAX];

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_L;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    if (data_fd >= 0) {
        property_get("ro.hardware.lightsensor", buffer, "0");
        strcpy(ls_sysfs_path, buffer);
        DEBUG("LightSensor::LightSensor()    ls_sysfs_path %s\n", ls_sysfs_path);
        ls_sysfs_path_len = strlen(ls_sysfs_path);
        enable(0, 1);
    }

    /* Default threshold lux is 10 if ro.lightsensor.threshold
       isn't set */
    property_get("ro.lightsensor.threshold", buffer, "10");
    mThresholdLux = atoi(buffer);
}

LightSensor::~LightSensor() {
    if (mEnabled) {
        enable(0, 0);
    }
}

int LightSensor::setDelay(int32_t handle, int64_t ns)
{
    //dummy due to not support in driver....
    return 0;
}

int LightSensor::enable(int32_t handle, int en)
{
    return 0;
}


int LightSensor::setIntLux()
{
    FILE *fd = NULL;
    char buf[6];
    int n, lux, int_ht_lux, int_lt_lux;

    /* Read current lux value firstly, then change Delta value */
    strcpy(&ls_sysfs_path[ls_sysfs_path_len], "lux");
    if ((fd = fopen(ls_sysfs_path, "r")) == NULL) {
        ALOGE("Unable to open %s\n", ls_sysfs_path);
        return -1;
    }
    memset(buf, 0, 6);
    if ((n = fread(buf, 1, 6, fd)) < 0) {
        ALOGE("Unable to read %s\n", ls_sysfs_path);
        return -1;
    }
    fclose(fd);

    lux = atoi(buf);
    return 0;
}
bool LightSensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    if (mHasPendingEvent) {
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        return mEnabled ? 1 : 0;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            if (event->code == EVENT_TYPE_LIGHT) {
                mPendingEvent.light = event->value;
                setIntLux();
            }
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = timevalToNano(event->time);
            if (mEnabled && (mPendingEvent.light != mPreviousLight)) {
                *data++ = mPendingEvent;
                count--;
                numEventReceived++;
                mPreviousLight = mPendingEvent.light;
            }
        } else {
            ALOGE("LightSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

void LightSensor::processEvent(int code, int value)
{
}
