#include "shared.h"
//#include "stdio.h"

inline Matrix GetBoardTransform(GameState * gameState)
{
    Matrix translation = MatrixTranslate(gameState->boardPos.x, gameState->boardPos.y, gameState->boardPos.z);
    Matrix rotation = MatrixRotate(gameState->boardAxis, gameState->boardAngle*DEG2RAD);
    Matrix boardTransform = MatrixMultiply(rotation, translation);

    return boardTransform;
}

internal Obstacle MakeObstacle(GameState * gameState, Vector3 pos, Vector3 size, ObstacleType type, Color color) //type
{
    Obstacle result = { 0 };
    result.oldPos = pos;
    result.oldPos.y += gameState->boardHeight/2.0f;
    result.pos = pos;
    result.pos.y += gameState->boardHeight / 2.0f;
    result.size = size;
    switch (type)
    {
        case OBSTACLE_TYPE_CYLINDER:
        {
            float radius = result.size.x;
            float height = result.size.y;
            result.mesh = GenMeshCylinder(radius, height, 12);
            break;
        }
        case OBSTACLE_TYPE_CUBE:
        {
            result.oldPos.y += 0.5f*result.size.y;
            result.pos.y += 0.5f*result.size.y;
            result.mesh = GenMeshCube(result.size.x, result.size.y, result.size.z);
            break;
        }
    }
    // As obstacle is made, automatically rebase from board coords to real world coords.
    result.pos = Vector3Transform(result.pos, GetBoardTransform(gameState));
    result.color = color;
    result.elasticity = 1;

    gameState->obstacles[gameState->obstacleCount] = result;
    ++gameState->obstacleCount;

    return result;
}

internal void DrawObstacle(GameState * gameState, Obstacle obstacle)
{
    Model model = LoadModelFromMesh(obstacle.mesh);
    //DrawModelEx(model, obstacle.pos, gameState->boardAxis, gameState->boardAngle, { 1.0f,1.0f,1.0f }, obstacle.color);
    DrawModelWiresEx(model, obstacle.pos, gameState->boardAxis, gameState->boardAngle, { 1.0f,1.0f,1.0f }, PURPLE);
    DrawModelWires(model, obstacle.oldPos, 1.0f, BLUE);
}

internal Ball MakeBall(GameState * gameState, Vector3 pos, float radius, Color color)
{
    Ball result = { 0 };
    result.pos = pos;
    result.radius = radius;
    result.mesh = GenMeshSphere(result.radius, 12, 12);
    result.color = color;
    result.collisionRay = { 0 };

    gameState->balls[gameState->ballCount] = result;
    ++gameState->ballCount;
    return result;
}

