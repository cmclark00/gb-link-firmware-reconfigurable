/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include <stdio.h>
#include <string.h>
#include "hardware/pio.h"
#include "hardware/clocks.h" // Required for clock_get_hz
#include "pio/pio_spi.h"
#include "spi.pio.h"      // Generated PIO header for SPI
#include "pico/time.h"
#include "pico/stdlib.h"  // Required for stdio_init_all

#include "pokemon_data.h" // For Gen1PokemonPartyData and related functions
#include <stdio.h>        // For printf

#define NUM_CMP_BYTES 0x20
#define NUM_CMP_BYTES_RECV (NUM_CMP_BYTES+4)

#define NUM_DEFAULT_BYTES_PER_TRANSFER 1
#define US_DEFAULT_PER_TRANSFER 1000

#define MAX_TRANSFER_BYTES 0x40

// CRITICAL: Game Boy operates at 5V logic. Level shifters (e.g., bidirectional 3.3V-5V)
// MUST be used on GB_PIN_CLK, GB_PIN_DATA_OUT, and GB_PIN_DATA_IN when connecting
// to a Game Boy to prevent damage to the RP2040 and ensure reliable communication.
#define GB_PIN_CLK 0       // Game Boy Link Cable Clock
#define GB_PIN_DATA_OUT 1  // Data Out from RP2040 to Game Boy
#define GB_PIN_DATA_IN 2   // Data In from Game Boy to RP2040

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,

  BLINK_ALWAYS_ON   = UINT32_MAX,
  BLINK_ALWAYS_OFF  = 0
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static uint8_t data_buf[MAX_TRANSFER_BYTES];
static uint8_t compare_bytes[NUM_CMP_BYTES] = {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
static uint8_t buf_count;
static uint8_t num_bytes_per_transfer = NUM_DEFAULT_BYTES_PER_TRANSFER;
static uint32_t us_between_transfer = US_DEFAULT_PER_TRANSFER;
static uint32_t total_transferred = 0;

#define URL  "tetris.gblink.io"

const tusb_desc_webusb_url_t desc_url =
{
  .bLength         = 3 + sizeof(URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 1, // 0: http, 1: https
  .url             = URL
};

static bool web_serial_connected = false;

// Global PIO SPI instance for Game Boy communication
pio_spi_inst_t g_gb_spi_inst;

// Global Pokemon data for trading
Gen1PokemonPartyData g_outgoing_pokemon;
Gen1PokemonPartyData g_received_pokemon;
#define GEN1_TRADE_POKEMON_BLOCK_SIZE 44

//------------- prototypes -------------//
void prepare_outgoing_magikarp(void);
void execute_trade_sequence(void);
void handle_input_data(uint8_t* buf_in, uint32_t count);
// void data_transfer_task(void); // Already commented out
void led_blinking_task(void);
void cdc_task(void);
void webserial_task(void);

/*------------- MAIN -------------*/

  // pio_spi_inst_t spi = {
  //         .pio = pio1,
  //         .sm = 0
  // };


int main(void)
{
  stdio_init_all(); // Initialize stdio for printf over USB CDC
  printf("Board Initializing...\n");

  //board_init(); // board_init() is usually called by TinyUSB or not needed if using board_led_write directly
  buf_count = 0;
  // uint cpha1_prog_offs = pio_add_program(spi.pio, &spi_cpha1_program);
  // pio_spi_init(spi.pio, spi.sm, cpha1_prog_offs, 8, 4058.838/128, 1, 1, PIN_SCK, PIN_SOUT, PIN_SIN);

  gpio_init(GB_PIN_CLK);
  gpio_init(GB_PIN_DATA_OUT);
  gpio_init(GB_PIN_DATA_IN);

  tusb_init();

  // Initialize Game Boy serial communication
  gb_serial_init(0, 0, GB_PIN_CLK, GB_PIN_DATA_OUT, GB_PIN_DATA_IN, 8192);

  while (1)
  {
    tud_task(); // tinyusb device task
    // data_transfer_task();
    cdc_task();
    webserial_task();
    led_blinking_task();
  }

  return 0;
}

int oldmain(void)
{
  board_init();

  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    cdc_task();
    webserial_task();
    led_blinking_task();
  }

  return 0;
}

// send characters to both CDC and WebUSB
void echo_all(uint8_t buf[], uint32_t count)
{
  // echo to web serial
  if ( web_serial_connected )
  {
    tud_vendor_write(buf, count);
    tud_vendor_flush();
  }

  // echo to cdc
  if ( tud_cdc_connected() )
  {
    for(uint32_t i=0; i<count; i++)
    {
      tud_cdc_write_char(buf[i]);
    }
    tud_cdc_write_flush();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to do for DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bRequest)
  {
    case VENDOR_REQUEST_WEBUSB:
      // match vendor request in BOS descriptor
      // Get landing page url
      return tud_control_xfer(rhport, request, (void*) &desc_url, desc_url.bLength);

    case VENDOR_REQUEST_MICROSOFT:
      if ( request->wIndex == 7 )
      {
        // Get Microsoft OS 2.0 compatible descriptor
        uint16_t total_len;
        memcpy(&total_len, desc_ms_os_20+8, 2);

        return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
      }else
      {
        return false;
      }
    case 0x22:
      // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
      web_serial_connected = (request->wValue != 0);
      
      total_transferred = 0;
      num_bytes_per_transfer = NUM_DEFAULT_BYTES_PER_TRANSFER;
      us_between_transfer = US_DEFAULT_PER_TRANSFER;

      // Always lit LED if connected
      if ( web_serial_connected )
      {
        board_led_write(true);
        blink_interval_ms = BLINK_ALWAYS_ON;

        // tud_vendor_write_str("\r\nTinyUSB WebUSB device example\r\n");
      }else
      {
        blink_interval_ms = BLINK_MOUNTED;
      }

      // response with status OK
      return tud_control_status(rhport, request);
      break;

    default: break;
  }

  // stall unknown request
  return false;
}

// Invoked when DATA Stage of VENDOR's request is complete
bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;
  (void) request;

  // nothing to do
  return true;
}

