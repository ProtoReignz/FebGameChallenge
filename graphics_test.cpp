#include "raylib.h"
#include "raymath.h"



#define PLAYER_SPD 200.0f

#define MAX_PROJECTILES 100
#define PROJECTILES_SPD 400.0f
#define PROJECTILES_RADIUS 5.0f




typedef struct Player {
    Vector2 position;
    float speed;
} Player;

typedef struct EnvItem {
    Rectangle rect;
    int blocking;
    Color color;
} EnvItem;

typedef struct Projectile{
    Vector2 position;
    Vector2 velocity;
    float radius;
    bool active;
}Projectile;

//Player and Camera
void UpdatePlayer(Player *player, EnvItem *envItems, int envItemsLength, float delta);
void UpdateCameraCenter(Camera2D *camera, Player *player, EnvItem *envItems, int envItemsLength, float delta, int width, int height);

//Weapons
void Attack(Player *player, Vector2 mousePosition, Projectile *projectiles, int maxProjectiles); //add mouse direction, weapon type etc.
void UpdateProjectiles(Projectile *projectiles, int maxProjectiles, float delta);
void DrawProjectiles(Projectile *projectiles, int maxProjectiles);



int main(void){
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Mine n' Mayhem");

    Player player = {0};
    player.position = (Vector2){400, 280};
    player.speed = 0;
    
    Projectile projectiles[MAX_PROJECTILES] = {0};
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = false;
    }


    EnvItem envItems[] = {
        {{ 0, 0, 1000, 400 }, 0, LIGHTGRAY },
        {{ 0, 400, 1000, 200 }, 1, GRAY },
        {{ 300, 200, 400, 10 }, 1, GRAY },
        {{ 250, 300, 100, 10 }, 1, GRAY },
        {{ 650, 300, 100, 10 }, 1, GRAY }
    };

    int envItemsLength = sizeof(envItems)/sizeof(envItems[0]);

    Camera2D camera = { 0 };
    // camera.offset.x = CENTERX;   
    // camera.offset.y = CENTERY;   

    // Remake these with fixed variables
    camera.target = player.position;
    camera.offset = (Vector2){ screenWidth/2.0f, screenHeight/2.0f};
    
    // camera.target.y = 0;         
    camera.rotation = 0.0f;        
    camera.zoom     = 1.0f;

    SetTargetFPS(60);

    while(!WindowShouldClose()){
        float deltaTime = GetFrameTime();
        Vector2 mouse = GetMousePosition();
        Vector2 mousePos = GetScreenToWorld2D(mouse, camera);



        UpdatePlayer(&player, envItems, envItemsLength, deltaTime);
        UpdateProjectiles(projectiles, MAX_PROJECTILES, deltaTime);
        UpdateCameraCenter(&camera, &player, envItems, envItemsLength, deltaTime, screenWidth, screenHeight);
        
        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode2D(camera);

                for (int i = 0; i < envItemsLength; i++) DrawRectangleRec(envItems[i].rect, envItems[i].color);

                Rectangle playerRect = { player.position.x - 20, player.position.y - 40, 40.0f, 40.0f };
                DrawRectangleRec(playerRect, RED);

                DrawCircleV(player.position, 5.0f, GOLD);

                DrawProjectiles(projectiles, MAX_PROJECTILES);

            EndMode2D();

            DrawText("Send Help", 0, 20, 15, BLACK);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                Attack(&player, mousePos, projectiles, MAX_PROJECTILES);
            }

        EndDrawing();
    }


    CloseWindow();
    return 0;
}

void UpdatePlayer(Player *player, EnvItem *envItems, int envItemsLength, float delta){
    if (IsKeyDown(KEY_LEFT)) player->position.x -= PLAYER_SPD*delta;
    if (IsKeyDown(KEY_RIGHT)) player->position.x += PLAYER_SPD*delta;
    if (IsKeyDown(KEY_DOWN)) player->position.y += PLAYER_SPD*delta;
    if (IsKeyDown(KEY_UP)) player->position.y -= PLAYER_SPD*delta;

    if (IsKeyDown(KEY_A)) player->position.x -= PLAYER_SPD*delta;
    if (IsKeyDown(KEY_D)) player->position.x += PLAYER_SPD*delta;
    if (IsKeyDown(KEY_S)) player->position.y += PLAYER_SPD*delta;
    if (IsKeyDown(KEY_W)) player->position.y -= PLAYER_SPD*delta;



    bool hitObstacle = false;
    for (int i = 0; i < envItemsLength; i++)
    {
        EnvItem *ei = envItems + i;
        Vector2 *p = &(player->position);
        if (ei->blocking &&
            ei->rect.x <= p->x &&
            ei->rect.x + ei->rect.width >= p->x &&
            ei->rect.y >= p->y &&
            ei->rect.y <= p->y + player->speed*delta)
        {
            hitObstacle = true;
            player->speed = 0.0f;
            p->y = ei->rect.y;
            break;
        }
    }

}

void UpdateCameraCenter(Camera2D *camera, Player *player, EnvItem *envItems, int envItemsLength, float delta, int width, int height) {
    // Simple center following
    camera->target = player->position;
}

void Attack(Player *player, Vector2 mousePosition, Projectile *projectiles, int maxProjectiles){
    for (int i = 0; i < maxProjectiles; i++){
        if(!projectiles[i].active){
            projectiles[i].active = true;
            projectiles[i].position = player ->position;
            projectiles[i].radius = PROJECTILES_RADIUS;
            
            Vector2 direction = Vector2Subtract(mousePosition, player->position);

            // Normalizes direction to keep speed normal
            direction = Vector2Normalize(direction);

            projectiles[i].velocity = Vector2Scale(direction, PROJECTILES_SPD);

            break;
        }
    }
}

void UpdateProjectiles(Projectile *projectiles, int maxProjectiles, float delta){
    for (int i = 0; i < maxProjectiles; i++){
        if (projectiles[i].active){
            projectiles[i].position.x += projectiles[i].velocity.x * delta;
            projectiles[i].position.y += projectiles[i].velocity.y * delta;

            if (projectiles[i].position.x < -100 || projectiles[i].position.x > 1100 || projectiles[i].position.y < -100 || projectiles[i].position.y > 700) {
                    projectiles[i].active = false;
            }
        }
    }
}

void DrawProjectiles(Projectile *projectiles, int maxProjectiles){
    for (int i = 0; i<maxProjectiles; i++){
        if(projectiles[i].active){
            DrawCircleV(projectiles[i].position, projectiles[i].radius, RED);
        }
    }
}