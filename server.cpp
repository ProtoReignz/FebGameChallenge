//server.cpp

#define ENV_ITEMS_IMPL
#include "game.h"

static_assert(sizeof(MsgGameState)<20000,"Packet too large");

#include "raymath.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>

struct ServerPlayer {
    NetPlayer net {};
    ENetPeer* peer {nullptr};
    float speed {PLAYER_SPD};

    uint8_t left {};
    uint8_t right {};
    uint8_t up {};
    uint8_t down {};

    float mouse_x {};
    float mouse_y {};
    uint8_t shooting;
};


//Hitboxes

static Rectangle PlayerBox(const NetPlayer* p){
    return(Rectangle){
        p->x - p->width / 2.0f,
        p->y - p->height/2.0f,
        p->width, 
        p->height
    };
}


static Rectangle ProjBox(const NetProjectile* pr){
    return (Rectangle){
        pr->x - pr->radius,
        pr->y - pr->radius,
        pr->radius * 2.0f, 
        pr->radius * 2.0f
    };
}

static void ServerUpdatePlayer(ServerPlayer& sp, float delta){
    
    if (!sp.net.active) return;
    
    Vector2 movement = {0.0f, 0.0f};
    
    if (sp.left) movement.x -= 1.0f;
    if (sp.right) movement.x += 1.0f;
    if (sp.up) movement.y -= 1.0f;
    if (sp.down) movement.y += 1.0f;

    if(movement.x != 0.0f && movement.y != 0.0f){
        movement = Vector2Normalize(movement);
    }


    // X-Axis Collision detection
    sp.net.x += movement.x * PLAYER_SPD * delta;
    Rectangle box = PlayerBox(&sp.net);

    for (int i = 0; i < g_envItemsCount; i++){
        if (!g_envItems[i].blocking) continue;
        if (CheckCollisionRecs(box, g_envItems[i].rect)){
            sp.net.x -= movement.x * PLAYER_SPD * delta;
            break;
        }
    }

    // Y-Axis Collision Detection

    sp.net.y += movement.y * PLAYER_SPD * delta;
    box = PlayerBox(&sp.net);

    for (int i = 0; i < g_envItemsCount; i++){
        if (!g_envItems[i].blocking) continue;
        if (CheckCollisionRecs(box, g_envItems[i].rect)){
            sp.net.y -= movement.y * PLAYER_SPD * delta;
            break;
        }
    }
}

static void ServerSpawnProjectile(NetProjectile* projs, uint8_t owner, float ox, float oy, float mx, float my) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projs[i].active) {
            projs[i].active = 1;
            projs[i].owner = owner;
            projs[i].x = ox;
            projs[i].y = oy;
            projs[i].radius = PROJECTILES_RADIUS;
 
            const Vector2 dir = Vector2Normalize(
                Vector2Subtract({ mx, my }, { ox, oy })
            );
            
            projs[i].vx = dir.x * PROJECTILES_SPD;
            projs[i].vy = dir.y * PROJECTILES_SPD;
            break;
        }
    }
}


static void ServerUpdateProjectiles(NetProjectile* projs, ServerPlayer* players, float delta){
    for (int i = 0; i < MAX_PROJECTILES; i++){
        if (!projs[i].active) continue;


        projs[i].x += projs[i].vx * delta;
        projs[i].y += projs[i].vy * delta;


        //Change for when maps are dynamic
        if (projs[i].x < -100 || projs[i].x > 1100 || projs[i].y < -100 || projs[i].y > 700) {
                projs[i].active = false;
                continue;
        }

        const Rectangle pbox = ProjBox(&projs[i]);
        // Projectile Collision
        for (int j = 0; j < g_envItemsCount; j++){
            if (!g_envItems[j].blocking) continue;
            if(CheckCollisionRecs(pbox, g_envItems[j].rect)){
                projs[i].active = false;
                break;
            }
            //check for player collision here
        }

        if(!projs[i].active) continue;


        //Collision with person ADD DAMAGE AND fix so that a projectile CAN't Hit its owner
        for (int j = 0; j < MAX_PLAYERS; j++){
            if (!players[j].net.active) continue;
            if (j==projs[i].owner) continue;

            Rectangle playerBox = PlayerBox(&players[j].net);
            if (CheckCollisionRecs(pbox, playerBox)){
                projs[i].active = 0;
                break;
            }
        }
    }
}

static void BroadcastGameState(ENetHost* server, ServerPlayer* players, NetProjectile* projs) {
    MsgGameState msg {};
    msg.type    = static_cast<uint8_t>(MessageType::GAME_STATE);

    for (int i = 0; i < MAX_PLAYERS; i++)
        msg.players[i] = players[i].net;
    for (int i = 0; i < MAX_PROJECTILES; i++)
        msg.projectiles[i] = projs[i];

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].net.active) continue;
        if (players[i].peer == nullptr) continue;

        msg.your_id = static_cast<uint8_t>(i);

        ENetPacket* packet = enet_packet_create(&msg, sizeof(msg), 0);
        
       // std::printf("Broadcasting to player %d, packet size=%zu\n", i, sizeof(msg));
        
        enet_peer_send(players[i].peer, 1, packet);
    }
}