// void data_transfer_task(void) {
//     //if(buf_count) {
//         //uint8_t buf_out[MAX_TRANSFER_BYTES];
//     //    for(int i = 0; i < (buf_count+3) >> 2; i++) {
//     //        pio_spi_write8_blocking(&spi, data_buf+(4*i), 4);
//     //        busy_wait_us(36);
//     //    }
//         //pio_spi_write8_read8_blocking(&spi, data_buf, buf_out, buf_count);
//         //echo_all(buf_out, buf_count);
//     //    buf_count = 0;
//     //}
// }

// Game Boy Serial Communication Functions
bool gb_serial_init(PIO pio_instance_num, uint sm_num, uint clk_pin, uint data_out_pin, uint data_in_pin, uint32_t gb_baud_rate) {
    PIO pio = (pio_instance_num == 0) ? pio0 : pio1;
    uint offset = pio_add_program(pio, &spi_cpha1_program);
    float clkdiv = (float)clock_get_hz(clk_sys) / (gb_baud_rate * 2.0f);

    g_gb_spi_inst.pio = pio;
    g_gb_spi_inst.sm = sm_num;
    g_gb_spi_inst.cs_pin = -1; // Not used by pio_spi_init directly

    pio_spi_init(g_gb_spi_inst.pio, g_gb_spi_inst.sm, offset, 8, clkdiv, true /*cpha=1*/, false /*cpol=0*/, clk_pin, data_out_pin, data_in_pin);
    return true;
}

uint8_t gb_serial_exchange_byte(uint8_t byte_to_send) {
    uint8_t received_byte;
    pio_spi_write8_read8_blocking(&g_gb_spi_inst, &byte_to_send, &received_byte, 1);
    return received_byte;
}

