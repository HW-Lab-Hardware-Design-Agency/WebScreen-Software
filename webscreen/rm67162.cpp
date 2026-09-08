#include "rm67162.h"
#include "SPI.h"
#include "Arduino.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

const static lcd_cmd_t rm67162_spi_init[] = {
  { 0xFE, { 0x00 }, 0x01 },  // PAGE
  // {0x35, {0x00},        0x00}, //TE ON
  // {0x34, {0x00},        0x00}, //TE OFF
  { 0x36, { 0x00 }, 0x01 },  // Scan Direction Control
  { 0x3A, { 0x75 }, 0x01 },  // Interface Pixel Format 16bit/pixel
  // {0x3A, {0x76},        0x01}, //Interface Pixel Format    18bit/pixel
  // {0x3A, {0x77},        0x01}, //Interface Pixel Format    24bit/pixel
  { 0x51, { 0x00 }, 0x01 },         // Write Display Brightness MAX_VAL=0XFF
  { 0x11, { 0x00 }, 0x01 | 0x80 },  // Sleep Out
  { 0x29, { 0x00 }, 0x01 | 0x80 },  // Display on
  { 0x51, { 0xD0 }, 0x01 },         // Write Display Brightness    MAX_VAL=0XFF
};

const static lcd_cmd_t rm67162_qspi_init[] = {
  { 0x11, { 0x00 }, 0x80 },  // Sleep Out
  // {0x44, {0x01, 0x66},        0x02}, //Set_Tear_Scanline
  // {0x35, {0x00},        0x00}, //TE ON
  // {0x34, {0x00},        0x00}, //TE OFF
  // {0x36, {0x00},        0x01}, //Scan Direction Control
  { 0x3A, { 0x55 }, 0x01 },  // Interface Pixel Format 16bit/pixel
  // {0x3A, {0x66},        0x01}, //Interface Pixel Format    18bit/pixel
  // {0x3A, {0x77},        0x01}, //Interface Pixel Format    24bit/pixel
  { 0x51, { 0x00 }, 0x01 },  // Write Display Brightness MAX_VAL=0XFF
  { 0x29, { 0x00 }, 0x80 },  // Display on
  { 0x51, { 0xD0 }, 0x01 },  // Write Display Brightness   MAX_VAL=0XFF
};

static spi_device_handle_t spi;

// Serializes panel access: lcd_PushColors holds CS low across chunks, so concurrent
// lcd_brightness/lcd_send_cmd corrupts the stream. Recursive: lcd_PushColors reaches lcd_send_cmd while holding it.
static SemaphoreHandle_t s_panel_mutex = NULL;

static void panel_lock(void) {
  if (s_panel_mutex == NULL) {
    // Lazy fallback; normally created in rm67162_init() before any second task exists.
    s_panel_mutex = xSemaphoreCreateRecursiveMutex();
  }
  if (s_panel_mutex != NULL) {
    xSemaphoreTakeRecursive(s_panel_mutex, portMAX_DELAY);
  }
}

static void panel_unlock(void) {
  if (s_panel_mutex != NULL) {
    xSemaphoreGiveRecursive(s_panel_mutex);
  }
}

static void WriteComm(uint8_t data) {
  TFT_CS_L;
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
  TFT_DC_L;
  SPI.write(data);
  TFT_DC_H;
  SPI.endTransaction();
  TFT_CS_H;
}

static void WriteData(uint8_t data) {
  TFT_CS_L;
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
  TFT_DC_H;
  SPI.write(data);
  SPI.endTransaction();
  TFT_CS_H;
}

static void WriteData16(uint16_t data) {
  TFT_CS_L;
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
  TFT_DC_H;
  SPI.write16(data);
  SPI.endTransaction();
  TFT_CS_H;
}