static void HandleConnect(ENetEvent& event, ServerPlayer* players, ENetHost* server){
    int slot = -1;
    for (int i = 0; i < MAX_PLAYERS; i++){
        if (!players[i].net.active) {slot = i; break;}
    }

    if (slot<0){
        enet_peer_disconnect(event.peer, 0);
        std::printf("Connection refused: Server FULL\n");
        return;
    }

    std::printf("Player %d Connected\n", slot);

    players[slot].net.active = 1;
    players[slot].net.id = static_cast<uint8_t>(slot);
    players[slot].net.x = 100.0f +slot*50.0f;
    players[slot].net.y = 100.0f;
    players[slot].net.width = PLAYER_WIDTH;
    players[slot].net.height = PLAYER_HEIGHT;
    players[slot].peer = event.peer;
    players[slot].speed = PLAYER_SPD;

    //this avoids heap allocation for the slot integer
    event.peer->data = reinterpret_cast<void*>(static_cast<intptr_t>(slot));

    MsgPlayerEvent join {
        .type = static_cast<uint8_t>(MessageType::PLAYER_JOINED),
        .id = static_cast<uint8_t>(slot),
    };
    ENetPacket* pkt = enet_packet_create(&join, sizeof(join), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, 0, pkt);
}

static void HandleReceive(ENetEvent& event, ServerPlayer* players){
    if (event.packet->dataLength < 1){
        enet_packet_destroy(event.packet);
        return;
    }

    const uint8_t msgType = event.packet -> data[0];
    if (msgType == static_cast<uint8_t>(MessageType::CLIENT_INPUT) && event.packet->dataLength == sizeof(MsgClientInput)){
        const auto* input = reinterpret_cast<const MsgClientInput*>(event.packet->data);
        
        const int slot = static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));

        players[slot].left = input ->left;
        players[slot].right = input ->right;
        players[slot].up = input -> up;
        players[slot].down = input -> down;
        players[slot].mouse_x = input->mouse_mapX;
        players[slot].mouse_y = input ->mouse_mapY;
        players[slot].shooting = input ->shooting;
    
    }

    enet_packet_destroy(event.packet);
}

static void HandleDisconnect(ENetEvent& event, ServerPlayer* players, ENetHost* server){
    const int slot = static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
    std::printf("Player %d Disconnected\n", slot);
    std::memset(&players[slot], 0, sizeof(ServerPlayer));

    MsgPlayerEvent leave {
        .type = static_cast<uint8_t>(MessageType::PLAYER_LEFT),
        .id = static_cast<uint8_t>(slot),
    };

    ENetPacket* pkt = enet_packet_create(&leave, sizeof(leave), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, 0, pkt);

}

int main(void) {
    if (enet_initialize() != 0) {
        std::fprintf(stderr, "ENet init failed\n");
        return 1;
    }

    std::atexit(enet_deinitialize);

    // Bind to all interfaces on port 7777
    ENetAddress address = { .host = ENET_HOST_ANY, .port = 7777 };

    // enet_host_create(address, maxClients, channels, inBandwidth, outBandwidth)
    // 0 bandwidth = unlimited
    ENetHost *server = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);
    if (!server) {
        std::fprintf(stderr, "Failed to create server host\n");
        return 1;
    }
    std::printf("Server listening on port 7777\n");

    ServerPlayer players[MAX_PLAYERS] {};
    NetProjectile projectiles[MAX_PROJECTILES] {};

    // Fixed timestep: server ticks at 60hz regardless of enet_host_service timing
    constexpr float TICK_RATE = 1.0f / 60.0f;
    double      lastTime  = 0.0;  // we'll use a simple counter approach

    // For server timing we use enet_time (milliseconds since enet_initialize)
    uint32_t lastTick = enet_time_get();

    while (1) {
        // enet_host_service(host, event, timeout_ms)
        // timeout=0 means non-blocking: returns immediately if nothing queued.
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {

            case ENET_EVENT_TYPE_CONNECT:
                // Find an empty player slot
               HandleConnect(event,players,server);
               break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceive(event, players);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                HandleDisconnect(event,players,server);
                break;
            default: 
                break;
            }
        }

        // --- Fixed timestep tick ---
        const uint32_t now   = enet_time_get();
        const uint32_t delta = now - lastTick;

        if (delta >= static_cast<uint32_t>(TICK_RATE * 1000.0f)) {
            const float dt = delta / 1000.0f;
            lastTick = now;

            for (int i = 0; i < MAX_PLAYERS; i++) {
                if(!players[i].net.active) continue;
                if (players[i].shooting) {
                    ServerSpawnProjectile(projectiles, players[i].net.id, players[i].net.x, players[i].net.y, players[i].mouse_x, players[i].mouse_y);
                    players[i].shooting = 0; // consume the shoot event
                }
                ServerUpdatePlayer(players[i], dt);
            }
            ServerUpdateProjectiles(projectiles, players, dt);
            BroadcastGameState(server, players, projectiles);
        }

        // Tiny sleep to avoid burning 100% CPU in the polling loop.
        // enet_host_service with timeout=1 would also work.
       // enet_host_service(server, &event, 1);
    }

    enet_host_destroy(server);
    return 0;
}

