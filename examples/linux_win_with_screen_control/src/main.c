#include "lwm2m_connect.h"
#include "lwm2m_objects.h"
#include "client_debug.h"
#include "network.h"
#include "screen_object.h"
// For c++ projects with firmware support
// #include "object_firmware.hpp"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

static int g_quit = 0;

void handle_sigint(int signum)
{
    printf("Wait for socket timeout...\n");
    g_quit = 1;
}

int main(int argc, char *argv[])
{
    /*
     * We catch Ctrl-C signal for a clean exit
     */
    signal(SIGINT, handle_sigint);
    
    // For c++ projects with firmware support
    //checkIsUpdated(argc, argv);

    device_instance_t * device_data = lwm2m_device_data_get();
    device_data->manufacturer = "test manufacturer";
    device_data->model_name = "test model";
    device_data->device_type = "sensor";
    device_data->firmware_ver = "1.0";
    device_data->serial_number = "140234-645235-12353";
    lwm2m_context_t * lwm2mH = lwm2m_client_init("testClient");
    if (lwm2mH == 0)
    {
        fprintf(stderr, "Failed to initialize wakaama\r\n");
        return -1;
    }

    // Create object with the C-Object API
    lwm2m_object_t* test_object = get_screen_object();
    lwm2m_add_initialize_object(lwm2mH, test_object, false);
    lwm2m_object_instance_add(lwm2mH, test_object, get_screen_instance());

    uint8_t bound_sockets = lwm2m_network_init(lwm2mH, NULL);

    if (bound_sockets == 0)
    {
        fprintf(stderr, "Failed to open socket: %d %s\r\n", errno, strerror(errno));
        return -1;
    }
    
    // Connect to the lwm2m server with unique id 123, lifetime of 100s, no storing of
    // unsend messages. The host url is either coap:// or coaps://.
    lwm2m_add_server(123, "coap://192.168.1.18", 100, false);
    
    // If you want to establish a DTLS secured connection, you need to alter the security
    // information:
    // lwm2m_server_security_preshared(123, "publicid", "password", sizeof("password"));

    /*
     * We now enter a while loop that will handle the communications from the server
     */
    while (0 == g_quit)
    {
        struct timeval tv = {5,0};
        fd_set readfds = {0};
        for (uint8_t c = 0; c < bound_sockets; ++c)
            FD_SET(lwm2m_network_native_sock(lwm2mH, c), &readfds);

        print_state(lwm2mH);

        uint8_t result = lwm2m_step(lwm2mH, &tv.tv_sec);
        if (result == COAP_503_SERVICE_UNAVAILABLE)
            printf("No server found so far\r\n");
        else if (result != 0)
            fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);

        /*
         * This part wait for an event on the socket until "tv" timed out (set
         * with the precedent function)
         */
        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0 && errno != EINTR)
        {
            fprintf(stderr, "Error in select(): %d %s\r\n", errno, strerror(errno));
        }

        for (uint8_t c = 0; c < bound_sockets; ++c)
        {
            if (result > 0 && FD_ISSET(lwm2m_network_native_sock(lwm2mH, c), &readfds))
            {
                lwm2m_network_process(lwm2mH);
            }
        }
    }

    printf("finished\n");

    lwm2m_remove_object(lwm2mH, test_object->objID);
    lwm2m_client_close();

    return 0;
}
