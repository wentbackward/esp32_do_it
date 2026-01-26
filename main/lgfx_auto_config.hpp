// Auto-configured LovyanGFX configuration from Kconfig
//
// NOTE: For RGB panels, use custom config (lgfx_elecrow_7inch.hpp) instead
// The auto-config currently only supports SPI panels

#pragma once

#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include "sdkconfig.h"

#ifndef CONFIG_APP_LGFX_PANEL_SPI
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#endif

#if CONFIG_APP_LGFX_PANEL_SPI

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();

            cfg.spi_host = (spi_host_device_t)CONFIG_APP_LCD_SPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = CONFIG_APP_LCD_SPI_CLOCK_HZ;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = CONFIG_APP_LCD_PIN_SCK;
            cfg.pin_mosi = CONFIG_APP_LCD_PIN_MOSI;
            cfg.pin_miso = CONFIG_APP_LCD_PIN_MISO;
            cfg.pin_dc   = CONFIG_APP_LCD_PIN_DC;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();

            cfg.memory_width  = CONFIG_APP_LCD_HRES;
            cfg.memory_height = CONFIG_APP_LCD_VRES;
            cfg.panel_width   = CONFIG_APP_LCD_HRES;
            cfg.panel_height  = CONFIG_APP_LCD_VRES;

            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = CONFIG_APP_LCD_INVERT_DEFAULT;
            cfg.rgb_order = !CONFIG_APP_LCD_BGR;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }
};

#elif CONFIG_APP_LGFX_PANEL_RGB

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_RGB     _panel_instance;
    lgfx::Bus_RGB       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            // RGB data pins (D0-D15) R, G, B channels
            cfg.pin_d0  = CONFIG_APP_LCD_RGB_PIN_D0;    // B0
            cfg.pin_d1  = CONFIG_APP_LCD_RGB_PIN_D1;    // B1
            cfg.pin_d2  = CONFIG_APP_LCD_RGB_PIN_D2;    // B2
            cfg.pin_d3  = CONFIG_APP_LCD_RGB_PIN_D3;    // B3
            cfg.pin_d4  = CONFIG_APP_LCD_RGB_PIN_D4;    // B4
            cfg.pin_d5  = CONFIG_APP_LCD_RGB_PIN_D5;    // G0
            cfg.pin_d6  = CONFIG_APP_LCD_RGB_PIN_D6;    // G1
            cfg.pin_d7  = CONFIG_APP_LCD_RGB_PIN_D7;    // G2
            cfg.pin_d8  = CONFIG_APP_LCD_RGB_PIN_D8;    // G3
            cfg.pin_d9  = CONFIG_APP_LCD_RGB_PIN_D9;    // G4
            cfg.pin_d10 = CONFIG_APP_LCD_RGB_PIN_D10;   // G5
            cfg.pin_d11 = CONFIG_APP_LCD_RGB_PIN_D11;   // R0
            cfg.pin_d12 = CONFIG_APP_LCD_RGB_PIN_D12;   // R1
            cfg.pin_d13 = CONFIG_APP_LCD_RGB_PIN_D13;   // R2
            cfg.pin_d14 = CONFIG_APP_LCD_RGB_PIN_D14;   // R3
            cfg.pin_d15 = CONFIG_APP_LCD_RGB_PIN_D15;   // R4

            // Control signals
            cfg.pin_henable = CONFIG_APP_LCD_RGB_PIN_DE;     // DE (Data Enable)
            cfg.pin_vsync   = CONFIG_APP_LCD_RGB_PIN_VSYNC;  // VSYNC
            cfg.pin_hsync   = CONFIG_APP_LCD_RGB_PIN_HSYNC;  // HSYNC
            cfg.pin_pclk    = CONFIG_APP_LCD_RGB_PIN_PCLK;   // PCLK

            cfg.freq_write = CONFIG_APP_LCD_RGB_PCLK_HZ;  // 15 MHz pixel clock

            // Timing parameters for 800x480 display
            cfg.hsync_polarity    = CONFIG_APP_LCD_RGB_HSYNC_POLARITY;
            cfg.hsync_front_porch = CONFIG_APP_LCD_RGB_HSYNC_FRONT_PORCH;
            cfg.hsync_pulse_width = CONFIG_APP_LCD_RGB_HSYNC_PULSE_WIDTH;
            cfg.hsync_back_porch  = CONFIG_APP_LCD_RGB_HSYNC_BACK_PORCH;

            cfg.vsync_polarity    = CONFIG_APP_LCD_RGB_VSYNC_POLARITY;
            cfg.vsync_front_porch = CONFIG_APP_LCD_RGB_VSYNC_FRONT_PORCH;
            cfg.vsync_pulse_width = CONFIG_APP_LCD_RGB_VSYNC_PULSE_WIDTH;
            cfg.vsync_back_porch  = CONFIG_APP_LCD_RGB_VSYNC_BACK_PORCH;

            cfg.pclk_active_neg = CONFIG_APP_LCD_RGB_PCLK_ACTIVE_NEG;
            cfg.de_idle_high    = CONFIG_APP_LCD_RGB_DE_IDLE_HIGH;
            cfg.pclk_idle_high  = CONFIG_APP_LCD_RGB_PCLK_IDLE_HIGH;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();

            #ifdef CONFIG_APP_LCD_BGR
            cfg.rgb_order = true; // True for BGR, False for RGB (default)
            #else
            cfg.rgb_order = false; // True for BGR, False for RGB (default)
            #endif


            #if CONFIG_APP_LCD_INVERT_DEFAULT
                cfg.invert = true;
            #else
                cfg.invert = false;
            #endif

            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;

            cfg.offset_x = 0;
            cfg.offset_y = 0;

            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;

            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();

            cfg.pin_bl = GPIO_NUM_2;      // Backlight PWM pin
            cfg.invert = false;
            cfg.freq   = 5000;            // 5 kHz PWM
            cfg.pwm_channel = 0;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif
