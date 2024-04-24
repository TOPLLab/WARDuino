#include <zephyr/console/console.h>
#include <zephyr/console/tty.h>
#include <zephyr/drivers/uart.h>

#include "../../src/WARDuino.h"
#include "upload.h"

#define DEBUGGER_STACK_SIZE 2048
#define DEBUGGER_PRIORITY 0

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

static struct tty_serial console_serial;

static uint8_t console_rxbuf[CONFIG_CONSOLE_GETCHAR_BUFSIZE];
static uint8_t console_txbuf[CONFIG_CONSOLE_PUTCHAR_BUFSIZE];

ssize_t war_console_read(void *dummy, void *buf, size_t size) {
    ARG_UNUSED(dummy);
    return tty_read(&console_serial, buf, size);
}

int war_console_init(void) {
    const struct device *uart_dev;
    int ret;

    uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev)) {
        return -ENODEV;
    }

    ret = tty_init(&console_serial, uart_dev);

    if (ret) {
        return ret;
    }

    /* Checks device driver supports for interrupt driven data transfers. */
    if (CONFIG_CONSOLE_GETCHAR_BUFSIZE + CONFIG_CONSOLE_PUTCHAR_BUFSIZE) {
        const struct uart_driver_api *api =
            (const struct uart_driver_api *)uart_dev->api;
        if (!api->irq_callback_set) {
            return -ENOTSUP;
        }
    }

    tty_set_tx_buf(&console_serial, console_txbuf, sizeof(console_txbuf));
    tty_set_rx_buf(&console_serial, console_rxbuf, sizeof(console_rxbuf));

    console_serial.rx_timeout = 200;

    return 0;
}

WARDuino *wac = WARDuino::instance();

void startDebuggerStd() {
    Channel *duplex = new Duplex(stdin, stdout);
    wac->debugger->setChannel(duplex);
    duplex->open();

    war_console_init();
    int valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        k_msleep(1000);

        while ((valread = war_console_read(NULL, buffer, 1024)) > 0) {
            wac->handleInterrupt(valread, buffer);
        }
    }
}

K_THREAD_DEFINE(debugger_tid, DEBUGGER_STACK_SIZE, startDebuggerStd, NULL, NULL,
                NULL, DEBUGGER_PRIORITY, 0, 0);

int main(void) {
    Module *m = wac->load_module(upload_wasm, upload_wasm_len, {});
    wac->run_module(m);
    wac->unload_module(m);

    return 0;
}
