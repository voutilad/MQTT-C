/*
MIT License

Copyright(c) 2023 Dave Voutila

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include "dws.h"
#include <mqtt.h>

int main(int argc, const char *argv[])
{
    int rv = 0;
    enum MQTTErrors result;
    struct websocket ws = { 0 };

    rv = dumb_connect(&ws, "test.mosquitto.org", "8080");
    if (rv != 0)
        errx(1, "dumb_connect");

    printf("Connected\n");

    rv = dumb_handshake(&ws, "test.mosquitto.org", "/mqtt", "mqttv3.1");
    if (rv != 0)
        errx(1, "dumb_handshake");

    printf("Completed handshake\n");

    rv = dumb_ping(&ws);
    if (rv != 0)
        errx(1, "dumb_ping");

    printf("Pinged\n");

    // Start the MQTT junk
    struct mqtt_client client;
    uint8_t sendbuf[4096];
    uint8_t recvbuf[4096];

    mqtt_pal_socket_handle handle = &ws;
    mqtt_init(&client, handle, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), NULL);
    result = mqtt_connect(&client, "dws-client", NULL, NULL, 0, NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 400);
    if (result != MQTT_OK)
        errx(1, "mqtt_connect: %s", mqtt_error_str(result));

    if (client.error != MQTT_OK)
        errx(1, "error: %s", mqtt_error_str(client.error));

    printf("Client connected\n");

    rv = dumb_close(&ws);
    if (rv != 0)
        errx(1, "dumb_close");

    printf("Disconnected\n");

    return 0;
}