void handle_input_data(uint8_t* buf_in, uint32_t count) {
    // To be implemented later for Pokemon trading commands
    // For now, can echo back or do nothing
    // echo_all(buf_in, count); // Optional: echo back for testing USB

    if (count > 0 && buf_in[0] == 't') {
        printf("Trade command 't' received.\n");
        prepare_outgoing_magikarp();
        execute_trade_sequence();
    } else if (count > 0 && buf_in[0] == 'h') {
        printf("Commands:\n t - Initiate Trade\n h - Help\n");
    }
    // echo_all(buf_in, count); // Commented out to prevent interference with printf debugging
}

void prepare_outgoing_magikarp() {
    printf("Preparing Magikarp Lvl 1 for trade...\n");
    // OT Name for Magikarp is "RP2040" (6 chars + 0x50 terminator = 7 bytes)
    // Nickname for Magikarp is "MAGIKARP" (8 chars + 0x50 terminator = 9 bytes, padded to 11)
    // create_tradeable_magikarp_lvl1 handles padding.
    create_tradeable_magikarp_lvl1(&g_outgoing_pokemon, "RP2040", 0x1234); // Example OT Name and ID
    printf("Magikarp ready. Species: %d (0x%02X), Level: %d, OT: %s, Nick: %s\n", 
           g_outgoing_pokemon.species_id, g_outgoing_pokemon.species_id, 
           g_outgoing_pokemon.level, g_outgoing_pokemon.ot_name, g_outgoing_pokemon.nickname);
    // Basic check of a few bytes that will be sent
    printf("Outgoing data sample: Species=0x%02X, Move1=0x%02X, Type1=0x%02X, Level=%d\n", 
           ((uint8_t*)&g_outgoing_pokemon)[0], ((uint8_t*)&g_outgoing_pokemon)[8], 
           ((uint8_t*)&g_outgoing_pokemon)[5], ((uint8_t*)&g_outgoing_pokemon)[0x21]);
}

