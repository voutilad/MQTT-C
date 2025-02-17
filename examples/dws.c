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
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#include "dws.h"
#include <mqtt.h>

#define MODE_PUB 1
#define MODE_SUB 2

static char MSG[128];
static const char *MSG_PATTERN = "Test from websocket wrapper! tick=%u";
static const char *WILL_MSG = "Bye bye bye!";
static const char *DEFAULT_HOST = "test.mosquitto.org";
static uint16_t DEFAULT_PORT = 8080;
static const char *DEFAULT_PATH = "/mqtt";

static const char *SUB = "datetime/#";
static char TOPIC[128];
static const char *TOPIC_PATTERN = "datetime/%d/now";
static char WILL[128];
static const char *WILL_PATTERN = "datetime/%d/will";
static char CLIENT[23];
static const char *CLIENT_PATTERN = "dws%u";

static uint8_t BUF_SEND[4096];
static uint8_t BUF_RECV[4096];

static int KEEP_GOING = 1;

void sighandler(int sig)
{
    if (KEEP_GOING) {
        printf("\b\bDisconnecting...please wait!\n");
        KEEP_GOING = 0;
    }
}

void callback(void **unused, struct mqtt_response_publish *published)
{
    char buf[4096];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, published->topic_name, published->topic_name_size);
    printf("%s: topic=%s, msg=%s\n", __func__, buf,
           (const char*) published->application_message);
}

int main(int argc, const char *argv[])
{
    int rv = 0, client_id, mode = 0;
    const char *host = DEFAULT_HOST;
    const char *path = DEFAULT_PATH;
    uint16_t port = DEFAULT_PORT;
    unsigned int tick = 0;
    enum MQTTErrors result;
    struct mqtt_client client;
    struct websocket ws = { 0 };

    if (argc > 1 && argv[1][0] == 'p')
        mode = MODE_PUB;
    else if (argc > 1 && argv[1][0] == 's')
        mode = MODE_SUB;
    else
        errx(1, "missing mode switch!");

    if (argc > 2) {
        host = argv[2];
        if (argc > 3) {
            port = atoi(argv[3]);
            if (argc > 4)
                path = argv[4];
        }
    }


	memset(TOPIC, 0, sizeof(TOPIC));
	memset(WILL, 0, sizeof(WILL));
    memset(CLIENT, 0, sizeof(CLIENT));
	memset(BUF_SEND, 0, sizeof(BUF_SEND));
	memset(BUF_RECV, 0, sizeof(BUF_RECV));

	client_id = getpid();
	snprintf(TOPIC, sizeof(TOPIC), TOPIC_PATTERN, client_id);
	snprintf(WILL, sizeof(WILL), WILL_PATTERN, client_id);
    snprintf(CLIENT, sizeof(CLIENT), CLIENT_PATTERN, client_id);
	printf("Starting client '%s' %s to %s:%hu\n", CLIENT,
           mode == MODE_PUB ? "publishing" : "subscribing",
           host, port);

	/*
	 * Websocket setup.
	 */
    rv = dumb_connect(&ws, host, port);
    if (rv != 0)
        errx(1, "dumb_connect: rv=%d", rv);
    printf("Connected to %s:%d\n", host, port);

    rv = dumb_handshake(&ws, path, "mqtt");
    if (rv != 0)
        errx(1, "dumb_handshake: rv=%d", rv);
    printf("Completed Websocket handshake with ws://%s:%d%s\n",
           host, port, path);

	/*
	 * MQTT setup.
	 */
    mqtt_pal_socket_handle handle = &ws;
    mqtt_init(&client, handle, BUF_SEND, sizeof(BUF_SEND), BUF_RECV,
			  sizeof(BUF_RECV), callback);
    result = mqtt_connect(&client, NULL, NULL, NULL, 0,
                          NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 30);
	if (result != MQTT_OK)
        errx(1, "mqtt_connect: %s", mqtt_error_str(client.error));
    printf("Client connected\n");

    if (mode == MODE_SUB) {
        result = mqtt_subscribe(&client, SUB, 0);
        if (result != MQTT_OK)
            errx(1, "mqtt_subscribe: %s", mqtt_error_str(client.error));
        printf("Subscribed to '%s'\n", SUB);
    }

    // Prepare our loop controls.
    signal(SIGINT, sighandler);

    // Pump it.
    while (KEEP_GOING) {
        // Produce every 5 "ticks" so we don't spam the test server :-)
        if (mode == MODE_PUB && (tick++) % 5 == 0) {
            memset(MSG, 0, sizeof(MSG));
            snprintf(MSG, sizeof(MSG), MSG_PATTERN, tick);
            result = mqtt_publish(&client, TOPIC, MSG, strlen(MSG),
							  MQTT_PUBLISH_QOS_0);
            if (result != MQTT_OK)
                errx(1, "mqtt_publish: %s", mqtt_error_str(client.error));
            printf("Published to '%s': %s\n", TOPIC, MSG);
        }

        result = mqtt_sync(&client);
        if (result != MQTT_OK) {
            printf("error: %s\n", mqtt_error_str(client.error));
            break;
        }

        // We use usleep(3) here because for some reason macOS's nanosleep(2)
        // is a total fraud and can sleep wayyyy too long.
        usleep(200 * 1000); // 200ms

        printf("...tick...\n");
    }

    result = mqtt_disconnect(&client);
    if (result != MQTT_OK)
        errx(1, "mqtt_disconnect");

    rv = dumb_close(&ws);
    if (rv != 0)
        errx(1, "dumb_close");

    printf("Disconnected. Bye!\n");

    return 0;
}
