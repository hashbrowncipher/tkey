// Copyright (C) 2022, 2023 - Tillitis AB
// SPDX-License-Identifier: GPL-2.0-only

#include <monocypher/monocypher-ed25519.h>
#include <tkey/qemu_debug.h>
#include <tkey/blake2s.h>
#include <tkey/tk1_mem.h>

#include "app_proto.h"

// clang-format off
static volatile uint32_t *cdi           = (volatile uint32_t *)TK1_MMIO_TK1_CDI_FIRST;
static volatile uint32_t *led           = (volatile uint32_t *)TK1_MMIO_TK1_LED;
static volatile uint32_t *touch         = (volatile uint32_t *)TK1_MMIO_TOUCH_STATUS;
static volatile uint32_t *cpu_mon_ctrl  = (volatile uint32_t *) TK1_MMIO_TK1_CPU_MON_CTRL;
static volatile uint32_t *cpu_mon_first = (volatile uint32_t *) TK1_MMIO_TK1_CPU_MON_FIRST;
static volatile uint32_t *cpu_mon_last  = (volatile uint32_t *) TK1_MMIO_TK1_CPU_MON_LAST;
static volatile uint32_t *app_addr      = (volatile uint32_t *) TK1_MMIO_TK1_APP_ADDR;
static volatile uint32_t *app_size      = (volatile uint32_t *) TK1_MMIO_TK1_APP_SIZE;

#define LED_BLACK 0
#define LED_RED   (1 << TK1_MMIO_TK1_LED_R_BIT)
#define LED_GREEN (1 << TK1_MMIO_TK1_LED_G_BIT)
#define LED_BLUE  (1 << TK1_MMIO_TK1_LED_B_BIT)
// clang-format on

#define MAX_SIGN_SIZE 4096

const uint8_t app_name0[4] = "tk1 ";
const uint8_t app_name1[4] = "sign";
const uint32_t app_version = 0x00000001;

void wait_touch_ledflash(int ledvalue, int loopcount)
{
	int led_on = 0;
	// first a write, to ensure no stray touch?
	*touch = 0;
	for (;;) {
		*led = led_on ? ledvalue : 0;
		for (int i = 0; i < loopcount; i++) {
			if (*touch & (1 << TK1_MMIO_TOUCH_STATUS_EVENT_BIT)) {
				goto touched;
			}
		}
		led_on = !led_on;
	}
touched:
	// write, confirming we read the touch event
	*touch = 0;
}

void set_seed(uint8_t * purpose, int purpose_len, const uint8_t * cdi, uint8_t * secret_key, uint8_t * pubkey) {
	blake2s_ctx blake2s_ctx;
	uint32_t derived_seed[8];

	blake2s(derived_seed, purpose_len, cdi, 32, purpose, 32, &blake2s_ctx);
	crypto_ed25519_key_pair(secret_key, pubkey, (uint8_t *)derived_seed);
}