internal void MoveBall(GameState * gameState, Ball * ball, float dt, Vector3 accel)
{
    Vector3 oldPlayerPos = ball->pos;
    accel.y -= 9.807f; // Gravity
    //accel += -2.0f * ball->dPos; // Replace -2.0f with some better friction coefficient

    Vector3 playerDelta = (0.5f * accel * square(dt)) + (ball->dPos * dt);
    ball->dPos = accel * dt + ball->dPos;
    Vector3 newPlayerPos = oldPlayerPos + playerDelta;


    for (int collisionIterator = 0; collisionIterator < 4; ++collisionIterator)
    {
        float tMin = 1.0f;
        Vector3 desiredPos = ball->pos + playerDelta;
#if 0
        int hitObstacleIndex = 0;
        for (int obstacleIndex = 1; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
        {
            Obstacle testObstacle = { 0 };
        }
#endif
        Ray ray = { 0 };
        ray.position = oldPlayerPos;
        ray.direction = playerDelta;
        RayCollision collision = GetRayCollisionMesh(ray, gameState->boardMesh, GetBoardTransform(gameState));

        if (Vector3Length(playerDelta) != 0.0f)
        {
            tMin = collision.distance / Vector3Length(playerDelta);
        }

        ball->pos += tMin * playerDelta;
        if (collision.hit)
        {
            ball->dPos = ball->dPos - Vector3DotProduct(ball->dPos, collision.normal)*collision.normal;
            playerDelta = desiredPos - ball->pos;
            playerDelta = playerDelta - Vector3DotProduct(playerDelta, collision.normal)*collision.normal;
            ball->collisionRay = { collision.point, collision.normal };
        }
    }
}

// Called on every frame
void Update(GameState *gameState)
{
    UpdateMusicStream(gameState->music);
    if (IsKeyPressed(KEY_SPACE))
    {
        PlaySound(gameState->fxCoin);
        MakeBall(gameState, { 1,6.0f,3 }, 1, BEIGE);
    }

    if (IsKeyDown(KEY_W))
    {
        gameState->camera.position.y += 0.2f;
    }
    if (IsKeyDown(KEY_S))
    {
        gameState->camera.position.y -= 0.2f;
    }
    if (IsKeyDown(KEY_A))
    {
        gameState->angleOffset += 0.05f;
    }
    if (IsKeyDown(KEY_D))
    {
        gameState->angleOffset -= 0.05f;
    }
    gameState->camera.position.x = 30.0f*cos(gameState->angleOffset);
    gameState->camera.position.z = 30.0f*sin(gameState->angleOffset);
    //gameState->camera.target = { 0.0f, gameState->camera.position.y, 0.0f };

    if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
    {
        int monitor = GetCurrentMonitor();
        if (IsWindowFullscreen())
        {
            ClearWindowState(FLAG_WINDOW_UNDECORATED);
            ToggleFullscreen();
            SetWindowSize(gameState->screenWidth, gameState->screenHeight);
        } else
        {
            SetWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            ToggleFullscreen();
        }
    }
    for (int ballIndex = 1; ballIndex < gameState->ballCount; ++ballIndex)
    {
        MoveBall(gameState, &gameState->balls[ballIndex], gameState->dt, {0});
    }

    BeginDrawing();
    {
        BeginMode3D(gameState->camera);
        {
            ClearBackground(DARKGRAY);
            Model model = LoadModelFromMesh(gameState->boardMesh);
            DrawModelEx(model, gameState->boardPos, gameState->boardAxis, gameState->boardAngle, {1.0f,1.0f,1.0f}, DARKBROWN);
            DrawModelWiresEx(model, gameState->boardPos, gameState->boardAxis, gameState->boardAngle, {1.0f,1.0f,1.0f}, BROWN);
            DrawModelWires(model, { 0 }, 1.0f, DARKBROWN);
            // Draw osbtacles
            for (int obstacleIndex = 1; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
            {
                DrawObstacle(gameState, gameState->obstacles[obstacleIndex]);
            }
            // Draw balls
            for (int ballIndex = 1; ballIndex < gameState->ballCount; ++ballIndex)
            {
                Ball ball = gameState->balls[ballIndex];
                DrawSphere(ball.pos, ball.radius, ball.color);
                DrawLine3D(ball.collisionRay.position, ball.collisionRay.direction, RED);
            }
            //DrawGrid(50, 1);
        }
        EndMode3D();
    }
    EndDrawing();
}

// Called when you recompile the program while its running
void HotReload(GameState *gameState)
{
    TraceLog(LOG_INFO, "HOTRELOAD");

    gameState->angleOffset = -PI/2.0f;
    gameState->camera = { 0 };
    gameState->camera.position;
    gameState->camera.position = { 0.0f, 24.0f, 0.0f };
    gameState->camera.target = { 0.0f, 0.0f, 0.0f };
    gameState->camera.up = { 0.0f, 1.0f, 0.0f };
    gameState->camera.fovy = 45.0f;
    gameState->camera.projection = CAMERA_PERSPECTIVE;

    gameState->boardLength = 36.0f;
    gameState->boardHeight = 1.0f;
    gameState->boardWidth = 27.0f;
    gameState->boardAngle = 6.5f;
    gameState->boardAxis = { -1.0f,0,0 };
    gameState->boardPos = { 0.0f, gameState->boardLength*(float)sin(gameState->boardAngle*DEG2RAD), 0.0f };
    gameState->boardMesh = GenMeshCube(gameState->boardWidth, gameState->boardHeight, gameState->boardLength);
    gameState->matDefault = LoadMaterialDefault();
    gameState->matDefault.maps[MATERIAL_MAP_DIFFUSE].color = BLUE;

    gameState->ballCount = 0;
    Ball nullBall = MakeBall(gameState, { 0 }, 0, { 0 });
    gameState->obstacleCount = 0;
    Obstacle nullObstace = MakeObstacle(gameState, { 0 }, { 0 }, OBSTACLE_TYPE_NULL, { 0 });

    for (int i = -2; i < 2; ++i)
    {
        Vector3 obstaclePos = { (float)i*4, 0, (float)i*4 };
        Vector3 obstacleSize = { 2,2,2 };
        ObstacleType type = OBSTACLE_TYPE_CYLINDER;
        if (i % 2 == 0)
        {
            type = OBSTACLE_TYPE_CUBE;
        }
        MakeObstacle(gameState, obstaclePos, obstacleSize, type, DARKPURPLE);
    }

    // Load global data (assets that must be available in all screens, i.e. font)
    ChangeDirectory(GetWorkingDirectory());
    //TraceLog(LOG_INFO, GetWorkingDirectory());//debug output
    ChangeDirectory("../src");

    gameState->font = LoadFont("resources/Inconsolata-ExtraBold.ttf");
    //gameState->music = LoadMusicStream("resources/ambient.ogg");
    gameState->fxCoin = LoadSound("resources/coin.wav");

    Image image = LoadImage("resources/WIND.png");
    SetWindowIcon(image);
    //SetWindowOpacity(0.9f);
    ChangeDirectory(GetWorkingDirectory());

    SetMusicVolume(gameState->music, 1.0f);
    PlayMusicStream(gameState->music);
}

// Called before the dynamic libraries get swapped
void HotUnload(GameState *gameState)
{
    TraceLog(LOG_INFO, "HOTUNLOAD");

    StopMusicStream(gameState->music);
}

// Called at the the start of the program
void Initialize(GameState *gameState)
{
    gameState->screenWidth = 1280;
    gameState->screenHeight = 720;
    InitWindow(gameState->screenWidth, gameState->screenHeight, "pinballClone");
    SetExitKey(KEY_KP_0);
    InitAudioDevice(); // Initialize audio device

    HotReload(gameState);

    SetTargetFPS(60);
}
