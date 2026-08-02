#include "../includes/main.h"
#include <stdlib.h>
#include <signal.h>

static Client g_client;

static void sig_handler(int sig) {
    printf("\n[main] signal %d, stopping...\n", sig);
    g_client.running = 0;
}

int main(int argc, char const *argv[]) {

    printf("[embedded]: current firmware version: %s\n", DEVICE_VERSION);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    const char* ip    = (argc > 1) ? argv[1] : DEFAULT_IP;
    int         port  = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    const char* name  = (argc > 3) ? argv[3] : DEFAULT_NAME;
    const char* group = (argc > 4) ? argv[4] : DEFAULT_GROUP;
    const char* uid   = (argc > 5) ? argv[5] : NULL;

    printf("[main] starting: name=%s group=%s server=%s:%d\n",
           name, group, ip, port);

    if (client_init(&g_client, ip, port, name, group, uid) < 0) {
        fprintf(stderr, "client_init failed\n");
        return 1;
    }

    client_run(&g_client);
    client_destroy(&g_client);

    return 0;
}
