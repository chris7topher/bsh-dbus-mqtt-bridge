#ifndef CRC16_H
#define CRC16_H

#include <Arduino.h>

class CRC16 {
public:
  CRC16(uint16_t polynome, uint16_t initial, uint16_t xorOut, bool revIn, bool revOut)
    : _polynome(polynome), _initial(initial), _xorOut(xorOut), _revIn(revIn), _revOut(revOut) {
    restart();
  }

  void restart() {
    _crc = _initial;
  }

  void add(uint8_t b) {
    if (_revIn) b = _reverse(b);
    _crc ^= (uint16_t)b << 8;
    for (uint8_t i = 0; i < 8; i++) {
      if (_crc & 0x8000)
        _crc = (_crc << 1) ^ _polynome;
      else
        _crc = (_crc << 1);
    }
  }

  uint16_t calc() {
    uint16_t res = _crc;
    if (_revOut) res = _reverse16(res);
    return res ^ _xorOut;
  }

private:
  uint16_t _crc, _polynome, _initial, _xorOut;
  bool _revIn, _revOut;

  uint8_t _reverse(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
  }
  uint16_t _reverse16(uint16_t x) {
    x = (x & 0xFF00) >> 8 | (x & 0x00FF) << 8;
    x = (x & 0xF0F0) >> 4 | (x & 0x0F0F) << 4;
    x = (x & 0xCCCC) >> 2 | (x & 0x3333) << 2;
    x = (x & 0xAAAA) >> 1 | (x & 0x5555) << 1;
    return x;
  }
};

// XMODEM-Parameter
#define CRC16_XMODEM_POLYNOME 0x1021
#define CRC16_XMODEM_INITIAL  0x0000
#define CRC16_XMODEM_XOR_OUT  0x0000
#define CRC16_XMODEM_REV_IN   false
#define CRC16_XMODEM_REV_OUT  false

#endif
