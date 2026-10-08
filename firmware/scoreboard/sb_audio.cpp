#include "sb_audio.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>
#include "sb_log.h"
#include "sb_settings.h"
#include "sb_sound_data.h"

// Pins (Seengreat RGB Matrix HUB75 S3)
static const int MCLK = 38, SCLK = 48, LRCK = 21, DOUT = 14, AMP_EN = 3;
static const uint8_t CODEC = 0x18;
static const int RATE = 16000;   // MCLK = 256 x RATE = 4.096 MHz
static const i2s_port_t PORT = I2S_NUM_0;

static bool ready = false;
static QueueHandle_t queue = nullptr;
static const uint8_t CHIME = 0xFF;

static bool wr(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(CODEC);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

// ES8311 as an I2S slave, DAC only, 16-bit, MCLK from the ESP32 at 256 fs
// (register values follow Espressif's es8311 driver).
static bool codecInit() {
  bool ok = wr(0x00, 0x1F);   // reset
  delay(20);
  ok &= wr(0x00, 0x00);
  ok &= wr(0x01, 0x30);       // clocks from the MCLK pin
  ok &= wr(0x02, 0x00);       // pre-divide 1, multiply 1
  ok &= wr(0x03, 0x10);       // ADC: single speed, OSR 16
  ok &= wr(0x16, 0x24);
  ok &= wr(0x04, 0x10);       // DAC OSR 16
  ok &= wr(0x05, 0x00);       // ADC / DAC clock divide 1
  ok &= wr(0x06, 0x03);       // BCLK divide 4 (unused as slave)
  ok &= wr(0x07, 0x00);       // LRCK divide 256
  ok &= wr(0x08, 0xFF);
  ok &= wr(0x0B, 0x00);
  ok &= wr(0x0C, 0x00);
  ok &= wr(0x10, 0x1F);
  ok &= wr(0x11, 0x7F);
  ok &= wr(0x00, 0x80);       // power on, slave mode
  ok &= wr(0x01, 0x3F);       // all clocks on
  ok &= wr(0x13, 0x10);       // output drive on
  ok &= wr(0x1B, 0x0A);
  ok &= wr(0x1C, 0x6A);
  ok &= wr(0x09, 0x0C);       // data in: I2S, 16-bit
  ok &= wr(0x0A, 0x0C);       // data out: I2S, 16-bit
  ok &= wr(0x0E, 0x02);       // analog on
  ok &= wr(0x12, 0x00);       // DAC on
  ok &= wr(0x14, 0x1A);
  ok &= wr(0x0D, 0x01);       // power up
  ok &= wr(0x15, 0x40);
  ok &= wr(0x37, 0x08);       // DAC equaliser bypassed
  ok &= wr(0x45, 0x00);
  ok &= wr(0x32, 0xBF);       // DAC volume 0 dB (the loudest that doesn't distort)
  ok &= wr(0x31, 0x00);       // unmute
  return ok;
}

static bool i2sInit() {
  i2s_config_t c = {};
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  c.sample_rate = RATE;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = 0;
  c.dma_buf_count = 4;
  c.dma_buf_len = 256;
  c.use_apll = false;
  c.tx_desc_auto_clear = true;   // silence when nothing is playing
  c.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(PORT, &c, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t p = {};
  p.mck_io_num = MCLK;
  p.bck_io_num = SCLK;
  p.ws_io_num = LRCK;
  p.data_out_num = DOUT;
  p.data_in_num = I2S_PIN_NO_CHANGE;
  return i2s_set_pin(PORT, &p) == ESP_OK;
}

// A rising four-note chime (C E G C), each note a bell-like tone that fades.
static void playChime() {
  static const float NOTE[] = {1046.5f, 1318.5f, 1568.0f, 2093.0f};
  static const int LEN_MS[] = {160, 160, 160, 600};
  static int16_t buf[256 * 2];
  digitalWrite(AMP_EN, HIGH);
  delay(30);
  for (int n = 0; n < 4; n++) {
    int total = RATE * LEN_MS[n] / 1000;
    float ph = 0, dph = 2 * (float)M_PI * NOTE[n] / RATE;
    for (int i = 0; i < total;) {
      int k = 0;
      for (; k < 256 && i < total; k++, i++) {
        float t = (float)i / RATE;
        float env = (i < RATE / 200 ? i / (RATE / 200.0f) : 1.0f) * expf(-t * (n == 3 ? 4.0f : 9.0f));
        float s = (sinf(ph) * 0.8f + sinf(2 * ph) * 0.2f) * env;
        ph += dph;
        if (ph > 2 * (float)M_PI) ph -= 2 * (float)M_PI;
        int16_t v = (int16_t)(s * 32000);
        buf[2 * k] = v;
        buf[2 * k + 1] = v;
      }
      size_t done;
      i2s_write(PORT, buf, k * 4, &done, portMAX_DELAY);
    }
  }
  delay(120);   // let the last of it play out before the amplifier goes off
  i2s_zero_dma_buffer(PORT);
  digitalWrite(AMP_EN, LOW);
}

// mu-law byte -> 16-bit sample (G.711)
static int16_t ulaw(uint8_t b) {
  b = ~b;
  int e = (b >> 4) & 7, m = b & 15;
  int x = (((m << 3) + 132) << e) - 132;
  return (b & 0x80) ? -x : x;
}

static void playClip(SoundId id) {
  const SoundClip& c = SOUNDS[id];
  static int16_t buf[256 * 2];
  digitalWrite(AMP_EN, HIGH);
  delay(30);
  for (uint32_t i = 0; i < c.samples;) {
    int k = 0;
    for (; k < 256 && i < c.samples; k++, i++) {
      int16_t v = ulaw(pgm_read_byte(c.data + i));
      buf[2 * k] = v;
      buf[2 * k + 1] = v;
    }
    size_t done;
    i2s_write(PORT, buf, k * 4, &done, portMAX_DELAY);
  }
  delay(120);
  i2s_zero_dma_buffer(PORT);
  digitalWrite(AMP_EN, LOW);
}

static void audioTask(void*) {
  for (;;) {
    uint8_t id;
    if (xQueueReceive(queue, &id, portMAX_DELAY) != pdTRUE) continue;
    if (id == CHIME) playChime();
    else if (id > SND_NONE && id < SND_COUNT) playClip((SoundId)id);
  }
}

bool audioBegin() {
  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, LOW);
  if (!i2sInit()) { sbLog("audio: I2S didn't start"); return false; }
  delay(10);   // MCLK running before the codec is set up
  if (!codecInit()) { sbLog("audio: sound chip (0x18) didn't answer"); return false; }
  queue = xQueueCreate(2, 1);
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 2, nullptr, 0);
  ready = true;
  sbLog("audio: ready");
  return true;
}

bool audioReady() { return ready; }

static void send(uint8_t id) {
  if (queue) xQueueSend(queue, &id, 0);   // full (two waiting): this one is dropped
}
void audioChime() { send(CHIME); }
void audioPlayAlways(SoundId id) { if (id != SND_NONE) send(id); }
void audioPlay(SoundId id) {
  if (settings.sound && id != SND_NONE) send(id);
}
