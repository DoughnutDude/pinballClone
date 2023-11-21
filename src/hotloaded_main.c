#include "shared.h"

global_var RayCollision mouseCollision = {0};

inline Matrix GetBoardTransform(GameState * gameState)
{
    Matrix translation = MatrixTranslate(gameState->boardPos.x, gameState->boardPos.y, gameState->boardPos.z);
    Matrix rotation = MatrixRotate(gameState->boardAxis, gameState->boardAngle*DEG2RAD);
    Matrix boardTransform = MatrixMultiply(rotation, translation);

    return boardTransform;
}

internal Obstacle MakeObstacle(GameState * gameState, Vector3 pos, Vector3 size, ObstacleType type, Color color)
{
    Obstacle result = { 0 };
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

internal Ball MakeBall(GameState * gameState, Vector3 pos, float radius, Color color)
{
    Ball result = { 0 };
    result.momentum = { 0 };
    result.pos = pos;
    result.velocity = { 0 };
    result.mass = 1.0f;
    result.inverseMass = 1.0f;
    result.radius = radius;
    result.mesh = GenMeshSphere(result.radius, 8, 8);
    result.color = color;
    result.collisionRay = { 0 };

    gameState->balls[gameState->ballCount] = result;
    ++gameState->ballCount;
    return result;
}

internal Vector3 MoveBall(GameState * gameState, Ball * ball, float dt, Vector3 force)
{
    Vector3 oldPos = ball->pos;
    Vector3 accel = force * ball->inverseMass;
    //accel.y -= 9.807f; // Gravity
    //accel -= 2.0f * ball->velocity; // Replace -2.0f with some better friction coefficient


    Vector3 playerDelta = (0.5f * accel * square(dt)) + (ball->velocity * dt);
    //ball->momentum = ball->momentum + force * dt;
    //ball->velocity = ball->momentum * ball->inverseMass;
    ball->velocity = accel * dt + ball->velocity;
    //Vector3 newPlayerPos = oldPos + playerDelta;
    for (int collisionIterator = 0; collisionIterator < 4; ++collisionIterator)
    {
        float tMin = 1.0f;
        Vector3 desiredPos = ball->pos + playerDelta;

		float len = Vector3Length(playerDelta) + ball->radius;

        Ray ray = { 0 };
        ray.position = ball->pos;
		ray.direction = playerDelta;
        ball->desiredRay = ray;

        int hitObstacleIndex = 0;
        RayCollision collision = { 0 };
		collision.distance = FLOAT_MAX;
        for (int obstacleIndex = 0; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
        {
			Matrix collidingTransform = { 0 };
			Vector3 collidingPos = { 0 };
            Mesh collidingMesh = { 0 };
            if (obstacleIndex == 0)
            {
                collidingMesh = gameState->boardMesh;
				collidingTransform = GetBoardTransform(gameState);
			}
            else
            {
                collidingMesh = gameState->obstacles[obstacleIndex].mesh;
				collidingPos = gameState->obstacles[obstacleIndex].pos;
				Matrix translation = MatrixTranslate(collidingPos.x, collidingPos.y, collidingPos.z);
				Matrix rotation = MatrixRotate(gameState->boardAxis, gameState->boardAngle * DEG2RAD);
				collidingTransform = MatrixMultiply(rotation, translation);
            }
			// Still getting some collision fails. Maybe try some sort of cast from center to center instead of only the playerDelta ray?
            RayCollision testCollision = GetRayCollisionMesh(ray, collidingMesh, collidingTransform);
			Vector3 sub = ray.position - testCollision.point;
			float subLen = Vector3Length(sub);
			testCollision.distance = subLen;
			if (ball->pos.y > 100.0f || ball->pos.y < -100.0f) {
				int x = 0;
			}
			if (testCollision.hit && (len > testCollision.distance) && (testCollision.distance < collision.distance) && (len != 0.0f))
            {
                float tResult = testCollision.distance / len;
                Vector3 partialNewPos = oldPos + tResult * (playerDelta);
                if ((tResult >= 0.0f) && (tMin > tResult))
                {
                    tMin = (tResult - EPSILON > 0.0f) ? (tResult - EPSILON) : 0.0f;
                    hitObstacleIndex = obstacleIndex;
                }
				collision = testCollision;
            }
        }

        //ball->pos = ball->pos + ball->velocity * dt;
        ball->pos += tMin * playerDelta;
        if (collision.hit && (len > collision.distance))
        {
            //collision.normal *= 1.0f;
            float bounceMultip = (hitObstacleIndex == 0) ? 1.0f : 2.0f;
            ball->collisionRay = { collision.point,  collision.normal + collision.point };
            //collision.normal = Vector3Normalize(collision.normal);
            //collision.normal = Vector3Add(collision.point, collision.normal);
            //Vector3 collisionNormalRel = collision.normal - collision.point;
            ball->velocity = ball->velocity - bounceMultip*Vector3Project(ball->velocity, collision.normal); //1.0f*Vector3DotProduct(ball->velocity, collision.normal)*collision.normal;
            playerDelta = desiredPos - ball->pos;
            playerDelta = playerDelta - bounceMultip*Vector3Project(playerDelta, collision.normal); //1.0f * Vector3DotProduct(playerDelta, collision.normal)*collision.normal;
        }
    }
    return force;
}

// Called on every frame
void Update(GameState *gameState)
{
    UpdateMusicStream(gameState->music);

	Ray mouseRay = GetMouseRay(GetMousePosition(), gameState->camera);
	mouseCollision = GetRayCollisionMesh(mouseRay, gameState->boardMesh, GetBoardTransform(gameState));
    if (IsKeyPressed(KEY_SPACE))
    {
		if (mouseCollision.hit)
		{
			PlaySound(gameState->fxCoin);
			Vector3 spawnPoint = mouseCollision.point;
			spawnPoint.y += 8.0f;
			MakeBall(gameState, spawnPoint, 0.5f, BEIGE);
		}
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
    gameState->cameraDist -= (float)GetMouseWheelMove()*0.5f;
    gameState->camera.position.x = gameState->cameraDist*cos(gameState->angleOffset);
    gameState->camera.position.z = gameState->cameraDist*sin(gameState->angleOffset);
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
    Vector3 accel[256] = { 0 };
    for (int ballIndex = 1; ballIndex < gameState->ballCount; ++ballIndex)
    {
        Ball * ball = &gameState->balls[ballIndex];
        Vector3 gravity = Vector3{ 0,-9.8f,0 } * ball->mass;
        accel[ballIndex] = MoveBall(gameState, ball, gameState->dt, gravity);
    }

    BeginDrawing();
    {
        BeginMode3D(gameState->camera);
		{
			ClearBackground(DARKGRAY);
			Model model = LoadModelFromMesh(gameState->boardMesh);
			//DrawModelEx(model, gameState->boardPos, gameState->boardAxis, gameState->boardAngle, {1.0f,1.0f,1.0f}, DARKBROWN);
			DrawModelWiresEx(model, gameState->boardPos, gameState->boardAxis, gameState->boardAngle, { 1.0f,1.0f,1.0f }, BROWN);
			//DrawModelWires(model, { 0 }, 1.0f, DARKBROWN);
			// Draw osbtacles
			for (int obstacleIndex = 1; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
			{
				Obstacle obstacle = gameState->obstacles[obstacleIndex];
				Model model = LoadModelFromMesh(obstacle.mesh);
				//DrawModelEx(model, obstacle.pos, gameState->boardAxis, gameState->boardAngle, { 1.0f,1.0f,1.0f }, obstacle.color);
				DrawModelWiresEx(model, obstacle.pos, gameState->boardAxis, gameState->boardAngle, { 1.0f,1.0f,1.0f }, PURPLE);
				//DrawModelWires(model, obstacle.oldPos, 1.0f, BLUE);
			}
			// Draw balls
			for (int ballIndex = 1; ballIndex < gameState->ballCount; ++ballIndex)
			{
				Ball ball = gameState->balls[ballIndex];
				Model sphere = LoadModelFromMesh(ball.mesh);
				//DrawSphere(ball.pos, ball.radius, ball.color);
				DrawModelWiresEx(sphere, ball.pos, {}, {}, { 1,1,1 }, ball.color);
				DrawLine3D(ball.collisionRay.position, ball.collisionRay.direction * 1.0f, RED);
				//DrawLine3D(ball.pos, ball.velocity+ball.pos, GREEN);
				accel[ballIndex] += ball.pos;
				//DrawLine3D(ball.pos, accel[ballIndex], BLUE);
				DrawLine3D(ball.desiredRay.position, ball.desiredRay.direction + ball.desiredRay.position, WHITE);
			}
			//DrawGrid(50, 1);
			if (mouseCollision.hit)
			{
				DrawCubeWires(mouseCollision.point, 0.3f, 0.3f, 0.3f, BEIGE);

				Vector3 normalEnd;
				normalEnd.x = mouseCollision.point.x + mouseCollision.normal.x;
				normalEnd.y = mouseCollision.point.y + mouseCollision.normal.y;
				normalEnd.z = mouseCollision.point.z + mouseCollision.normal.z;

				DrawLine3D(mouseCollision.point, normalEnd, BEIGE);
			}
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
    gameState->cameraDist = 36.0f;
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
