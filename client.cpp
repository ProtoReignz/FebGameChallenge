//client.cpp

#define ENV_ITEMS_IMPL
#include "game.h"

static_assert(sizeof(MsgGameState) < 16000, "Packet too big");

#include "raymath.h"

#include <cstdio>
#include <cstring>
#include <cstdbool>


//Wrappers that allow the Enet and Raylib windows to be cleaned up even outside of the scope they were init in
struct ENetGuard{
    ENetGuard() {
        if (enet_initialize() != 0){
            std::fprintf(stderr, "ENet init failed\n");
            std::exit(1);
        }
    }

    ~ENetGuard() { enet_deinitialize();}


    //find out what these do in more detail
    ENetGuard(const ENetGuard&) =delete;
    ENetGuard& operator=(const ENetGuard&) =delete;
};

struct WindowGuard{
    WindowGuard(int w, int h, const char* title){
        InitWindow(w, h, title);
        SetTargetFPS(60);
    }

    ~WindowGuard() { CloseWindow(); }

    WindowGuard(const WindowGuard&) =delete;
    WindowGuard& operator=(const WindowGuard&) =delete;
};

//ClientState --> holds the info that the server holds at any given moment
// keeping it a struct allows fields to be added much easier without DRASTICALLY changing the loop logic

struct ClientState{
    NetPlayer players[MAX_PLAYERS] {};
    NetProjectile projectiles[MAX_PROJECTILES] {};

    uint8_t myID {255}; //ID for the current client; 255 is unassigned
    bool connected {false};
    bool gotFirstState {false};
};


//Clientside Functions

//Polling the server for gamestate changes

static bool PollNetwork(ENetHost* host, ClientState& state){
    ENetEvent event;
    while (enet_host_service(host, &event, 0)>0){
        switch(event.type){
            case ENET_EVENT_TYPE_RECEIVE: {
                //stop if its a bad packet
                
                if (event.packet -> dataLength <1) {
                    enet_packet_destroy(event.packet);
                    break;
                }

                const uint8_t msgType = event.packet -> data[0];

                std::printf("Packet received, size=%zu, type=%d\n", event.packet->dataLength, event.packet->data[0]);
                if (msgType == static_cast<uint8_t>(MessageType::GAME_STATE) && event.packet->dataLength == sizeof(MsgGameState)){
                    const auto* msg = reinterpret_cast<const MsgGameState*>(event.packet->data);
                    
                    state.myID = msg->your_id;
                    
                    std::memcpy(state.players, msg->players, sizeof(state.players));
                    std::memcpy(state.projectiles, msg->projectiles, sizeof(state.projectiles));

                    state.gotFirstState = true;
                                  

                }
                
                enet_packet_destroy(event.packet);
                break; 
            }

            case ENET_EVENT_TYPE_DISCONNECT:
                std::printf("Disconnected from server\n");
                return false;
            
            default:
                break;
        
        }
    }
    return true;
}

//collect the current frame's state to send to the server
    //Taking camera as const-ref means the signals won't be modified
static void SendInput(ENetPeer* server, const ClientState& state, const Camera2D& camera){
    const Vector2 mouse = GetMousePosition();
    const Vector2 mouseWorld = GetScreenToWorld2D(mouse, camera);

    MsgClientInput input{
        .type = static_cast<uint8_t>(MessageType::CLIENT_INPUT),
        .left       = static_cast<uint8_t>(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
        .right      = static_cast<uint8_t>(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)),
        .up         = static_cast<uint8_t>(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)),
        .down       = static_cast<uint8_t>(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)),
        .mouse_mapX = mouseWorld.x,
        .mouse_mapY = mouseWorld.y,
        .shooting   = static_cast<uint8_t>(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)),
    }; //static cast as the narrowing is explicit for the compiler, thanks claude ;D

    ENetPacket* pkt = enet_packet_create(&input, sizeof(input), ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(server, 1, pkt);
}

//Camera Updating
//if no updates --> center on map

static void UpdateCamera(Camera2D& camera, const ClientState& state){
    if (state.gotFirstState && state.myID<MAX_PLAYERS && state.players[state.myID].active){
        const NetPlayer& me = state.players[state.myID];
        camera.target = {me.x, me.y};

    } else {
        camera.target = {500.0f, 200.0f};
    }

}

