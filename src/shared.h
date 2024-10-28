#include "raylib/raylib.h"
#include "raylib/raymath.h"
//#include "gjk.h"

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
#define PINBALL_EPSILON .0001f

#define MAX_OBSTACLES 256
#define MAX_BALLS     256

typedef enum ObstacleType
{
    OBSTACLE_TYPE_NULL    , //= 0x0,
    OBSTACLE_TYPE_BOX     , //= 0x1,
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
    char* name;
    ObstacleType type;
    Vector3 pos;
    Vector3 centerOfRot;
    int elasticity;

    Vector3 size;
    Mesh mesh;
    Color color;
} Obstacle;

typedef struct GameState
{
    int screenWidth;
    int screenHeight;
    int frameCount;
    float dt;

    bool controllingCamera = false;
    float angleOffset;
    float cameraDist;
    Camera3D camera;
    Font font;
    Music music;
    Sound fxCoin;

    union
    {
        struct
        {
            float boardWidth;
            float boardHeight;
            float boardLength;
        };
        Vector3 boardDim;
    };
    float boardAngle;
    Vector3 boardPos;
    Vector3 boardAxis;
    Mesh boardMesh;
    Material matDefault;

    int obstacleCount;
    Obstacle obstacles[MAX_OBSTACLES];
    int ballCount;
    Ball balls[MAX_BALLS];
} GameState;

struct VertexGJK
{
    Vector3 i;
    Vector3 j;
    Vector3 minkowski;
};

struct CollisionData
{
    bool hit;
    float distanceSquared;
    Vector3 penetrationVec;
    Vector3 faceCollided[3];
    Vector3 closestPointI;
    Vector3 closestPointJ;
    Vector3 closestFeatureI[4];
    int closestFeatureIvertexCount;
    Vector3 closestFeatureJ[4];
    int closestFeatureJvertexCount;

    VertexGJK vertices[4]; //makes up the simplex
    float barycentricCoords[4];
    int count;
};


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
inline float square(float a)
{
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
inline bool operator==(Vector3 a, Vector3 b)
{
    bool result;
    result = (a.x == b.x) & (a.y == b.y) & (a.z == b.z);
    return result;
}
inline Vector3 Vector3Copy(Vector3 a)
{
    Vector3 result = { 0 };
    result.x = a.x;
    result.y = a.y;
    result.z = a.z;
    return result;
}
inline Vector3 Vector3TripleProduct(Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 result;

    float ac = Vector3DotProduct(a, c);
    float bc = Vector3DotProduct(b, c);
    result = b*ac - a*bc;
    return result;
}

internal Vector3 GetPlaneNormalFromTriangle(Vector3 p1, Vector3 p2, Vector3 p3)
{
    Vector3 edge1 = p2 - p1;
    Vector3 edge2 = p3 - p1;
    Vector3 normal = Vector3CrossProduct(edge1, edge2);

    return normal;
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
    result.a = a.a - b.a;
    return result;
}
inline Color &operator-=(Color &a, Color b)
{
    a = a - b;
    return a;
}
inline Color operator*(Color a, Color b)
{
    Color result = {};
    result.r = a.r * b.r;
    result.g = a.g * b.g;
    result.b = a.b * b.b;
    result.a = a.a * b.a;
    return result;
}
inline Color operator*(float a, Color b)
{
    Color result = {};
    result.r = a*b.r;
    result.g = a*b.g;
    result.b = a*b.b;
    return result;
}