static void lcd_send_cmd(uint32_t cmd, uint8_t *dat, uint32_t len) {
  panel_lock();
#if LCD_USB_QSPI_DREVER == 1
  TFT_CS_L;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.flags = (SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR);
  t.cmd = 0x02;
  t.addr = cmd << 8;
  // LOGF("t.addr:0x%X\r\n", t.addr);
  if (len != 0) {
    t.tx_buffer = dat;
    t.length = 8 * len;
  } else {
    t.tx_buffer = NULL;
    t.length = 0;
  }
  spi_device_polling_transmit(spi, &t);
  TFT_CS_H;
#else
  WriteComm(cmd);
  if (len != 0) {
    for (int i = 0; i < len; i++)
      WriteData(dat[i]);
  }
#endif
  panel_unlock();
}
void rm67162_init(void) {
  // Idempotent: a second spi_bus_initialize() would abort via ESP_ERROR_CHECK.
  static bool s_initialized = false;
  if (s_initialized) {
    return;
  }
  s_initialized = true;

  if (s_panel_mutex == NULL) {
    s_panel_mutex = xSemaphoreCreateRecursiveMutex();
  }
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_RES, OUTPUT);

  TFT_RES_L;
  delay(300);
  TFT_RES_H;
  delay(200);

#if LCD_USB_QSPI_DREVER == 1
  esp_err_t ret;

  spi_bus_config_t buscfg = {
    .data0_io_num = TFT_QSPI_D0,
    .data1_io_num = TFT_QSPI_D1,
    .sclk_io_num = TFT_QSPI_SCK,
    .data2_io_num = TFT_QSPI_D2,
    .data3_io_num = TFT_QSPI_D3,
    .max_transfer_sz = SEND_BUF_SIZE * sizeof(uint16_t),
    .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS /* |
                 SPICOMMON_BUSFLAG_QUAD */
    ,
  };
  spi_device_interface_config_t devcfg = {
    .command_bits = 8,
    .address_bits = 24,
    .mode = TFT_SPI_MODE,
    .clock_speed_hz = SPI_FREQUENCY,
    .spics_io_num = -1,
    // .spics_io_num = TFT_QSPI_CS,
    .flags = SPI_DEVICE_HALFDUPLEX,
    .queue_size = 1,  // All transfers use spi_device_polling_transmit().
  };
  ret = spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);
  ret = spi_bus_add_device(TFT_SPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);

#else
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(SPI_FREQUENCY);
  pinMode(TFT_DC, OUTPUT);
