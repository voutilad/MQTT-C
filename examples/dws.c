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
#include <err.h>

#include "dws.h"
#include <mqtt.h>


static const char *MSG = "Test from websocket wrapper!";
static const char *WILL_MSG = "Bye bye bye!";
static const char *HOST = "test.mosquitto.org";
static uint16_t PORT = 8080;

static const char *SUB = "datetime/#";
static char TOPIC[128];
static const char *TOPIC_PATTERN = "datetime/%d/now";
static char WILL[128];
static const char *WILL_PATTERN = "datetime/%d/will";
static char CLIENT[23];
static const char *CLIENT_PATTERN = "dws-%u";

static uint8_t BUF_SEND[4096];
static uint8_t BUF_RECV[4096];


void callback(void **unused, struct mqtt_response_publish *published)
{
    char buf[4096];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, published->topic_name, published->topic_name_size);
    printf("%s: topic=%s, msg=%s\n", __func__, buf,
           (const char*) published->application_message);
}

#define MODE_PUB 1
#define MODE_SUB 2

int main(int argc, const char *argv[])
{
    int rv = 0, client_id, mode = 0;
    enum MQTTErrors result;
    struct websocket ws = { 0 };
    struct mqtt_client client;

    if (argc > 1 && argv[1][0] == 'p')
        mode = MODE_PUB;
    else if (argc > 1 && argv[1][0] == 's')
        mode = MODE_SUB;
    else
        errx(1, "missing mode switch!");

	memset(TOPIC, 0, sizeof(TOPIC));
	memset(WILL, 0, sizeof(WILL));
    memset(CLIENT, 0, sizeof(CLIENT));
	memset(BUF_SEND, 0, sizeof(BUF_SEND));
	memset(BUF_RECV, 0, sizeof(BUF_RECV));

	client_id = getpid();
	snprintf(TOPIC, sizeof(TOPIC), TOPIC_PATTERN, client_id);
	snprintf(WILL, sizeof(WILL), WILL_PATTERN, client_id);
    snprintf(CLIENT, sizeof(CLIENT), CLIENT_PATTERN, client_id);
	printf("Starting client '%s' producing to topic '%s' and using will topic "
           "'%s'\n", CLIENT, TOPIC, WILL);

	/*
	 * Websocket setup.
	 */
    rv = dumb_connect(&ws, HOST, PORT);
    if (rv != 0)
        errx(1, "dumb_connect: rv=%d", rv);
    printf("Connected to %s:%d\n", HOST, PORT);

    rv = dumb_handshake(&ws, "/mqtt", "mqttv3.1");
    if (rv != 0)
        errx(1, "dumb_handshake: rv=%d", rv);
    printf("Completed Websocket handshake\n");

	/*
	 * MQTT setup.
	 */
    mqtt_pal_socket_handle handle = &ws;
    mqtt_init(&client, handle, BUF_SEND, sizeof(BUF_SEND), BUF_RECV,
			  sizeof(BUF_RECV), callback);
    result = mqtt_connect(&client, CLIENT, WILL, WILL_MSG, strlen(WILL_MSG),
                          NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 10);
	if (result != MQTT_OK)
        errx(1, "mqtt_connect: %s", mqtt_error_str(client.error));
    printf("Client connected\n");


    if (mode == MODE_SUB) {
        result = mqtt_subscribe(&client, SUB, 0);
        if (result != MQTT_OK)
            errx(1, "mqtt_subscribe: %s", mqtt_error_str(client.error));
        printf("Subscribed to '%s'\n", SUB);
    }

    for (;;) {
        if (mode == MODE_PUB) {
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
        usleep(1000000U);
    }

    rv = dumb_close(&ws);
    if (rv != 0)
        errx(1, "dumb_close");

    printf("Disconnected\n");

    return 0;
}
