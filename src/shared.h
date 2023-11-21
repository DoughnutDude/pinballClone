#include "raylib/raylib.h"
#include "raylib/raymath.h"

#define local_persist static
#define global_var	  static
#define internal      static

//----------------------------------------------------------------------------------
// Macros
//----------------------------------------------------------------------------------
#define ARRAYCOUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
//#define assert(expression) if(!(expression)) {*(int *) 0 = 0;}
#define ASSERT(expression)

#define FLOAT_MAX 340282346638528859811704183484516925440.0f

typedef enum ObstacleType
{
    OBSTACLE_TYPE_NULL    , //= 0x0,
    OBSTACLE_TYPE_CUBE    , //= 0x1,
    OBSTACLE_TYPE_CYLINDER, //= 0x2,
} ObstacleType;

typedef struct Ball
{
    Vector3 momentum;
    Vector3 pos;
    Vector3 velocity;
    float mass;
    float inverseMass;

    float radius;
    Mesh mesh;
    Color color;

    Ray collisionRay;
    Ray desiredRay;
} Ball;

typedef struct Obstacle
{
    ObstacleType type;
    Vector3 pos;
    int elasticity;

    Vector3 size;
    Mesh mesh;
    Color color;
} Obstacle;

typedef struct GameState
{
    int screenWidth;
    int screenHeight;
    float dt;

    float angleOffset;
    float cameraDist;
    Camera3D camera;
    Font font;
    Music music;
    Sound fxCoin;

    float boardLength;
    float boardHeight;
    float boardWidth;
    float boardAngle;
    Vector3 boardPos;
    Vector3 boardAxis;
    Mesh boardMesh;
    Material matDefault;

    int obstacleCount;
    Obstacle obstacles[256];
    int ballCount;
    Ball balls[256];
} GameState;


internal GameState operator+(GameState a, GameState b)
{
    GameState result = a;
    for (int obstacleIndex = 1; obstacleIndex < ARRAYCOUNT(result.obstacles); ++obstacleIndex)
    {
        //result.obstacles[obstacleIndex] = a.obstacles[obstacleIndex] + b.obstacles[obstacleIndex];
        int y = 0;
    }
    for (int ballIndex = 1; ballIndex < ARRAYCOUNT(result.balls); ++ballIndex)
    {
        int x = 0;
    }
    return result;
}
internal GameState operator*(GameState a, double b)
{
    GameState result = {};
    return result;
}


//----------------------------------------------------------------------------------
// Math Operations
//----------------------------------------------------------------------------------
inline float square(float a) {
    float result;
    result = a * a;
    return result;
}

//----------------------------------------------------------------------------------
// Vector Operations
//----------------------------------------------------------------------------------
// Vector2
inline Vector2 operator+(Vector2 a, Vector2 b)
{
    Vector2 result = {};
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}
inline Vector2 &operator+=(Vector2 &a, Vector2 b)
{
    a = a + b;
    return a;
}
inline Vector2 operator-(Vector2 a, Vector2 b)
{
    Vector2 result = {};
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}
inline Vector2 &operator-=(Vector2 &a, Vector2 b)
{
    a = a - b;
    return a;
}
inline Vector2 operator*(float a, Vector2 b)
{
    Vector2 result = {};
    result.x = a * b.x;
    result.y = a * b.y;
    return result;
}
inline Vector2 operator*(Vector2 a, float b)
{
    Vector2 result = b * a;
    return result;
}
inline Vector2 &operator*=(Vector2 &a, float b)
{
    a = a * b;
    return a;
}

// Vector3
inline Vector3 operator+(Vector3 a, Vector3 b)
{
    Vector3 result = {};
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}
inline Vector3 &operator+=(Vector3 &a, Vector3 b)
{
    a = a + b;
    return a;
}
inline Vector3 operator-(Vector3 a, Vector3 b)
{
    Vector3 result = {};
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}
inline Vector3 &operator-=(Vector3 &a, Vector3 b)
{
    a = a - b;
    return a;
}
inline Vector3 operator*(float a, Vector3 b)
{
    Vector3 result = {};
    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;
    return result;
}
inline Vector3 operator*(Vector3 a, float b)
{
    Vector3 result = b * a;
    return result;
}
inline Vector3 &operator*=(Vector3 &a, float b)
{
    a = a * b;
    return a;
}


//----------------------------------------------------------------------------------
// Color Operations
//----------------------------------------------------------------------------------
inline Color operator-(Color a, Color b)
{
    Color result = {};
    result.r = a.r - b.r;
    result.g = a.g - b.g;
    result.b = a.b - b.b;
    return result;
}
inline Color &operator-=(Color &a, Color b)
{
    a = a - b;
    return a;
}