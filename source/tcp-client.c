/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/netifapi.h"

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#include <stdio.h>

#include "dnet.h"

#define HANDLE_ERROR(error, handler) if (error) { handler; }

volatile char send_data = 0;

static err_t send(struct netconn* conn, const char* data, const size_t len) {
	err_t err;

	err = netconn_write(conn, data, len, NETCONN_NOFLAG);
	HANDLE_ERROR(err != ERR_OK, return err);
}

static err_t receive(struct netconn* conn, char* data) {
	HANDLE_ERROR(!data, return ERR_BUF);

	struct netbuf* buf;
	err_t err = netconn_recv(conn, &buf);
	HANDLE_ERROR(err != ERR_OK, return err);

	char* rq = buf->ptr->payload;
	uint16_t len = buf->ptr->len;

	memcpy(data, rq, len);
	data[len] = 0;

	netbuf_delete(buf);

	return err;
}

static void manage_tcp(void* param)
{
	(void)param;

	struct netconn* conn = netconn_new(NETCONN_TCP);
	assert(conn != NULL);

	ip_addr_t server;
	ip4addr_aton("192.168.0.103", &server);

	err_t err = ERR_OK;
	while(1)
	{
		err = netconn_connect(conn, &server, 8080);
		if(err)
			PRINTF("Client - Error occurred while connecting : %d\r\n", err);
		assert(err == ERR_OK);

		PRINTF("Client - Connected\r\n");

		while (err == ERR_OK)
		{
			if (send_data)
			{
				const char* msg = (send_data == 1) ? "LED ON" : "LED OFF";

				send_data = 0;

				char buf[32];
				memset(buf, 0, 32);

	//			err_t err;

				PRINTF("Client - Sending: %s\r\n", msg);
				err = send(conn, msg, strlen(msg));
				HANDLE_ERROR(err != ERR_OK, PRINTF("Client - Error occurred while sending data: %d\r\n", err));

				err = receive(conn, buf);
				HANDLE_ERROR(err != ERR_OK, PRINTF("Client - Error occurred while receiving data: %d\r\n", err));

				if (!memcmp(buf, "Client - LED ON", strlen(buf))) {
					LED_BLUE_ON();
				} else {
					LED_BLUE_OFF();
				}

			}
		}
		conn = netconn_new(NETCONN_TCP);
		assert(conn != NULL);
	}
}

void BOARD_SW3_IRQ_HANDLER(void) {
	GPIO_PortClearInterruptFlags(BOARD_SW3_GPIO, 1U << BOARD_SW3_GPIO_PIN);

	uint32_t event = GPIO_PinRead(BOARD_SW3_GPIO, BOARD_SW3_GPIO_PIN);

	send_data = event + 1;

	SDK_ISR_EXIT_BARRIER;
}

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

	PRINTF("Start\r\n");

	EnableIRQ(BOARD_SW3_IRQ);

    PRINTF("Client - Initializing network\r\n");
    uint8_t mac[] = { 0x02, 0x12, 0x13, 0x10, 0x15, 0x13 };
    DnetConfig config = dnet_init("192.168.0.102", NULL, NULL, mac);

    dnet_start_new_thread("manage_tcp", manage_tcp, NULL);

    vTaskStartScheduler();

    return 0;
}
#endif
