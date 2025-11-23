/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SensorNotifierUtils"

#include <android-base/logging.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

#include "SensorNotifierUtils.h"

bool readBool(int fd) {
    char c;
    int rc;

    rc = lseek(fd, 0, SEEK_SET);
    if (rc == -1) {
        LOG(ERROR) << "Failed to seek fd, errno: " << errno << " - " << strerror(errno);
        return false;
    }

    rc = read(fd, &c, sizeof(char));
    if (rc != 1) {
        LOG(ERROR) << "Failed to read bool from fd, errno: " << errno << " - " << strerror(errno);
        return false;
    }

    bool result = (c != '0');
    LOG(VERBOSE) << "Read bool value: " << result << " (char: '" << c << "')";
    return result;
}

std::shared_ptr<disp_event_resp> parseDispEvent(int fd) {
    disp_event header;
    
    if (lseek(fd, 0, SEEK_SET) == -1) {
        LOG(ERROR) << "Failed to seek display event fd, errno: " << errno << " - " << strerror(errno);
        return nullptr;
    }
    
    ssize_t headerSize = read(fd, &header, sizeof(header));
    if (headerSize != sizeof(header)) {
        LOG(ERROR) << "Unexpected display event header size: " << headerSize 
                   << ", expected: " << sizeof(header) << ", errno: " << errno << " - " << strerror(errno);
        return nullptr;
    }

    int dataLength = header.length - sizeof(header);
    if (dataLength < 0 || dataLength > 1024) { // Reasonable upper limit
        LOG(ERROR) << "Invalid data length: " << dataLength;
        return nullptr;
    }

    std::shared_ptr<disp_event_resp> response(
        static_cast<disp_event_resp*>(malloc(sizeof(disp_event) + dataLength)), 
        free
    );
    
    if (!response) {
        LOG(ERROR) << "Failed to allocate memory for display event response";
        return nullptr;
    }
    
    response->base = header;
    
    if (dataLength > 0) {
        ssize_t dataSize = read(fd, &response->data, dataLength);
        if (dataSize != dataLength) {
            LOG(ERROR) << "Unexpected display event data size: " << dataSize 
                       << ", expected: " << dataLength << ", errno: " << errno << " - " << strerror(errno);
            return nullptr;
        }
    }

    return response;
}