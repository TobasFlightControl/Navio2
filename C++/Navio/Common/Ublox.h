/*
GPS driver dcode is placed under the BSD license.
Written by Egor Fedorov (egor.fedorov@emlid.com)
Copyright (c) 2014, Emlid Limited
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Emlid Limited nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL EMLID LIMITED BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// #define _XOPEN_SOURCE 600
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <string>
#include <stdint.h>
#include <vector>
#include "SPIdev.h"

#include "./nav_payloads.hpp"

#define PACKED __attribute__((__packed__))

static const int UBX_BUFFER_LENGTH = 1024;

class UBXScanner
{
public:
  enum State
  {
    Sync1,
    Sync2,
    Class,
    ID,
    Length1,
    Length2,
    Payload,
    CK_A,
    CK_B,
    Done,
  };

  unsigned char message[UBX_BUFFER_LENGTH];  // Buffer for UBX message
  unsigned int message_length;               // Length of the received message

private:
  unsigned int position;        // Indicates current buffer offset
  unsigned int payload_length;  // Length of current message payload
  State state;                  // Current scanner state

public:
  UBXScanner();
  unsigned char* getMessage();
  unsigned int getMessageLength();
  unsigned int getPosition();
  void reset();
  int update(unsigned char data);
};

class UBXParser
{
private:
  UBXScanner* scanner;     // pointer to the scanner, which finds the messages in the data stream
  unsigned char* message;  // pointer to the scanner's message buffer
  unsigned int length;     // current message length
  unsigned int position;   // current message end position

  uint16_t latest_id_;

public:
  UBXParser(UBXScanner* ubxsc);
  void updateMessageData();
  uint16_t calcId();
  int checkMessage();

  void decode(NavPayload_STATUS& data);
  void decode(NavPayload_POSLLH& data);
  void decode(NavPayload_VELNED& data);
  void decode(NavPayload_PVT& data);
  void decode(NavPayload_COV& data);
};

class Ublox
{
public:
  enum message_t
  {
    NAV_STATUS = (0x01 << 8) + 0x03,
    NAV_POSLLH = (0x01 << 8) + 0x02,
    NAV_VELNED = (0x01 << 8) + 0x12,
    NAV_PVT = (0x01 << 8) + 0x07,
    NAV_COV = (0x01 << 8) + 0x36,
  };

private:
  enum ubx_protocol_bytes
  {
    PREAMBLE1 = 0xb5,
    PREAMBLE2 = 0x62,

    CLASS_CFG = 0x06,

    MSG_CFG_RATE = 0x08
  };

  struct PACKED CfgNavRate
  {
    std::uint16_t measure_rate;
    std::uint16_t nav_rate;
    std::uint16_t timeref;
  };

  struct PACKED UbxHeader
  {
    std::uint8_t preamble1;
    std::uint8_t preamble2;
    std::uint8_t msg_class;
    std::uint8_t msg_id;
    std::uint16_t length;
  };

  struct PACKED CheckSum
  {
    uint8_t CK_A;
    uint8_t CK_B;
  };

  std::string spi_device_name;
  UBXScanner* scanner;
  UBXParser* parser;

public:
  Ublox(std::string name = "/dev/spidev0.0");
  Ublox(std::string name, UBXScanner* scan, UBXParser* pars);
  int enableNAV(message_t msg);
  int disableNAV(message_t msg);
  int testConnection();
  /* 32.10.27.1 Navigation/measurement rate settings */
  int configureSolutionRate(
    std::uint16_t meas_rate,
    std::uint16_t nav_rate = 1,
    std::uint16_t timeref = 0);
  uint16_t update();

  void decode(NavPayload_STATUS& data);
  void decode(NavPayload_POSLLH& data);
  void decode(NavPayload_VELNED& data);
  void decode(NavPayload_PVT& data);
  void decode(NavPayload_COV& data);

private:
  int _sendMessage(std::uint8_t msg_class, std::uint8_t msg_id, void* msg, std::uint16_t size);
  int _spliceMemory(unsigned char* dest, const void* const src, size_t size, int dest_offset = 0);
  /* p.171, 32.4 UBX Checksum */
  CheckSum _calculateCheckSum(unsigned char message[], std::size_t size);
};  // end of ublox class def
