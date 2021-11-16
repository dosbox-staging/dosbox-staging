#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define ENET_IMPLEMENTATION
#include "enet.h"
#include <stdio.h>

typedef struct {
    ENetHost *host;
    ENetPeer *peer;
} Client;

void host_server(ENetHost *server) {
    ENetEvent event;
    while (enet_host_service(server, &event, 2) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from ::1:%u.\n", event.peer->address.port);
                /* Store any relevant client information here. */
                event.peer->data = "Client information";
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %zu containing %s was received from %s on channel %u.\n",
                        event.packet->dataLength,
                        event.packet->data,
                        (char *)event.peer->data,
                        event.channelID);

                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy (event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf ("%s disconnected.\n", (char *)event.peer->data);
                /* Reset the peer's client information. */
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                printf ("%s timeout.\n", (char *)event.peer->data);
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_NONE: break;
        }
    }
}

int main() {
    if (enet_initialize() != 0) {
        printf("An error occurred while initializing ENet.\n");
        return 1;
    }

    #define MAX_CLIENTS 32

    int i = 0;
    ENetHost *server;
    Client clients[MAX_CLIENTS];
    ENetAddress address = {0};

    address.host = ENET_HOST_ANY; /* Bind the server to the default localhost. */
    address.port = 7777; /* Bind the server to port 7777. */


    /* create a server */
    printf("starting server...\n");
    server = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);
    if (server == NULL) {
        printf("An error occurred while trying to create an ENet server host.\n");
        return 1;
    }

    printf("starting clients...\n");
    for (i = 0; i < MAX_CLIENTS; ++i) {
        enet_address_set_host(&address, "127.0.0.1");
        clients[i].host = enet_host_create(NULL, 1, 2, 0, 0);
        clients[i].peer = enet_host_connect(clients[i].host, &address, 2, 0);
        if (clients[i].peer == NULL) {
            printf("coundlnt connect\n");
            return 1;
        }
    }

    // program will make N iterations, and then exit
    static int counter = 1000;

    do {
        host_server(server);

        ENetEvent event;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            enet_host_service(clients[i].host, &event, 0);
        }

        counter--;
    } while (counter > 0);

    for (i = 0; i < MAX_CLIENTS; ++i) {
        enet_peer_disconnect_now(clients[i].peer, 0);
        enet_host_destroy(clients[i].host);
    }

    host_server(server);

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
