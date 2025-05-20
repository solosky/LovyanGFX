/*----------------------------------------------------------------------------/
  Lovyan GFX - Graphics library for embedded devices.

Original Source:
 https://github.com/lovyan03/LovyanGFX/

Licence:
 [FreeBSD](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt)

Author:
 [lovyan03](https://twitter.com/lovyan03)

Contributors:
 [ciniml](https://github.com/ciniml)
 [mongonta0716](https://github.com/mongonta0716)
 [tobozo](https://github.com/tobozo)
/----------------------------------------------------------------------------*/
#pragma once

#include "../../misc/DataWrapper.hpp"

#include "hpm_csr_drv.h"
#include "hpm_clock_drv.h"
#include "hpm_gpio_drv.h"

#include <stdlib.h>

namespace lgfx
{
  inline namespace v1
  {
    //----------------------------------------------------------------------------

    __attribute__((unused)) static inline unsigned long millis(void)
    {
      uint64_t current_ms = (hpm_csr_get_core_cycle() * 1000) / clock_get_frequency(clock_cpu0);
      return (uint32_t)current_ms;
    }
    __attribute__((unused)) static inline unsigned long micros(void)
    {
      uint64_t current_micros = hpm_csr_get_core_cycle() / clock_get_frequency(clock_cpu0);
      return current_micros;
    }
    __attribute__((unused)) static inline void delay(unsigned long milliseconds)
    {
      clock_cpu_delay_ms(milliseconds);
    }
    __attribute__((unused)) static void delayMicroseconds(unsigned int us)
    {
      clock_cpu_delay_us(us);
    }

    static inline void *heap_alloc(size_t length) { return malloc(length); }
    static inline void *heap_alloc_psram(size_t length) { return malloc(length); }
    static inline void *heap_alloc_dma(size_t length) { return malloc(length); }
    static inline void heap_free(void *buf) { free(buf); }
    static inline bool heap_capable_dma(const void *ptr) { return false; }

    static inline volatile void gpio_write(int_fast8_t pin, uint32_t state)
    {
      gpio_write_pin(HPM_GPIO0, GPIO_GET_PORT_INDEX(pin),
                     GPIO_GET_PIN_INDEX(pin), state);
    }

    static inline void gpio_hi(int_fast8_t pin)
    {
      gpio_write(pin, 1);
    }
    static inline void gpio_lo(int_fast8_t pin)
    {
      gpio_write(pin, 0);
    }
    static inline bool gpio_in(int_fast8_t pin)
    {
      return gpio_read_pin(HPM_GPIO0, GPIO_GET_PORT_INDEX(pin), GPIO_GET_PIN_INDEX(pin));
    }

    enum pin_mode_t
    {
      output,
      input,
      input_pullup,
      input_pulldown
    };

    void pinMode(int_fast16_t pin, pin_mode_t mode);
    inline void lgfxPinMode(int_fast16_t pin, pin_mode_t mode)
    {
      pinMode(pin, mode);
    }

    //----------------------------------------------------------------------------
    struct FileWrapper : public DataWrapper
    {
      FileWrapper() : DataWrapper() { need_transaction = true; }

#if defined(ARDUINO) && defined(__SEEED_FS__)

      fs::File _file;
      fs::File *_fp;

      fs::FS *_fs = nullptr;
      void setFS(fs::FS &fs)
      {
        _fs = &fs;
        need_transaction = false;
      }
      FileWrapper(fs::FS &fs) : DataWrapper(), _fp(nullptr) { setFS(fs); }
      FileWrapper(fs::FS &fs, fs::File *fp) : DataWrapper(), _fp(fp) { setFS(fs); }

      bool open(fs::FS &fs, const char *path)
      {
        setFS(fs);
        return open(path);
      }

      bool open(const char *path) override
      {
        fs::File file = _fs->open(path, "r");
        // この邪悪なmemcpyは、Seeed_FSのFile実装が所有権moveを提供してくれないのにデストラクタでcloseを呼ぶ実装になっているため、
        // 正攻法ではFileをクラスメンバに保持できない状況を打開すべく応急処置的に実装したものです。
        memcpy(&_file, &file, sizeof(fs::File));
        // memsetにより一時変数の中身を吹っ飛ばし、デストラクタによるcloseを予防します。
        memset(&file, 0, sizeof(fs::File));
        _fp = &_file;
        return _file;
      }

      int read(uint8_t *buf, uint32_t len) override { return _fp->read(buf, len); }
      void skip(int32_t offset) override { seek(offset, SeekCur); }
      bool seek(uint32_t offset) override { return seek(offset, SeekSet); }
      bool seek(uint32_t offset, SeekMode mode) { return _fp->seek(offset, mode); }
      void close() override { _fp->close(); }
      int32_t tell(void) override { return _fp->position(); }

#else // dummy.

      bool open(const char *) override { return false; }
      int read(uint8_t *, uint32_t) override { return 0; }
      void skip(int32_t) override {}
      bool seek(uint32_t) override { return false; }
      bool seek(uint32_t, int) { return false; }
      void close() override {}
      int32_t tell(void) override { return 0; }

#endif
    };

    //----------------------------------------------------------------------------
  }
}