//Draw the items on screen
// Environment -> Players -> Projectiles -> Text
static void Render(const ClientState& state, const Camera2D& camera){
    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode2D(camera);

    //Draw environment
    for (int i = 0; i < g_envItemsCount; i++) {
        DrawRectangleRec(g_envItems[i].rect, g_envItems[i].color);
    }


    //Draw Players if they are active
    for (int i = 0; i < MAX_PLAYERS; i++){
        if (!state.players[i].active) continue;

        const NetPlayer& p = state.players[i];
        const Color col = (i == state.myID) ? RED:BLUE;

        const Rectangle rect {
            p.x - p.width /2.0f,
            p.y - p.height /2.0f,
            p.width,
            p.height
        };
        DrawRectangleRec(rect, col);
        DrawCircleV({p.x, p.y}, 5.0f, GOLD);
    }

    //Draw projectiles
    for (int i = 0; i<MAX_PROJECTILES;i++){
        if(!state.projectiles[i].active) continue;
        const NetProjectile& pr = state.projectiles[i];
        DrawCircleV({pr.x, pr.y}, pr.radius, YELLOW);
    }

    EndMode2D();

    if(!state.connected){
        DrawText("DISCONNECTED", 10, 10, 20, RED);
    } else if (!state.gotFirstState){
        DrawText("Waiting..", 10,10,20,WHITE);
    } else {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Player ID: %d", state.myID);
        DrawText(buf, 10,10,15,WHITE);
    }

    EndDrawing();

}

static void Disconnect(ENetHost* host, ENetPeer* server){
    enet_peer_disconnect(server, 0);

    ENetEvent event;
    while (enet_host_service(host, &event, 2000)>0){
        if(event.type == ENET_EVENT_TYPE_RECEIVE){
            enet_packet_destroy(event.packet);
        }else if (event.type == ENET_EVENT_TYPE_DISCONNECT){
            break;
        }
    }
}

int main(int argc, char **argv){
    const char* serverIP = (argc > 1) ? argv[1] : "127.0.0.1";

    std::printf("sizeof(MsgGameState)    = %zu\n", sizeof(MsgGameState));
    std::printf("sizeof(MsgClientInput)  = %zu\n", sizeof(MsgClientInput));
    std::printf("sizeof(NetPlayer)       = %zu\n", sizeof(NetPlayer));
    std::printf("sizeof(NetProjectile)   = %zu\n", sizeof(NetProjectile));
        
    ENetGuard enetGuard;
    WindowGuard windowGuard (800, 450, "Mine n' Mayhem");

    ENetHost* host = enet_host_create(nullptr,1,2,0,0);
    if(!host){
        std::fprintf(stderr, "Failed to create client host\n");
        return 1;

    }

    ENetAddress serverAddr {};
    enet_address_set_host(&serverAddr, serverIP);
    serverAddr.port = 7777;

    ENetPeer* serverPeer = enet_host_connect(host, &serverAddr, 2, 0);
     if (!serverPeer) {
        std::fprintf(stderr, "No available peers\n");
        enet_host_destroy(host);
        return 1;
    }

    ENetEvent event {};
    if (enet_host_service(host, &event, 5000)<=0||event.type != ENET_EVENT_TYPE_CONNECT){
        std::fprintf(stderr, "Could not connect to %s:7777\n", serverIP);
        enet_peer_reset(serverPeer);
        enet_host_destroy(host);
        return 1;
    }
    std::printf("Connected to %s:7777\n",serverIP);

    ClientState state {};
    state.connected = true;

    Camera2D camera {};
    camera.offset = {800.0f/2.0f, 450.0f/2.0f};
    camera.zoom = 1.0f;

    while(!WindowShouldClose()){
        state.connected = PollNetwork(host, state);
        UpdateCamera(camera, state);

        if (state.connected){
            SendInput(serverPeer, state, camera);
        }

        Render(state, camera);
    }

    Disconnect(host, serverPeer);
    enet_host_destroy(host);
    return 0;
}