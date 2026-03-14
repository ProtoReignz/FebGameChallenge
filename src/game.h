#pragma once

#include "raylib.h"
#include <enet/enet.h>
#include <stdint.h>

//Constants

constexpr int MAX_PLAYERS = 4;

constexpr float PLAYER_SPD = 200.0f;
constexpr float PLAYER_WIDTH = 32.0f;
constexpr float PLAYER_HEIGHT = 32.0f;

constexpr int MAX_PROJECTILES = 100;
constexpr float PROJECTILES_SPD = 400.0f;
constexpr float PROJECTILES_RADIUS = 5.0f;

enum class MessageType : uint8_t{ // WHAT TYPE OF PACKET IS BEING SENT?
    CLIENT_INPUT = 0, // CLIENT->SERVER
    GAME_STATE = 1,  // SERVER->CLIENT GAME STATE
    PLAYER_JOINED = 2, // SERVER -> CLIENTS SELF EXPLANATORY
    PLAYER_LEFT = 3,
};

//Game state structure
//sent every frame

struct NetPlayer{
    uint8_t id {}; //who are you
    uint8_t active {}; //are you the one that's doing something
    float x {};
    float y {}; // world position
    float width {};
    float height {};
};

struct NetProjectile {
    uint8_t active {}; 
    float x {};
    float y {}; // position
    float vx {};
    float vy {}; //velocity
    float radius {};
};


//Packet structure

//Client sends this every frame
struct MsgClientInput{
    uint8_t type {}; //client input
    uint8_t left {};
    uint8_t right {};
    uint8_t up {};
    uint8_t down {}; //keypress and other input
    float mouse_mapX {};
    float mouse_mapY {};
    uint8_t shooting {}; //1 if attack is pressed
};

struct MsgPlayerEvent{

    uint8_t type {};
    uint8_t id {};
};

struct MsgGameState {

    uint8_t type {};    // MSG_GAME_STATE
    uint8_t your_id {}; // which player slot
    NetPlayer players[MAX_PLAYERS] {};
    NetProjectile projectiles[MAX_PROJECTILES] {};
};


//SHARED BETWEEN CLIENT AND SERVER
//MAP ENVIRONMENT CLASSIFICATION

struct EnvItem {
    Rectangle rect {};
    int blocking {};
    Color color {};
};



#ifndef ENV_ITEMS_IMPL

    extern EnvItem g_envItems[];
    extern int g_envItemsCount;
#else

    EnvItem g_envItems[] = {
        {{0,0,1000,400}, 0, LIGHTGRAY },
        {{0,400,1000,400}, 1, GRAY},
        {{300,200,400,10}, 1, GRAY},
        {{250,300,100,10}, 1, GRAY},
        {{650,300,100,10}, 1, GRAY},
    };

    int g_envItemsCount = sizeof(g_envItems) / sizeof(g_envItems[0]);
#endif
