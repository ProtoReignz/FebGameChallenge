//server.cpp

#define ENV_ITEMS_IMPL
#include "game.h"
#include "raymath.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct ServerPlayer {
    NetPlayer net;
    ENetPeer *peer;
    float speed;

    uint8_t left, right, up, down;
    float mouse_x, mouse_y;
    uint8_t shooting;
} ServerPlayer;


//Hitboxes

static Rectangle PlayerBox(NetPlayer *p){
    return(Rectangle){
        p->x - p->width / 2.0f,
        p->y - p->height/2.0f,
        p->width, 
        p->height
    };
}


static Rectangle ProjBox(NetProjectile *pr){
    return (Rectangle){
        pr->x - pr->radius,
        pr->y - pr->radius,
        pr->radius * 2.0f, 
        pr->radius * 2.0f
    };
}

static void ServerUpdatePlayer(ServerPlayer *sp, float delta){
    NetPlayer *p = &sp -> net; 
    if (!p->active) return; //if not init leave

    Vector2 movement = {0.0f, 0.0f};
    
    if (sp -> left) movement.x -= 1.0f;
    if (sp -> right) movement.x += 1.0f;
    if (sp -> up) movement.y -= 1.0f;
    if (sp -> down) movement.y += 1.0f;

    if(movement.x != 0.0f && movement.y != 0.0f){
        movement = Vector2Normalize(movement);
    }


    // X-Axis Collision detection
    p->x += movement.x * PLAYER_SPD * delta;
    Rectangle playerRect = PlayerBox(p);

    for (int i = 0; i < g_envItemsCount; i++){
        if (!g_envItems[i].blocking) continue;
        if (CheckCollisionRecs(playerRect, g_envItems[i].rect)){
            p->x -= movement.x * PLAYER_SPD * delta;
            break;
        }
    }

    // Y-Axis Collision Detection

    p->y += movement.y * PLAYER_SPD * delta;
    playerRect = PlayerBox(p);

    for (int i = 0; i < g_envItemsCount; i++){
        if (!g_envItems[i].blocking) continue;
        if (CheckCollisionRecs(playerRect, g_envItems[i].rect)){
            p->y -= movement.y * PLAYER_SPD * delta;
            break;
        }
    }
}

static void ServerUpdateProjectiles(NetProjectile *projs, ServerPlayer *players, float delta){
    for (int i = 0; i < MAX_PROJECTILES; i++){
        if (!projs[i].active) continue;


        projs[i].x += projs[i].vx * delta;
        projs[i].y += projs[i].vy * delta;

        if (projs[i].x < -100 || projs[i].x > 1100 || projs[i].y < -100 || projs[i].y > 700) {
                projs[i].active = false;
                continue;
        }

        Rectangle pbox = ProjBox(&projs[i]);
        // Projectile Collision
        Rectangle projRect = ProjBox(&projs[i]);
        for (int j = 0; j < g_envItemsCount; j++){
            if (!g_envItems[j].blocking) continue;
            else if(CheckCollisionRecs(pbox, g_envItems[j].rect)){
                projs[i].active = false;
                break;
            }
            //check for player collision here
        }

        if(!projs[i].active) continue;

        for (int j = 0; j < MAX_PLAYERS; j++){
            if (!players[j].net.active) continue;
            Rectangle playerBox = PlayerBox(&players[j].net);
            if (CheckCollisionRecs(pbox, playerBox)){
                projs[i].active = 0;
                break;
            }
        }
    }
}

static void BroadcastGameState(ENetHost *server, ServerPlayer *players,
                                NetProjectile *projs, uint8_t your_id_unused) {
    MsgGameState msg;
    msg.type    = MSG_GAME_STATE;
    msg.your_id = 0; // overridden per-client below

    for (int i = 0; i < MAX_PLAYERS; i++)
        msg.players[i] = players[i].net;
    for (int i = 0; i < MAX_PROJECTILES; i++)
        msg.projectiles[i] = projs[i];

    // Send a personalised copy to each client so they know which player they are
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].net.active) continue;
        msg.your_id = (uint8_t)i;

        // ENET_PACKET_FLAG_UNSEQUENCED = no ordering guarantee, lowest latency.
        // For game state snapshots this is fine — old frames are useless anyway.
        ENetPacket *packet = enet_packet_create(&msg, sizeof(msg),
                                                ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(players[i].peer, 1, packet);
        // Channel 1 = unreliable game-state channel
        // Channel 0 = reliable events (join/leave/shop later)
    }
}

static void ServerSpawnProjectile(NetProjectile *projs, float ox, float oy,
                                   float mx, float my) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projs[i].active) {
            projs[i].active = 1;
            projs[i].x      = ox;
            projs[i].y      = oy;
            projs[i].radius = PROJECTILES_RADIUS;

            Vector2 dir = Vector2Normalize(
                Vector2Subtract((Vector2){mx, my}, (Vector2){ox, oy})
            );
            projs[i].vx = dir.x * PROJECTILES_SPD;
            projs[i].vy = dir.y * PROJECTILES_SPD;
            break;
        }
    }
}

