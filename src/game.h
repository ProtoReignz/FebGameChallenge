#pragma once

#include "raylib.h"
#include <enet/enet.h>
#include <stdint.h>


#define MAX_PLAYERS 4

#define PLAYER_SPD 200.0f
#define PLAYER_WIDTH 32.0f
#define PLAYER_HEIGHT 32.0f

#define MAX_PROJECTILES 100
#define PROJECTILES_SPD 400.0f
#define PROJECTILES_RADIUS 5.0f

typedef enum MessageType{ // WHAT TYPE OF PACKET IS BEING SENT?
    MSG_CLIENT_INPUT = 0, // CLIENT->SERVER
    MSG_GAME_STATE = 1,  // SERVER->CLIENT GAME STATE
    MSG_PLAYER_JOINED = 2, // SERVER -> CLIENTS SELF EXPLANATORY
    MSG_PLAYER_LEFT = 3,
} MessageType;

//Game state structure
//sent every frame

typedef struct NetPlayer{
    uint8_t id; //who are you
    uint8_t active; //are you the one that's doing something
    float x, y; // world position
    float width, height;
} NetPlayer;

typedef struct NetProjectile {
    uint8_t active; 
    float x, y; // position
    float vx, vy; //velocity
    float radius;
} NetProjectile;


//Packet structure

//Client sends this every frame
typedef struct MsgClientInput{
    uint8_t type; //client input
    uint8_t left, right, up, down; //keypress and other input
    float mouse_mapX;
    float mouse_mapY;
    uint8_t shooting; //1 if attack is pressed
} MsgClientInput;

typedef struct MsgPlayerLink{
    uint8_t type;
    uint8_t id;
} MsgPlayerEvent;

typedef struct MsgGameState {
    uint8_t        type;    // MSG_GAME_STATE
    uint8_t        your_id; // which player slot
    NetPlayer      players[MAX_PLAYERS];
    NetProjectile  projectiles[MAX_PROJECTILES];
} MsgGameState;

//SHARED BETWEEN CLIENT AND SERVER
//MAP ENVIRONMENT CLASSIFICATION

typedef struct EnvItem {
    Rectangle rect;
    int blocking;
    Color color;
} EnvItem;



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