int main(void)
{
#ifdef QEMU_DEBUG
	uint32_t stack;
#endif
	uint8_t pubkey[32];
	struct frame_header hdr; // Used in both directions
	uint8_t cmd[CMDLEN_MAXBYTES];
	uint8_t rsp[CMDLEN_MAXBYTES];
	uint32_t message_size = 0;
	uint8_t message[MAX_SIGN_SIZE];
	int msg_idx; // Where we are currently loading the data to sign
	uint8_t signature[64];
	uint32_t signature_done = 0;
	int left = 0;	// Bytes left to read
	int nbytes = 0; // Bytes to write to memory
	uint8_t in;
	uint32_t local_cdi[8];
	uint8_t secret_key[64];

	// Use Execution Monitor on RAM after app
	*cpu_mon_first = *app_addr + *app_size;
	*cpu_mon_last = TK1_RAM_BASE + TK1_RAM_SIZE;
	*cpu_mon_ctrl = 1;

#ifdef QEMU_DEBUG
	qemu_puts("Hello! &stack is on: ");
	qemu_putinthex((uint32_t)&stack);
	qemu_lf();
#endif

	// Generate a public key from CDI (only word aligned access to CDI)
	wordcpy(local_cdi, (void *)cdi, 8);
	set_seed(NULL, 0, (uint8_t *) local_cdi, secret_key, pubkey);

	for (;;) {
		*led = LED_BLUE;
		in = readbyte();
		qemu_puts("Read byte: ");
		qemu_puthex(in);
		qemu_lf();

		if (parseframe(in, &hdr) == -1) {
			qemu_puts("Couldn't parse header\n");
			continue;
		}

		memset(cmd, 0, CMDLEN_MAXBYTES);
		// Read app command, blocking
		read(cmd, hdr.len);

		if (hdr.endpoint == DST_FW) {
			appreply_nok(hdr);
			qemu_puts("Responded NOK to message meant for fw\n");
			continue;
		}

		// Is it for us?
		if (hdr.endpoint != DST_SW) {
			qemu_puts("Message not meant for app. endpoint was 0x");
			qemu_puthex(hdr.endpoint);
			qemu_lf();
			continue;
		}

		// Reset response buffer
		memset(rsp, 0, CMDLEN_MAXBYTES);

		// Min length is 1 byte so this should always be here
		switch (cmd[0]) {
		case APP_CMD_GET_PUBKEY:
			qemu_puts("APP_CMD_GET_PUBKEY\n");
			memcpy(rsp, pubkey, 32);
			appreply(hdr, APP_RSP_GET_PUBKEY, rsp);
			break;
		case APP_CMD_SET_SEED:
			qemu_puts("APP_CMD_SET_SEED\n");
			set_seed(cmd + 1, 32, (uint8_t *) local_cdi, secret_key, pubkey);
			rsp[0] = STATUS_OK;
			appreply(hdr, APP_RSP_SET_SEED, rsp);
			break;


		case APP_CMD_SET_SIZE:
			qemu_puts("APP_CMD_SET_SIZE\n");
			// Bad length
			if (hdr.len != 32) {
				rsp[0] = STATUS_BAD;
				appreply(hdr, APP_RSP_SET_SIZE, rsp);
				break;
			}
			signature_done = 0;
			// cmd[1..4] contains the size.
			message_size = cmd[1] + (cmd[2] << 8) + (cmd[3] << 16) +
				       (cmd[4] << 24);

			if (message_size > MAX_SIGN_SIZE) {
				qemu_puts("Message too big!\n");
				rsp[0] = STATUS_BAD;
				appreply(hdr, APP_RSP_SET_SIZE, rsp);
				break;
			}

			// Reset where we load the data
			left = message_size;
			msg_idx = 0;

			rsp[0] = STATUS_OK;
			appreply(hdr, APP_RSP_SET_SIZE, rsp);
			break;
		case APP_CMD_SIGN_DATA:
			qemu_puts("APP_CMD_SIGN_DATA\n");
			const uint32_t cmdBytelen = 128;

			// Bad length of this command, or APP_CMD_SET_SIZE has
			// not been called
			if (hdr.len != cmdBytelen || message_size == 0) {
				rsp[0] = STATUS_BAD;
				appreply(hdr, APP_RSP_SIGN_DATA, rsp);
				break;
			}

			if (left > (cmdBytelen - 1)) {
				nbytes = cmdBytelen - 1;
			} else {
				nbytes = left;
			}

			memcpy(&message[msg_idx], cmd + 1, nbytes);
			msg_idx += nbytes;
			left -= nbytes;

			if (left == 0) {
#ifndef TKEY_SIGNER_APP_NO_TOUCH
				wait_touch_ledflash(LED_GREEN, 350000);
#endif
				// All loaded, device touched, let's
				// sign the message
				crypto_ed25519_sign(signature, secret_key,
						    message, message_size);
				signature_done = 1;
				message_size = 0;
			}

			rsp[0] = STATUS_OK;
			appreply(hdr, APP_RSP_SIGN_DATA, rsp);
			break;

		case APP_CMD_SIGN_PH_DATA:
			qemu_puts("APP_CMD_SIGN_PH_DATA\n");
			// Bad length of this command
			if (hdr.len != 128) {
				rsp[0] = STATUS_BAD;
				appreply(hdr, APP_RSP_SIGN_PH_DATA, rsp);
				break;
			}

			memcpy(&message, cmd + 1, 64);

#ifndef TKEY_SIGNER_APP_NO_TOUCH
			wait_touch_ledflash(LED_GREEN, 350000);
#endif
			// sign the pre-hashed message
			crypto_ed25519_ph_sign(signature, secret_key, message);
			signature_done = 1;

			rsp[0] = STATUS_OK;
			appreply(hdr, APP_RSP_SIGN_PH_DATA, rsp);
			break;

		case APP_CMD_GET_SIG:
			qemu_puts("APP_CMD_GET_SIG\n");
			if (signature_done == 0) {
				rsp[0] = STATUS_BAD;
				appreply(hdr, APP_RSP_GET_SIG, rsp);
				break;
			}
			rsp[0] = STATUS_OK;
			memcpy(rsp + 1, signature, 64);
			appreply(hdr, APP_RSP_GET_SIG, rsp);
			break;

		case APP_CMD_GET_NAMEVERSION:
			qemu_puts("APP_CMD_GET_NAMEVERSION\n");
			// only zeroes if unexpected cmdlen bytelen
			if (hdr.len == 1) {
				memcpy(rsp, app_name0, 4);
				memcpy(rsp + 4, app_name1, 4);
				memcpy(rsp + 8, &app_version, 4);
			}
			appreply(hdr, APP_RSP_GET_NAMEVERSION, rsp);
			break;

		default:
			qemu_puts("Received unknown command: ");
			qemu_puthex(cmd[0]);
			qemu_lf();
			appreply(hdr, APP_RSP_UNKNOWN_CMD, rsp);
		}
	}
}