int main(void) {
    if (enet_initialize() != 0) {
        fprintf(stderr, "ENet init failed\n");
        return 1;
    }

    atexit(enet_deinitialize);

    // Bind to all interfaces on port 7777
    ENetAddress address = { .host = ENET_HOST_ANY, .port = 7777 };

    // enet_host_create(address, maxClients, channels, inBandwidth, outBandwidth)
    // 0 bandwidth = unlimited
    ENetHost *server = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);
    if (!server) {
        fprintf(stderr, "Failed to create server host\n");
        return 1;
    }
    printf("Server listening on port 7777\n");

    ServerPlayer players[MAX_PLAYERS] = { 0 };
    NetProjectile projectiles[MAX_PROJECTILES] = { 0 };

    // Fixed timestep: server ticks at 60hz regardless of enet_host_service timing
    const float TICK_RATE = 1.0f / 60.0f;
    double      lastTime  = 0.0;  // we'll use a simple counter approach

    // For server timing we use enet_time (milliseconds since enet_initialize)
    uint32_t lastTick = enet_time_get();

    while (1) {
        // --- Process network events ---
        // enet_host_service(host, event, timeout_ms)
        // timeout=0 means non-blocking: returns immediately if nothing queued.
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {

            case ENET_EVENT_TYPE_CONNECT: {
                // Find an empty player slot
                int slot = -1;
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (!players[i].net.active) { slot = i; break; }
                }
                if (slot < 0) {
                    // Server full — disconnect them immediately
                    enet_peer_disconnect(event.peer, 0);
                    printf("Connection refused: server full\n");
                    break;
                }

                printf("Player %d connected\n", slot);
                players[slot].net.active = 1;
                players[slot].net.id     = (uint8_t)slot;
                players[slot].net.x      = 100.0f + slot * 50.0f;
                players[slot].net.y      = 100.0f;
                players[slot].net.width  = PLAYER_WIDTH;
                players[slot].net.height = PLAYER_HEIGHT;
                players[slot].peer       = event.peer;
                players[slot].speed      = PLAYER_SPD;

                // Store slot in peer->data so we can identify them on disconnect
                // event.peer->data is a void* you can use freely
                event.peer->data = (void*)(intptr_t)slot;

                // Notify all clients someone joined
                MsgPlayerEvent join = { MSG_PLAYER_JOINED, (uint8_t)slot };
                ENetPacket *pkt = enet_packet_create(&join, sizeof(join),
                                                     ENET_PACKET_FLAG_RELIABLE);
                enet_host_broadcast(server, 0, pkt);
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet->dataLength < 1) {
                    enet_packet_destroy(event.packet);
                    break;
                }

                uint8_t msgType = event.packet->data[0];

                if (msgType == MSG_CLIENT_INPUT &&
                    event.packet->dataLength == sizeof(MsgClientInput)) {

                    MsgClientInput *input = (MsgClientInput*)event.packet->data;
                    int slot = (int)(intptr_t)event.peer->data;

                    players[slot].left    = input->left;
                    players[slot].right   = input->right;
                    players[slot].up      = input->up;
                    players[slot].down    = input->down;
                    players[slot].mouse_x = input->mouse_mapX;
                    players[slot].mouse_y = input->mouse_mapY;
                    players[slot].shooting = input->shooting;
                }

                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                int slot = (int)(intptr_t)event.peer->data;
                printf("Player %d disconnected\n", slot);
                memset(&players[slot], 0, sizeof(ServerPlayer));

                MsgPlayerEvent leave = { MSG_PLAYER_LEFT, (uint8_t)slot };
                ENetPacket *pkt = enet_packet_create(&leave, sizeof(leave),
                                                     ENET_PACKET_FLAG_RELIABLE);
                enet_host_broadcast(server, 0, pkt);
                break;
            }

            default: break;
            }
        }

        // --- Fixed timestep tick ---
        uint32_t now   = enet_time_get();
        uint32_t delta = now - lastTick;

        if (delta >= (uint32_t)(TICK_RATE * 1000.0f)) {
            float dt = delta / 1000.0f;
            lastTick = now;

            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].net.active && players[i].shooting) {
                    ServerSpawnProjectile(projectiles,
                        players[i].net.x, players[i].net.y,
                        players[i].mouse_x, players[i].mouse_y);
                    players[i].shooting = 0; // consume the shoot event
                }
                ServerUpdatePlayer(&players[i], dt);
            }
            ServerUpdateProjectiles(projectiles, players, dt);
            BroadcastGameState(server, players, projectiles, 0);
        }

        // Tiny sleep to avoid burning 100% CPU in the polling loop.
        // enet_host_service with timeout=1 would also work.
        enet_host_service(server, &event, 1);
    }

    enet_host_destroy(server);
    return 0;
}