void execute_trade_sequence() {
    // Zero out the received Pokemon struct before the trade
    memset(&g_received_pokemon, 0, sizeof(Gen1PokemonPartyData)); 
    printf("Starting trade sequence (g_received_pokemon zeroed)...\n");

    // Gen 1 Trade Protocol Overview (simplified):
    // 1. Handshake:
    //    - Master sends 0x01 (LINK_MODE_NORMAL), Slave responds 0x01.
    //    - Master sends 0x02 (LINK_ACTION_REQ_BLOCK) to become master, Slave responds 0x01.
    //    - Master sends 0xFD (LINK_VERSION_CHECK), Slave responds 0xFD.
    //    (These steps establish roles and verify compatibility)
    //
    // 2. Random Data Exchange (Entropy):
    //    - Master and Slave exchange blocks of random data (e.g., 256 bytes).
    //    - This helps ensure link quality and provides entropy for other operations.
    //
    // 3. Player/Pokemon Info Exchange (Trade Setup):
    //    - Exchange player names.
    //    - Exchange number of Pokémon in party.
    //    - Exchange species list of party Pokémon.
    //    - Exchange OT names of party Pokémon.
    //    - Exchange nicknames of party Pokémon.
    //    - Selection of Pokémon to trade by each player.
    //
    // 4. Core Pokémon Data Exchange:
    //    - The actual 44-byte data block for the selected Pokémon is exchanged.
    //    - This contains species, stats, moves, IVs, EVs, etc.
    //
    // 5. Confirmation/Checksum:
    //    - Potentially a checksum or final confirmation bytes.
    //
    // Simplified Implementation for this subtask: Direct 44-byte data block exchange.

    uint8_t send_buffer[GEN1_TRADE_POKEMON_BLOCK_SIZE];
    uint8_t receive_buffer[GEN1_TRADE_POKEMON_BLOCK_SIZE];

    // The g_outgoing_pokemon struct (62 bytes) has the 44-byte core data block at the beginning.
    memcpy(send_buffer, &g_outgoing_pokemon, GEN1_TRADE_POKEMON_BLOCK_SIZE);

    printf("Sending Magikarp data (%d bytes)...\n", GEN1_TRADE_POKEMON_BLOCK_SIZE);
    printf("Simulating byte-by-byte exchange with Game Boy (相手NPCなど)...\n");

    for (int i = 0; i < GEN1_TRADE_POKEMON_BLOCK_SIZE; ++i) {
        receive_buffer[i] = gb_serial_exchange_byte(send_buffer[i]);
        // Optional: printf("Sent: 0x%02X, Rcvd: 0x%02X\n", send_buffer[i], receive_buffer[i]);
        // busy_wait_us(100); // Small delay, adjust if necessary, might not be needed with PIO handling
    }

    printf("Data exchange complete.\n");

    // Copy received data into the 44-byte core section of g_received_pokemon.
    // Nickname and OT name fields in g_received_pokemon will remain uninitialized by this memcpy,
    // as they are located after the first 44 bytes in the Gen1PokemonPartyData struct.
    memcpy(&g_received_pokemon, receive_buffer, GEN1_TRADE_POKEMON_BLOCK_SIZE);

    printf("Received Pokemon Data (populated first 44 bytes into g_received_pokemon):\n");
    printf("  Species ID: %d (0x%02X)\n", g_received_pokemon.species_id, g_received_pokemon.species_id);
    printf("  Level: %d\n", g_received_pokemon.level);
    // Display OT Name and Nickname - these are outside the 44-byte block, so they'll show zeroed data.
    printf("  Received Pokemon OT Name (raw, from 62-byte struct): %.*s\n", GEN1_POKEMON_OT_NAME_LENGTH, g_received_pokemon.ot_name);
    printf("  Received Pokemon Nickname (raw, from 62-byte struct): %.*s\n", GEN1_POKEMON_NICKNAME_LENGTH, g_received_pokemon.nickname);
    // Display stats as requested
    printf("  Received Stats - HP:%d, Atk:%d, Def:%d, Spd:%d, Spc:%d\n", 
           g_received_pokemon.max_hp, g_received_pokemon.attack, g_received_pokemon.defense, 
           g_received_pokemon.speed, g_received_pokemon.special);
    printf("  Received Types - Type1: 0x%02X, Type2: 0x%02X\n", 
           g_received_pokemon.type1, g_received_pokemon.type2);
    printf("  Received Moves - M1:0x%02X, M2:0x%02X, M3:0x%02X, M4:0x%02X\n",
           g_received_pokemon.move1_id, g_received_pokemon.move2_id, g_received_pokemon.move3_id, g_received_pokemon.move4_id);
    
    // It's good practice to print a few raw received bytes too
    printf("Raw received data sample (first 8 bytes of receive_buffer): ");
    for(int i=0; i<8; ++i) { // Print first 8 bytes
        printf("0x%02X ", receive_buffer[i]);
    }
    printf("\n");
    
    printf("Trade sequence finished.\n");
}


void webserial_task(void)
{
  if ( web_serial_connected )
    if ( tud_vendor_available() ) {
      uint8_t buf_in[MAX_TRANSFER_BYTES*2];
      uint32_t count = tud_vendor_read(buf_in, sizeof(buf_in));
      handle_input_data(buf_in, count);
    }
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  if ( tud_cdc_connected() )
    // connected and there are data available
    if ( tud_cdc_available() ) {
      uint8_t buf_in[MAX_TRANSFER_BYTES*2];
      uint32_t count = tud_cdc_read((uint8_t*)buf_in, sizeof(buf_in));
      handle_input_data(buf_in, count);
    }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;

  // connected
  if ( dtr && rts )
  {
    // print initial message when connected
    // tud_cdc_write_str("\r\nTinyUSB WebUSB device example\r\n");
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
