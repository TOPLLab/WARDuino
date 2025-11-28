#include <zephyr/console/console.h>
#include <zephyr/console/tty.h>
#include <zephyr/drivers/uart.h>

#include <vector>

#include "../../src/WARDuino.h"
#include "upload.h"

#define DEBUGGER_STACK_SIZE 2048
#define DEBUGGER_PRIORITY 0

/*BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console),
   zephyr_cdc_acm_uart), "Console device is not ACM CDC UART device");*/

static struct tty_serial console_serial;

static uint8_t console_rxbuf[CONFIG_CONSOLE_GETCHAR_BUFSIZE];
static uint8_t console_txbuf[CONFIG_CONSOLE_PUTCHAR_BUFSIZE];

struct ModuleInfo {
    unsigned char *wasm;
    unsigned int wasm_len;
    const char *name;
};

ModuleInfo modules[] = {
    {upload_wasm, upload_wasm_len, "main"},
};
const size_t module_count = sizeof(modules) / sizeof(modules[0]);

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
std::vector<Module *> loaded_modules;

void startDebuggerStd() {
    Channel *duplex = new Duplex(stdin, stdout);
    wac->debugger->setChannel(duplex);
    duplex->open();

    war_console_init();
    int valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        k_msleep(500);

        while ((valread = war_console_read(NULL, buffer, 1024)) > 0) {
            wac->handleInterrupt(valread, buffer);
        }
    }
}

K_THREAD_DEFINE(debugger_tid, DEBUGGER_STACK_SIZE, startDebuggerStd, NULL, NULL,
                NULL, DEBUGGER_PRIORITY, 0, 0);

int main(void) {
    // Load all modules
    for (size_t i = 0; i < module_count; i++) {
        Module *mod = wac->load_module(modules[i].wasm, modules[i].wasm_len,
                                       modules[i].name,
                                       {.disable_memory_bounds = false,
                                        .mangle_table_index = false,
                                        .dlsym_trim_underscore = false,
                                        .return_exception = true});

        if (mod) {
            loaded_modules.push_back(mod);
            printk("  ✓ Loaded %s (%u bytes)\n", modules[i].name,
                   modules[i].wasm_len);
        } else {
            printk("  ✗ Failed to load %s\n", modules[i].name);
        }
    }

    if (!loaded_modules.empty()) {
        Module *m = loaded_modules.back();
        wac->run_module(m);
    }

    for (auto mod : loaded_modules) {
        wac->unload_module(mod);
    }
    loaded_modules.clear();

    return 0;
}