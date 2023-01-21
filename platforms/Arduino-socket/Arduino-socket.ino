//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
#include <WARDuino.h>

#include "Arduino.h"
#include "SocketCommunication.h"
#include "upload.h"

unsigned int wasm_len = upload_wasm_len;
unsigned char* wasm = upload_wasm;

WARDuino* wac = WARDuino::instance();
Module* m;

struct WiFiCredentials wiFiCredentials = {"SSID", "pswd"};

SocketCommunication* serverForTool;
SocketCommunication* serverForProxy;

uint16_t proxyport = 8080;
uint16_t toolport = 8081;

#define UART_PIN 3

void onData(uint8_t* buff, size_t len) { wac->handleInterrupt(len, buff); }

void onNewProxyClient(AsyncClient* c) {
    wac->debugger->setProxyChannel(new ServerSideSocket(c));
}

void onNewToolClient(AsyncClient* c) {
    wac->debugger->setChannel(new ServerSideSocket(c));
}

void onGoneProxyClient() {
    wac->debugger->setProxyChannel(0);  // default channel
}

void onGoneToolClient() {
    wac->debugger->setChannel(0);  // default channel
}

void setup(void) {
    Serial.begin(115200);
    //    attachInterrupt(UART_PIN, handleInput, CHANGE);
    while (!Serial)
        ;

    Serial.println(ESP.getFreeHeap());
    Serial.println("Total heap:");
    Serial.println(ESP.getHeapSize());
    Serial.println("\nFree heap:");
    Serial.println(ESP.getFreeHeap());
    Serial.println("\nTotal PSRAM:");
    Serial.println(ESP.getPsramSize());
    Serial.println("\nFree PSRAM: ");
    Serial.println(ESP.getFreePsram());

    // create & connect SocketServer
    serverForProxy =
        new SocketCommunication("ProxyServer", proxyport, &onData,
                                &onNewProxyClient, &onGoneProxyClient);
    serverForTool = new SocketCommunication(
        "ToolServer", toolport, &onData, &onNewToolClient, &onGoneToolClient);

    SocketCommunication::connect2Wifi(wiFiCredentials);
    serverForProxy->begin();
    serverForTool->begin();
}

void loop() {
    disableCore0WDT();
    m = wac->load_module(wasm, wasm_len, {});

    printf("LOADED \n\n");
    uint8_t command[] = {'0', '3', '\n'};
    wac->handleInterrupt(3, command);
    //    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL, 1, NULL);
    printf("START\n\n");

    Serial.println("\nFree heap:");
    Serial.println(ESP.getFreeHeap());

    wac->run_module(m);
    //    printf("END\n\n");
    wac->unload_module(m);
}