#endif
  // Initialize the screen multiple times to prevent initialization failure
  int i = 3;
  while (i--) {
#if LCD_USB_QSPI_DREVER == 1
    const lcd_cmd_t *lcd_init = rm67162_qspi_init;
    for (int i = 0; i < sizeof(rm67162_qspi_init) / sizeof(lcd_cmd_t); i++)
#else
      const lcd_cmd_t *lcd_init = rm67162_spi_init;
    for (int i = 0; i < sizeof(rm67162_spi_init) / sizeof(lcd_cmd_t); i++)
#endif
    {
      lcd_send_cmd(lcd_init[i].cmd,
                   (uint8_t *)lcd_init[i].data,
                   lcd_init[i].len & 0x7f);

      if (lcd_init[i].len & 0x80)
        delay(120);
    }
  }
}
void lcd_setRotation(uint8_t r) {
  uint8_t gbr = TFT_MAD_RGB;

  switch (r) {
    case 0:  // Portrait
      // WriteData(gbr);
      break;
    case 1:  // Landscape (Portrait + 90)
      gbr = TFT_MAD_MX | TFT_MAD_MV | gbr;
      break;
    case 2:  // Inverter portrait
      gbr = TFT_MAD_MX | TFT_MAD_MY | gbr;
      break;
    case 3:  // Inverted landscape
      gbr = TFT_MAD_MV | TFT_MAD_MY | gbr;
      break;
  }
  lcd_send_cmd(TFT_MADCTL, &gbr, 1);
}
void lcd_address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  lcd_cmd_t t[3] = {
    { 0x2a, { uint8_t(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2 }, 0x04 },
    { 0x2b, { uint8_t(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2 }, 0x04 },
    { 0x2c, { 0x00 }, 0x00 },
  };

  for (uint32_t i = 0; i < 3; i++) {
    lcd_send_cmd(t[i].cmd, t[i].data, t[i].len);
  }
}
void lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color) {
  lcd_address_set(x, y, x + 1, y + 1);
  lcd_PushColors(&color, 1);
}
void lcd_PushColors(uint16_t x,
                    uint16_t y,
                    uint16_t width,
                    uint16_t high,
                    uint16_t *data) {
  // Held across address-set + every chunk: CS stays low for the whole burst.
  panel_lock();
#if LCD_USB_QSPI_DREVER == 1
  bool first_send = 1;
  size_t len = width * high;
  uint16_t *p = (uint16_t *)data;

  lcd_address_set(x, y, x + width - 1, y + high - 1);
  TFT_CS_L;
  do {
    size_t chunk_size = len;
    spi_transaction_ext_t t = { 0 };
    memset(&t, 0, sizeof(t));
    if (first_send) {
      t.base.flags =
        SPI_TRANS_MODE_QIO /* | SPI_TRANS_MODE_DIOQIO_ADDR */;
      t.base.cmd = 0x32 /* 0x12 */;
      t.base.addr = 0x002C00;
      first_send = 0;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
      t.command_bits = 0;
      t.address_bits = 0;
      t.dummy_bits = 0;
    }
    if (chunk_size > SEND_BUF_SIZE) {
      chunk_size = SEND_BUF_SIZE;
    }
    t.base.tx_buffer = p;
    t.base.length = chunk_size * 16;

    // spi_device_queue_trans(spi, (spi_transaction_t *)&t, portMAX_DELAY);
    spi_device_polling_transmit(spi, (spi_transaction_t *)&t);
    len -= chunk_size;
    p += chunk_size;
  } while (len > 0);
  TFT_CS_H;

#else
  lcd_address_set(x, y, x + width - 1, y + high - 1);
  TFT_CS_L;
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
  TFT_DC_H;
  SPI.writeBytes((uint8_t *)data, width * high * 2);
  SPI.endTransaction();
  TFT_CS_H;
#endif
  panel_unlock();
}
void lcd_PushColors(uint16_t *data, uint32_t len) {
  panel_lock();
#if LCD_USB_QSPI_DREVER == 1
  bool first_send = 1;
  uint16_t *p = (uint16_t *)data;
  TFT_CS_L;
  do {
    size_t chunk_size = len;
    spi_transaction_ext_t t = { 0 };
    memset(&t, 0, sizeof(t));
    if (first_send) {
      t.base.flags =
        SPI_TRANS_MODE_QIO /* | SPI_TRANS_MODE_DIOQIO_ADDR */;
      t.base.cmd = 0x32 /* 0x12 */;
      t.base.addr = 0x002C00;
      first_send = 0;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
      t.command_bits = 0;
      t.address_bits = 0;
      t.dummy_bits = 0;
    }
    if (chunk_size > SEND_BUF_SIZE) {
      chunk_size = SEND_BUF_SIZE;
    }
    t.base.tx_buffer = p;
    t.base.length = chunk_size * 16;

    // spi_device_queue_trans(spi, (spi_transaction_t *)&t, portMAX_DELAY);
    spi_device_polling_transmit(spi, (spi_transaction_t *)&t);
    len -= chunk_size;
    p += chunk_size;
  } while (len > 0);
  TFT_CS_H;

#else
  TFT_CS_L;
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
  TFT_DC_H;
  SPI.writeBytes((uint8_t *)data, len * 2);
  SPI.endTransaction();
  TFT_CS_H;
#endif
  panel_unlock();
}
void lcd_brightness(uint8_t brightness) {
  panel_lock();
  lcd_send_cmd(0x51, &brightness, 1);
  panel_unlock();
}

void lcd_sleep() {
  lcd_send_cmd(0x10, NULL, 0);
}
