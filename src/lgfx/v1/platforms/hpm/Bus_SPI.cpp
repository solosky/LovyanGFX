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

#include "Bus_SPI.hpp"
#include "../../misc/pixelcopy.hpp"
#include "hpm_spi.h"
#include "hpm_dma_mgr.h"

namespace lgfx
{
  inline namespace v1
  {
    //----------------------------------------------------------------------------

    void Bus_SPI::config(const config_t &config)
    {
      _cfg = config;
    }

    bool Bus_SPI::init(void)
    {
      lgfx::pinMode(_cfg.pin_dc, pin_mode_t::output);

      spi_initialize_config_t init_config;

      // board_init_spi_clock(TEST_SPI);
      /* pins init*/
      // board_init_spi_pins_with_gpio_as_cs(TEST_SPI);
      dma_mgr_init();
      hpm_spi_get_default_init_config(&init_config);
      init_config.mode = spi_master_mode;
      init_config.clk_phase = spi_sclk_sampling_odd_clk_edges;
      init_config.clk_polarity = spi_sclk_low_idle;
      init_config.data_len = 8;
      /* step.1  initialize spi */
      if (hpm_spi_initialize(_cfg.spi_type, &init_config) != status_success)
      {
        printf("hpm_spi_initialize fail\n");
        return false;
      }
      /* step.2  set spi sclk frequency for master */
      if (hpm_spi_set_sclk_frequency(_cfg.spi_type, _cfg.freq_write) != status_success)
      {
        printf("hpm_spi_set_sclk_frequency fail\n");
        return false;
      }
      return true;
    }

    void Bus_SPI::release(void)
    {
    }

    void Bus_SPI::beginTransaction(void)
    {
      // SPISettings setting(_cfg.freq_write, BitOrder::MSBFIRST, _cfg.spi_mode, true);
      // SPI.beginTransaction(setting);
    }

    void Bus_SPI::endTransaction(void)
    {
      // dc_control(true);
      // SPI.endTransaction();
    }

    void Bus_SPI::beginRead(void)
    {
      // SPI.endTransaction();
      // SPISettings setting(_cfg.freq_read, BitOrder::MSBFIRST, _cfg.spi_mode, false);
      // SPI.beginTransaction(setting);
    }

    void Bus_SPI::endRead(void)
    {
      // SPI.endTransaction();
      // beginTransaction();
    }

    void Bus_SPI::wait(void)
    {
      wait_spi();
    }

    bool Bus_SPI::busy(void) const
    {
      return spi_is_active(_cfg.spi_type);
    }

    bool Bus_SPI::writeCommand(uint32_t data, uint_fast8_t bit_length)
    {
      if (0 == (bit_length >> 3))
      {
        return true;
      }
      dc_control(false);
      if (hpm_spi_transmit_blocking(_cfg.spi_type, (uint8_t *)&data, bit_length >> 3, 200) != status_success)
      {
        printf("writeCommand failed\n");
        return false;
      }
      return true;
    }

    void Bus_SPI::writeData(uint32_t data, uint_fast8_t bit_length)
    {
      if(0 == (bit_length >> 3))
      {
        return;
      }
      dc_control(true);
      if (hpm_spi_transmit_blocking(_cfg.spi_type, (uint8_t *)&data, bit_length >> 3, 200) != status_success)
      {
        printf("writeData failed\n");
      }
    }

    void Bus_SPI::writeDataRepeat(uint32_t data, uint_fast8_t bit_length, uint32_t length)
    {
      /*
          auto spisr    = &_cfg.spi_port->SR;
          auto spidr = &_cfg.spi_port->DR;
          size_t bytes = bit_length >> 3;
          if (bytes == 3)
          {
            uint32_t surplus = length & 3;
            if (surplus)
            {
              length -= surplus;
              do
              {
                writeData(data, bit_length);
              } while (--surplus);
            }
            if (0 == length) return;
            uint32_t buf[3];
            buf[0] = data       | data << 24;
            buf[1] = data >>  8 | data << 16;
            buf[2] = data >> 16 | data <<  8;
            dc_control(true);
            do
            {
              do {} while (!(*spisr & sr_mask));
              *spidr = buf[0];
              do {} while (!(*spisr & sr_mask));
              *spidr = buf[1];
              do {} while (!(*spisr & sr_mask));
              *spidr = buf[2];
            } while (--length);
          }
          else
          {
            do
            {
              writeData(data, bit_length);
            } while (--length);
          }
      /*/
      const uint8_t dst_bytes = bit_length >> 3;
      uint32_t limit = (dst_bytes == 3) ? 12 : 16;
      auto dmabuf = _flip_buffer.getBuffer(1024);
      size_t fillpos = 0;
      reinterpret_cast<uint32_t *>(dmabuf)[0] = data;
      fillpos += dst_bytes;
      uint32_t len;
      do
      {
        len = ((length - 1) % limit) + 1;
        if (limit <= 512)
          limit <<= 1;

        while (fillpos < len * dst_bytes)
        {
          memcpy(&dmabuf[fillpos], dmabuf, fillpos);
          fillpos += fillpos;
        }

        writeBytes(dmabuf, len * dst_bytes, true, true);
      } while (length -= len);
      //*/
    }

    void Bus_SPI::writePixels(pixelcopy_t *param, uint32_t length)
    {
      const uint8_t dst_bytes = param->dst_bits >> 3;
      uint32_t limit = (dst_bytes == 3) ? 12 : 16;
      uint32_t len;
      do
      {
        len = ((length - 1) % limit) + 1;
        if (limit <= 512)
          limit <<= 1;
        auto dmabuf = _flip_buffer.getBuffer(len * dst_bytes);
        param->fp_copy(dmabuf, 0, len, param);
        writeBytes(dmabuf, len * dst_bytes, true, true);
      } while (length -= len);
    }

    void Bus_SPI::writeBytes(const uint8_t *data, uint32_t length, bool dc, bool use_dma)
    {
      dc_control(dc);
      if (hpm_spi_transmit_blocking(_cfg.spi_type, (uint8_t *)data, length, 200) != status_success)
      {
        printf("writeBytes failed\n");
      }
    }

    uint32_t Bus_SPI::readData(uint_fast8_t bit_length)
    {
      uint32_t res = 0;
      if (0 == (bit_length >> 3))
      {
        return true;
      }

      if (hpm_spi_receive_blocking(_cfg.spi_type, (uint8_t *)res, bit_length >> 3, 200) != status_success)
      {
        printf("writeBytes failed\n");
      }
      return res;
    }

    bool Bus_SPI::readBytes(uint8_t *dst, uint32_t length, bool use_dma)
    {
      if (hpm_spi_receive_blocking(_cfg.spi_type, dst, length, 200) != status_success)
      {
        printf("readBytes failed\n");
        return false;
      }
      return true;
    }

    void Bus_SPI::readPixels(void *dst, pixelcopy_t *param, uint32_t length)
    {
      uint32_t bytes = param->src_bits >> 3;
      uint32_t dstindex = 0;
      uint32_t len = 4;
      uint8_t buf[24];
      param->src_data = buf;
      do
      {
        if (len > length)
          len = length;
        readBytes((uint8_t *)buf, len * bytes, true);
        param->src_x = 0;
        dstindex = param->fp_copy(dst, dstindex, dstindex + len, param);
        length -= len;
      } while (length);
    }

    //----------------------------------------------------------------------------
  }
}