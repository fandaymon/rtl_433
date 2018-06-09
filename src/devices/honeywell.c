/*
 * *** Honeywell (Ademco) Door/Window Sensors (345.0Mhz) ***
 *
 * Tested with the Honeywell 5811 Wireless Door/Window transmitters
 *
 * also: 2Gig DW10 door sensors
 * and Resolution Products RE208 (wire to air repeater)
 *
 * 64 bit packets, repeated multiple times per open/close event
 *
 * Protocol whitepaper: "DEFCON 22: Home Insecurity" by Logan Lamb
 *
 * PP PP C IIIII EE SS SS
 * P: 16bit Preamble and sync bit (always ff fe)
 * C: 4bit Channel
 * I: 20bit Device serial number / or counter value
 * E: 8bit Event, where 0x80 = Open/Close, 0x04 = Heartbeat / or id
 * S: 16bit CRC
 *
 */

#include "rtl_433.h"
#include "pulse_demod.h"
#include "util.h"

// full preamble is 0xFFFE
static const unsigned char preamble_pattern[2] = {0xff, 0xe0}; // 12 bits

static int honeywell_callback(bitbuffer_t *bitbuffer)
{
    data_t *data;
    char time_str[LOCAL_TIME_BUFLEN];
    int row;
    int pos;
    uint8_t b[6] = {0};
    int channel;
    int device_id;
    int event;
    int state;
    int heartbeat;
    uint16_t crc_calculated;
    uint16_t crc;

    row = 0; // we expect a single row only. reduce collisions
    if (bitbuffer->num_rows != 1 || bitbuffer->bits_per_row[row] < 60)
        return 0; // Short buffer

    bitbuffer_invert(bitbuffer);

    pos = bitbuffer_search(bitbuffer, row, 0, preamble_pattern, 12);
    bitbuffer_extract_bytes(bitbuffer, row, pos + 12, b, 48);

    channel = b[0] >> 4;
    device_id = ((b[0] & 0xf) << 16) | (b[1] << 8) | b[2];
    crc = (b[4] << 8) | b[5];

    if (device_id == 0 && crc == 0)
        return 0; // Reduce collisions

    if (channel == 0x2 || channel == 0xA) {
        // 2GIG brand
        crc_calculated = crc16_ccitt(b, 4, 0x8050, 0);
    } else { // channel == 0x8
        crc_calculated = crc16_ccitt(b, 4, 0x8005, 0);
    }
    if (crc != crc_calculated)
        return 0; // Not a valid packet

    event = b[3];
    state = (event & 0x80) >> 7;
    heartbeat = (event & 0x04) >> 2;

    local_time_str(0, time_str);
    data = data_make(
            "time",     "", DATA_STRING, time_str,
            "model",    "", DATA_STRING, "Honeywell Door/Window Sensor",
            "id",       "", DATA_FORMAT, "%05x", DATA_INT, device_id,
            "channel",  "", DATA_INT, channel,
            "event",    "", DATA_FORMAT, "%02x", DATA_INT, event,
            "state",    "", DATA_STRING, state ? "open" : "closed",
            "heartbeat", "", DATA_STRING, heartbeat ? "yes" : "no",
            NULL);

    data_acquired_handler(data);
    return 1;
}

static char *output_fields[] = {
    "time",
    "model",
    "id",
    "channel",
    "event",
    "state",
    "heartbeat",
    NULL
};

r_device honeywell = {
    .name           = "Honeywell Door/Window Sensor",
    .modulation     = OOK_PULSE_MANCHESTER_ZEROBIT,
    .short_limit    = 156,
    .long_limit     = 0,
    .reset_limit    = 292,
    .json_callback  = &honeywell_callback,
    .disabled       = 0,
    .demod_arg      = 0,
    .fields         = output_fields,
};
