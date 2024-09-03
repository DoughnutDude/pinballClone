#include "shared.h"

global_var RayCollision mouseCollision = {0};

internal void Draw(GameState * gameState, CollisionData simplex)
{
    BeginDrawing();
    {
        BeginMode3D(gameState->camera);
        {
            ClearBackground(DARKGRAY);
            Model model = LoadModelFromMesh(gameState->boardMesh);
#if 0 //Old DrawBoard code
            DrawModelEx(model, gameState->boardPos, gameState->boardAxis, 0.0f,
                        { 1.0f,1.0f,1.0f }, DARKBROWN);
            DrawModelWiresEx(model, gameState->boardPos, { 0 },
                             0.0f, { 1.0f,1.0f,1.0f }, BROWN);
#endif
            //DrawModelWires(model, { 0 }, 1.0f, DARKBROWN);
            // Draw osbtacles
            for (int obstacleIndex = 1; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
            {
                Obstacle obstacle = gameState->obstacles[obstacleIndex];
                Model model = LoadModelFromMesh(obstacle.mesh);
                Color obstColor = PURPLE;
                obstColor -= obstacleIndex * Color{ 40,40,40,0 };
                DrawModelEx(model, obstacle.pos, { 0,0,0 }, 0, { 1.0f,1.0f,1.0f }, obstacle.color);
                DrawModelWiresEx(model, obstacle.pos, { 0 }, 0.0f,
                                 { 1.0f,1.0f,1.0f }, obstColor);//gameState->boardAxis, gameState->boardAngle, { 1.0f,1.0f,1.0f }, PURPLE);
                //DrawModelWires(model, obstacle.oldPos, 1.0f, BLUE);
            }
            // Draw balls
            for (int ballIndex = 1; ballIndex < gameState->ballCount; ++ballIndex)
            {
                Ball ball = gameState->balls[ballIndex];
                Model sphere = LoadModelFromMesh(ball.mesh);
                DrawSphere(ball.pos, ball.radius, ball.color);
                DrawModelWiresEx(sphere, ball.pos, {}, {}, { 1,1,1 }, ball.color);
                DrawLine3D(ball.collisionRay.position, ball.collisionRay.direction * 1.0f, RED);
                //DrawLine3D(ball.pos, ball.velocity+ball.pos, GREEN);
                //accel[ballIndex] += ball.pos;
                //DrawLine3D(ball.pos, accel[ballIndex], BLUE);
                DrawLine3D(ball.desiredRay.position, ball.desiredRay.direction + ball.desiredRay.position, WHITE);
            }
            //Draw simplex for GJK/EPA
            if (simplex.count > 0)
            {
                DrawLine3D(simplex.vertices[0], simplex.vertices[1], WHITE);
            }
            if (simplex.count >= 3)
            {
                DrawLine3D(simplex.vertices[2], simplex.vertices[1], WHITE);
                DrawLine3D(simplex.vertices[2], simplex.vertices[0], WHITE);
            }
            if (simplex.count == 4)
            {
                DrawLine3D(simplex.vertices[3], simplex.vertices[1], WHITE);
                DrawLine3D(simplex.vertices[3], simplex.vertices[2], WHITE);
                DrawLine3D(simplex.vertices[3], simplex.vertices[0], WHITE);
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
            DrawPoint3D(Vector3Zero(), RED);
        }
        EndMode3D();
    }
    EndDrawing();
}

inline Matrix GetBoardTransform(GameState * gameState)
{
    Matrix translation = MatrixTranslate(gameState->boardPos.x, gameState->boardPos.y, gameState->boardPos.z);
    //Matrix rotation = MatrixRotate(gameState->boardAxis, gameState->boardAngle*DEG2RAD);
    //Matrix boardTransform = MatrixMultiply(rotation, translation);

    return translation;//boardTransform;
}

internal Obstacle MakeObstacle(GameState * gameState, Vector3 posInBoard, Vector3 size, ObstacleType type, Color color)
{
    Obstacle result = { 0 };
    result.pos = posInBoard;
    result.pos.y += gameState->boardHeight / 2.0f;
    result.size = size;
    result.type = type;
    switch (type)
    {
        case OBSTACLE_TYPE_CYLINDER:
        {
            float radius = result.size.x;
            float height = result.size.y;
            result.mesh = GenMeshCylinder(radius, height, 12);
            break;
        }
        case OBSTACLE_TYPE_BOX:
        {
            result.pos.y += 0.5f*result.size.y;
            result.mesh = GenMeshCube(result.size.x, result.size.y, result.size.z);
            break;
        }
    }
    // As obstacle is made, automatically rebase from board coords to real world coords.
    //result.pos = Vector3Transform(result.pos, GetBoardTransform(gameState));
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
    //TEMPORARY: Remove after testing
    //result.mesh = GenMeshCube(radius, radius, radius);
    result.mesh = GenMeshSphere(result.radius, 8, 8);
    result.color = color;
    result.collisionRay = { 0 };

    gameState->balls[gameState->ballCount] = result;
    ++gameState->ballCount;
    return result;
}


// GJK/EPA stuff
//Finds the vertex with the greatest distance in a particular direction.
internal Vector3 Support(Vector3 direction, Vector3 * vertices, int count)
{
    int max = 0;
    float dotMax = Vector3DotProduct(*vertices, direction);
    for (int i = 1; i < count; ++i) {
        float dot = Vector3DotProduct(vertices[i], direction);
        if (dot > dotMax)
        {
            max = i;
            dotMax = dot;
        }
    }
    Vector3 result = vertices[max];
    return result;
}

// ClosestPointOnLine
// ClosestPointOnTriangle
// ClosesntPointOnTetrahedron

internal bool GJKDoSimplex(CollisionData *simplex, Vector3 * direction)
{
    // By convention, a always represents the newest vector to have been added.
    switch (simplex->count)
    {
        case 1: break; // Point | invalid code path?
        case 2: // Line
        {
            Vector3 a = Vector3Copy(simplex->vertices[1]);
            Vector3 b = Vector3Copy(simplex->vertices[0]);
            Vector3 ao = Vector3Copy(Vector3Negate(a));
            Vector3 bo = Vector3Copy(Vector3Negate(b));
            
            Vector3 ab = b - a;
            Vector3 ba = a - b;

            float u = Vector3DotProduct(b, ab);
            float v = Vector3DotProduct(a, ba);

            if (v <= 0.f)
            {
                //in region A
                simplex->vertices[0] = Vector3Copy(a);
                simplex->count = 1;
                *direction = ao;
            }
            else if (u <= 0.f)
            {
                //in region B
                simplex->vertices[0] = Vector3Copy(b);
                simplex->count = 1;
                *direction = bo;
            }
            else
            {
                //in region AB
                simplex->vertices[0] = b;
                simplex->vertices[1] = a;
                simplex->count = 2;
                simplex->closestPointMinkowski = u*a + v*b;
                *direction = Vector3TripleProduct(ab, Vector3Negate(a), ab);
            }
            break;
        }
        case 3: // Triangle
        {
            Vector3 a = Vector3Copy(simplex->vertices[2]);
            Vector3 b = Vector3Copy(simplex->vertices[1]);
            Vector3 c = Vector3Copy(simplex->vertices[0]);
            Vector3 ao = Vector3Copy(Vector3Negate(a));
            Vector3 bo = Vector3Copy(Vector3Negate(b));
            Vector3 co = Vector3Copy(Vector3Negate(c));

            Vector3 ab = b - a;
            Vector3 ba = a - b;
            Vector3 bc = c - b;
            Vector3 cb = b - c;
            Vector3 ac = c - a;
            Vector3 ca = a - c;

            float uAB = Vector3DotProduct(b, ab);
            float vAB = Vector3DotProduct(a, ba);
            float uBC = Vector3DotProduct(c, bc);
            float vBC = Vector3DotProduct(b, cb);
            float uCA = Vector3DotProduct(a, ca);
            float vCA = Vector3DotProduct(c, ac);
            
            if ((uCA <= 0.f) && (vAB <= 0.f))
            {//region a
                simplex->count = 1;
                simplex->vertices[0] = Vector3Copy(a);
                *direction = Vector3Copy(ao);
                break;
            }
            else if ((uAB <= 0.f) && (vBC <= 0.f))
            {//region b
                simplex->count = 1;
                simplex->vertices[0] = Vector3Copy(b);
                *direction = Vector3Copy(bo);
                break;
            }
            else if ((uBC <= 0.f) && (vCA <= 0.f))
            {//region c
                simplex->count = 1;
                simplex->vertices[0] = Vector3Copy(c);
                *direction = Vector3Copy(co);
                break;
            }

            Vector3 abXac = Vector3CrossProduct(ab, ac);
            Vector3 bXc = Vector3CrossProduct(b, c);
            Vector3 cXa = Vector3CrossProduct(c, a);
            Vector3 aXb = Vector3CrossProduct(a, b);
    
            float uABC = Vector3DotProduct(bXc, abXac);
            float vABC = Vector3DotProduct(cXa, abXac);
            float wABC = Vector3DotProduct(aXb, abXac);

            if ((uAB > 0.f) && (vAB > 0.f) && (wABC > 0.f))
            {//region AB
                simplex->count = 2;
                simplex->vertices[0] = b;
                simplex->vertices[1] = a;
                *direction = Vector3TripleProduct(ab,ao,ab);
                break;
            }
            else if ((uBC > 0.f) && (vBC > 0.f) && (uABC > 0.f))
            {//region BC
                simplex->count = 2;
                simplex->vertices[0] = c;
                simplex->vertices[1] = b;
                *direction = Vector3TripleProduct(bc,bo,bc);
                break;
            }
            else if ((uCA > 0.f) && (vCA > 0.f) && (vABC > 0.f))
            {//region CA
                simplex->count = 2;
                simplex->vertices[0] = c;
                simplex->vertices[1] = a;
                *direction = Vector3TripleProduct(ac,co,ac);
                break;
            }
            else if ((uABC > 0.f) && (vABC > 0.f) && (wABC > 0.f))
            {//region ABC
                simplex->count = 3;
                simplex->vertices[0] = c;
                simplex->vertices[1] = b;
                simplex->vertices[2] = a;
                if (Vector3DotProduct(abXac, ao) > 0.f)
                {
                    *direction = abXac;
                }
                else
                {
                    *direction = -1.f * abXac;
                }
            }

#if 0
            if (Vector3DotProduct(abcXac, ao) > 0)
            {
                if (Vector3DotProduct(ac, ao) > 0)
                {
                    simplex->vertices[0] = a;
                    simplex->vertices[1] = c;
                    simplex->count = 2;
                    *direction = Vector3TripleProduct(ac,ao,ac);
                }
                else
                {
                    if (Vector3DotProduct(ab, ao) > 0)
                    {
                        simplex->vertices[0] = a;
                        simplex->vertices[1] = b;
                        simplex->count = 2;
                        *direction = Vector3TripleProduct(ab,ao,ab);
                    }
                    else
                    {
                        simplex->vertices[0] = a; // invalid code path?
                        simplex->count = 1;
                        *direction = ao;
                    }
                }
            }
            else
            {
                if (Vector3DotProduct(abXabc, ao) > 0)
                {
                    if (Vector3DotProduct(ab, ao) > 0)
                    {
                        simplex->vertices[0] = a;
                        simplex->vertices[1] = b;
                        simplex->count = 2;
                        *direction = Vector3TripleProduct(ab, ao, ab);
                    }
                    else
                    {
                        simplex->vertices[0] = a; // invalid code path?
                        simplex->count = 1;
                        *direction = ao;
                    }
                }
                else
                {
                    if (Vector3DotProduct(abXac, ao) > 0)
                    {
                        simplex->vertices[0] = a;
                        simplex->vertices[1] = b;
                        simplex->vertices[2] = c;
                        simplex->count = 3;
                        *direction = abXac;
                    }
                    else
                    {
                        simplex->vertices[0] = a;
                        simplex->vertices[1] = c;
                        simplex->vertices[2] = b;
                        simplex->count = 3;
                        *direction = -1.f*abXac;
                    }
                }
            }
#endif
            break;
        }
        case 4: // Tetrahedron
        {
            bool faceChecks[3];

            Vector3 a = Vector3Copy(simplex->vertices[3]);
            Vector3 b = Vector3Copy(simplex->vertices[2]);
            Vector3 c = Vector3Copy(simplex->vertices[1]);
            Vector3 d = Vector3Copy(simplex->vertices[0]);

            Vector3 ao = -1.f * a;
            
            Vector3 ab = b - a; // 321/abc triangle
            Vector3 ac = c - a;
            Vector3 abXac = Vector3CrossProduct(ab, ac);
            faceChecks[0] = (Vector3DotProduct(abXac, ao) > 0);

            // 310/acd triangle
            Vector3 ad = d - a;
            Vector3 acXad = Vector3CrossProduct(ac, ad);
            faceChecks[1] = !(Vector3DotProduct(acXad, ao) > 0);

            // 302/adb triangle
            Vector3 adXab = Vector3CrossProduct(ad, ab);
            faceChecks[2] = (Vector3DotProduct(adXab, ao) > 0);

            if (faceChecks[0])
            {
                if (faceChecks[1])
                {
                    if (faceChecks[2])
                    {
                        simplex->vertices[0] = a;
                        simplex->count = 1;
                        *direction = ao;
                        simplex->hit = false;
                        return true;
                    }
                    else
                    {
                        simplex->vertices[1] = a;
                        simplex->vertices[0] = c;
                        simplex->count = 2;
                        *direction = Vector3TripleProduct(ac, ao, ac);
                    }
                }
                else
                {
                    if (faceChecks[2])
                    {
                        simplex->vertices[1] = a;
                        simplex->vertices[0] = b;
                        simplex->count = 2;
                        *direction = Vector3TripleProduct(ab, ao, ab);
                    }
                    else
                    {
                        simplex->vertices[2] = a;
                        simplex->vertices[1] = b;
                        simplex->vertices[0] = c;
                        simplex->count = 3;
                        *direction = abXac;
                    }
                }
            }
            else
            {
                if (faceChecks[1])
                {
                    if (faceChecks[2])
                    {
                        simplex->vertices[1] = a;
                        simplex->vertices[0] = d;
                        simplex->count = 2;
                        *direction = Vector3TripleProduct(ad, ao, ad);
                    }
                    else
                    {
                        simplex->vertices[2] = a;
                        simplex->vertices[1] = c;
                        simplex->vertices[0] = d;
                        simplex->count = 3;
                        *direction = acXad;
                    }
                }
                else
                {
                    if (faceChecks[2])
                    {
                        simplex->vertices[2] = a;
                        simplex->vertices[1] = b;
                        simplex->vertices[0] = d;
                        simplex->count = 3;
                        *direction = adXab;
                    }
                    else
                    {
                        ASSERT(!faceChecks[0] && !faceChecks[1] && !faceChecks[2]);
                        simplex->hit = true;
                    }
                }
            }
        }
        break;
    }
    return simplex->hit;
}

// This takes in the simplex from GJK and finds a penetration vector,
//which can later be used for colision handling.
//Algorithm/pseudocode from: https://www.youtube.com/watch?v=6rgiPrzqt9w
internal Vector3 RunEPA(CollisionData simplex, float * aVertices, int aCount, float* bVertices, int bCount)
{
    Vector3 polytope [256][3];
    int polytopeFaceCount = 4;
    // Load simplex into polytope
    polytope[0][0] = simplex.vertices[0];
    polytope[0][1] = simplex.vertices[1];
    polytope[0][2] = simplex.vertices[2];

    polytope[1][0] = simplex.vertices[3];
    polytope[1][1] = simplex.vertices[1];
    polytope[1][2] = simplex.vertices[0];

    polytope[2][0] = simplex.vertices[3];
    polytope[2][1] = simplex.vertices[0];
    polytope[2][2] = simplex.vertices[2];

    polytope[3][0] = simplex.vertices[3];
    polytope[3][1] = simplex.vertices[2];
    polytope[3][2] = simplex.vertices[1];

    while (true)
    {
        // Find closest face
        float min = 256;
        int closest = 0;
        Vector3 faceNormal = {};
        Vector3 planeToOrigin = {};
        for (int faceIndex = 0; faceIndex < polytopeFaceCount; ++faceIndex)
        {
            Vector3 pointOnFace = polytope[faceIndex][0];
            Vector3 pointToOrigin = Vector3Zero() - pointOnFace;
            faceNormal = GetPlaneNormalFromTriangle(polytope[faceIndex][0], polytope[faceIndex][1], 
                                                            polytope[faceIndex][2]);
            planeToOrigin = Vector3Project(pointToOrigin, faceNormal);
            float distToFace = Vector3Length(planeToOrigin);
            if (distToFace < min)
            {
                min = distToFace;
                closest = faceIndex;
            }
        }
        Vector3 suppA = Support(faceNormal, (Vector3*)aVertices, aCount);
        Vector3 suppB = Support(Vector3Negate(faceNormal), (Vector3*)bVertices, bCount);
        Vector3 extendPoint = suppA - suppB;
        Vector3 extendPointProj = Vector3Project(extendPoint, faceNormal);
        float distToExtendPoint = Vector3Length(extendPointProj);
        float diff = distToExtendPoint - min;
        if (fabsf(diff) < 0.0001f)
        {
            return Vector3Negate(planeToOrigin);
        }

        Vector3 edges[256][2];
        int edgeCount = 0;
        for (int faceIndex = 0; faceIndex < polytopeFaceCount; ++faceIndex)
        {
            if (Vector3DotProduct(faceNormal, extendPoint) > 0)
            {
                Vector3 a = polytope[faceIndex][0];
                Vector3 b = polytope[faceIndex][1];
                Vector3 c = polytope[faceIndex][2];
                Vector3 ab[2] = {b,a};
                Vector3 bc[2] = {c,b};
                Vector3 ca[2] = {a,c};
                bool edgesDuplicated[3] = {0,0,0};
                for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
                {
                    edgesDuplicated[0] = (edges[edgeIndex][1]==b) & (edges[edgeIndex][0]==a);
                    edgesDuplicated[1] = (edges[edgeIndex][1]==c) & (edges[edgeIndex][0]==b);
                    edgesDuplicated[2] = (edges[edgeIndex][1]==a) & (edges[edgeIndex][0]==c);
                    if (edgesDuplicated[0])
                    {
                        //remove and prevent adding of duplicated/shared edges
                    }
                }
                //add unique edges
            }
        }
    }

    return { 0 };
}


// This function uses the Minkowski sum/difference of two meshes to do collision detection.
//Algorithm/pseudocode from: https://www.youtube.com/watch?v=Qupqu1xe7Io
internal CollisionData RunGJK(GameState*gameState, Mesh aMesh, Matrix aTransform, Mesh bMesh, Matrix bTransform)
{
    float * aVertices = aMesh.vertices;
    int aCount = aMesh.vertexCount;
    for (int vertIndexA = 0; vertIndexA < aCount; ++vertIndexA)
    {
        Vector3 * vertex = ((Vector3 *)aVertices + vertIndexA);
        *vertex = Vector3Transform(*vertex, aTransform);
    }
    
    float * bVertices = bMesh.vertices;
    int bCount = bMesh.vertexCount;
    for (int vertIndexB = 0; vertIndexB < bCount; ++vertIndexB)
    {
        Vector3 * vertex = ((Vector3 *)bVertices + vertIndexB);
        *vertex = Vector3Transform(*vertex, bTransform);
    }

    Vector3 vertices[4];
    CollisionData simplex = { 0 };
    simplex.vertices = vertices;
    Vector3 suppA = Support(Vector3One(), (Vector3*)aVertices, aCount);
    Vector3 suppB = Support(Vector3Negate(Vector3One()), (Vector3*)bVertices, bCount);
    simplex.closestPointA = Vector3Copy(suppA);
    simplex.closestPointB = Vector3Copy(suppB);
    simplex.vertices[0] = suppA - suppB;
    ++simplex.count;
    Vector3 direction = Vector3Negate(simplex.vertices[0]);

    while (true)
    {
        Vector3 suppA = Support(direction, (Vector3*)aVertices, aCount);
        Vector3 suppB = Support(Vector3Negate(direction), (Vector3*)bVertices, bCount);
        Vector3 newSimplexVertex = suppA - suppB;
        if (Vector3DotProduct(newSimplexVertex, direction) <= EPSILON)
        {
            break; // No intersection
        }
        simplex.vertices[simplex.count] = newSimplexVertex;
        ++simplex.count;
        Draw(gameState, simplex);
        if (GJKDoSimplex(&simplex, &direction))
        {
            if (simplex.hit)
            {
                simplex.penetrationVec = RunEPA(simplex, aVertices, aCount, bVertices, bCount);
            }
            return simplex;
            break;
        }
    }

    return { 0 };
}

internal Vector3 MoveBall(GameState * gameState, Ball * ball, float dt, Vector3 force)
{
    float minkowskiRadius = ball->radius * 2.0f;

    Vector3 oldPos = ball->pos;
    Vector3 accel = force * ball->inverseMass;
    //accel.y -= 9.807f; // Gravity
    //accel -= 2.0f * ball->velocity; // Replace -2.0f with some better friction coefficient


    Vector3 playerDelta = (0.5f * accel * square(dt)) + (ball->velocity * dt);
    ball->momentum = ball->momentum + force * dt;
    ball->velocity = ball->momentum * ball->inverseMass;
    //ball->velocity = accel * dt + ball->velocity;
    //Vector3 newPlayerPos = oldPos + playerDelta;
    for (int collisionIterator = 0; collisionIterator < 4; ++collisionIterator)
    {
        float tMin = 1.0f;
        Vector3 desiredPos = ball->pos + playerDelta;

        float len = Vector3Length(playerDelta) + minkowskiRadius;

        Ray ray = { 0 };
        ray.position = ball->pos;
        ray.direction = playerDelta;
        ball->desiredRay = ray;

        int hitObstacleIndex = 0;
        RayCollision collision = { 0 };
        collision.distance = FLOAT_MAX;
        //gjk_result gjkResult = { 0 };
        Vector3 penVec = {};
        for (int obstacleIndex = 0; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
        {
            Obstacle obstacle = gameState->obstacles[obstacleIndex];
            
            Matrix collidingTransform = { 0 };
            Matrix collidingTranslation = { 0 };
            Matrix collidingRotation = { 0 };
            Vector3 collidingPos = { 0 };
            Mesh collidingMesh = { 0 };
            if (obstacleIndex == 0)
            {
                //Vector3 minkowskiSize = gameState->boardDim + 2.0f * ball->radius * Vector3One();
                //GenMeshCube(minkowskiSize.x, minkowskiSize.y, minkowskiSize.z);
                collidingPos = gameState->boardPos;
                collidingMesh = gameState->boardMesh;
                collidingTranslation = MatrixTranslate(gameState->boardPos.x, gameState->boardPos.y, gameState->boardPos.z);
                collidingRotation = MatrixRotate({0,0,0},0.f);
                collidingTransform = GetBoardTransform(gameState);
            }
            else
            {
                collidingPos = obstacle.pos;
                collidingTranslation = MatrixTranslate(collidingPos.x, collidingPos.y, collidingPos.z);
                collidingRotation = MatrixRotate({ 0,0,0 }, 0.f);
                //Matrix rotation = MatrixRotate(gameState->boardAxis, gameState->boardAngle * DEG2RAD);
                collidingTransform = MatrixMultiply(collidingRotation, collidingTranslation);

                collidingMesh = obstacle.mesh;
            }
            
            RayCollision testCollision = GetRayCollisionMesh(ray, collidingMesh, collidingTransform);
            Vector3 sub = ray.position - testCollision.point;
            float subLen = Vector3Length(sub);
            testCollision.distance = subLen;

            Matrix ballTranslation = MatrixTranslate(ball->pos.x, ball->pos.y, ball->pos.z);
            Matrix ballRotation = MatrixRotate({ 0,0,0 }, 0.f);
            Matrix ballTransform = MatrixMultiply(ballRotation, ballTranslation);
            CollisionData simplex = RunGJK(gameState, ball->mesh, ballTransform, collidingMesh, collidingTransform);//RunGJK(ball->mesh, ball->pos, ballRotation, collidingMesh, collidingPos, collidingRotation,gameState);

            float distance = 0.f;
            if (simplex.hit)
            {
                penVec = simplex.penetrationVec;//Vector3 normal = RunEPA(simplex, ball->mesh, ballTransform, collidingMesh, collidingTransform);
            }

#if 1
            if (testCollision.hit && (len > testCollision.distance) && (testCollision.distance < collision.distance) && (len != 0.0f))
            {
                float tResult = testCollision.distance / len;//testCollision.distance / len;
                Vector3 partialNewPos = oldPos + tResult * (playerDelta);
                if ((tResult >= 0.0f) && (tMin > tResult))
                {
                    tMin = (tResult - EPSILON > 0.0f) ? (tResult - EPSILON) : 0.0f;
                    hitObstacleIndex = obstacleIndex;
                }
                collision = testCollision;
            }
#endif
        }

        //ball->pos = ball->pos + ball->velocity * dt;
        ball->pos += tMin * playerDelta;
        if (collision.hit && (len > collision.distance))
        {
            float bounceMultip = (hitObstacleIndex == 0) ? 1.0f : 2.0f;
            ball->collisionRay = { collision.point,  collision.normal + collision.point };
            Vector3 velProjObst = Vector3Project(ball->velocity, collision.normal);
            ball->velocity = ball->velocity - bounceMultip*velProjObst;
            ball->momentum = ball->velocity * ball->mass;

            playerDelta = desiredPos - ball->pos;
            Vector3 deltaProjObst = Vector3Project(playerDelta, collision.normal);
            playerDelta = playerDelta - bounceMultip*deltaProjObst;
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
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        if (mouseCollision.hit)
        {
            PlaySound(gameState->fxCoin);
            Vector3 spawnPoint = mouseCollision.point;
            float radius = 1.0f;
            spawnPoint.y += radius + gameState->boardHeight;
            MakeBall(gameState, spawnPoint, radius, BEIGE);
        }
    }

    Vector3 cameraDir = gameState->camera.target - gameState->camera.position;
    Vector3 cameraDirPartial = 0.5f*Vector3Normalize(cameraDir);
    Vector3 strafeDir = Vector3CrossProduct(cameraDirPartial, gameState->camera.up);
    if (IsKeyDown(KEY_W))
    {
        gameState->camera.position += cameraDirPartial;
        gameState->camera.target += cameraDirPartial;
    }
    if (IsKeyDown(KEY_S))
    {
        gameState->camera.position -= cameraDirPartial;
        gameState->camera.target -= cameraDirPartial;
    }
    if (IsKeyDown(KEY_A))
    {
        gameState->camera.position -= strafeDir;
        gameState->camera.target -= strafeDir;
    }
    if (IsKeyDown(KEY_D))
    {
        gameState->camera.position += strafeDir;
        gameState->camera.target += strafeDir;
    }
    if (IsKeyDown(KEY_SPACE))
    {
        gameState->camera.position.y += 0.2f;
        gameState->camera.target.y += 0.2f;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL))
    {
        gameState->camera.position.y -= 0.2f;
        gameState->camera.target.y -= 0.2f;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_SIDE))
    {
        if (gameState->controllingCamera)
        {
            EnableCursor();
        }
        else
        {
            DisableCursor();
        }
        gameState->controllingCamera = !gameState->controllingCamera;
    }
    if (gameState->controllingCamera)
    {
        UpdateCamera(&gameState->camera, CAMERA_FREE);
        SetMousePosition(0,0);
    }
    //gameState->cameraDist -= (float)GetMouseWheelMove()*0.5f;
    //gameState->camera.position.x = gameState->cameraDist*cos(gameState->angleOffset);
    //gameState->camera.position.z = gameState->cameraDist*sin(gameState->angleOffset);
    //gameState->camera.target = { 0.0f, gameState->camera.position.y, 0.0f };

    if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && 
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
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
    CollisionData simplex = { 0 };
    for (int ballIndex = 1; ballIndex < gameState->ballCount; ++ballIndex)
    {
        Ball * ball = &gameState->balls[ballIndex];
        Vector3 gravity = Vector3{ 0.0f,-9.8f,0.0f } *ball->mass;
        // Instead of rotating every object, instead we only rotate the world/gravity so we can keep everything flat.
        gravity = Vector3RotateByAxisAngle(gravity, gameState->boardAxis, -gameState->boardAngle);
        //accel[ballIndex] = MoveBall(gameState, ball, gameState->dt, gravity);

#if 0 // Old code for testing GJK/EPA
        Obstacle obstacle = gameState->obstacles[1];
        Matrix obstacleTranslation = MatrixTranslate(obstacle.pos.x, obstacle.pos.y, obstacle.pos.z);
        Matrix obstacleRotation = MatrixRotate(Vector3Zero(), 0.f);
        Matrix obstacleTransform = MatrixMultiply(obstacleRotation, obstacleTranslation);

        Matrix ballTranslation = MatrixTranslate(ball->pos.x, ball->pos.y, ball->pos.z);
        Matrix ballRotation = MatrixRotate({ 0,0,0 }, 0.f);
        Matrix ballTransform = MatrixMultiply(ballRotation, ballTranslation);
        simplex = RunGJK(gameState, ball->mesh, ballTransform, obstacle.mesh, obstacleTransform);
#endif
        MoveBall(gameState, ball, gameState->dt, gravity);
    }
    Draw(gameState, simplex);
}

// Called when you recompile the program while its running
void HotReload(GameState *gameState)
{
    TraceLog(LOG_INFO, "HOTRELOAD");

    gameState->angleOffset = -PI/2.0f;
    gameState->cameraDist = 36.0f;

    gameState->camera = { 0 };
    gameState->camera.position;
    gameState->camera.position = { 0.0f, 24.0f, -36.0f };
    gameState->camera.target = { 0.0f, 0.0f, 0.0f };
    gameState->camera.up = { 0.0f, 1.0f, 0.0f };
    gameState->camera.fovy = 45.0f;
    gameState->camera.projection = CAMERA_PERSPECTIVE;

    gameState->boardLength = 36.0f;
    gameState->boardHeight = 1.0f;
    gameState->boardWidth = 27.0f;
    gameState->boardAngle = 6.5f;
    gameState->boardAxis = { -1.0f,0,0 };
    gameState->boardPos = { 0.0f, 0.0f, 0.0f }; //gameState->boardLength*(float)sin(gameState->boardAngle*DEG2RAD), 0.0f };
    gameState->boardMesh = GenMeshCube(gameState->boardWidth, gameState->boardHeight, gameState->boardLength);
    gameState->matDefault = LoadMaterialDefault();
    gameState->matDefault.maps[MATERIAL_MAP_DIFFUSE].color = BLUE;

    gameState->ballCount = 0;
    Ball nullBall = MakeBall(gameState, { 0 }, 0, { 0 });
    gameState->obstacleCount = 0;
    Obstacle nullObstacle = MakeObstacle(gameState, { 0 }, { 0 }, OBSTACLE_TYPE_NULL, { 0 });
    MakeObstacle(gameState, gameState->boardPos - Vector3{0,gameState->boardHeight,0},
                 gameState->boardDim, OBSTACLE_TYPE_BOX, BROWN);

#if 1
    Vector3 sideWallDim = { 0.5f, 2.0f*gameState->boardHeight, gameState->boardLength };
    MakeObstacle(gameState, { -0.5f*gameState->boardWidth, -1.0f, 0.0f}, sideWallDim, OBSTACLE_TYPE_BOX, DARKPURPLE);
    MakeObstacle(gameState, { 0.5f * gameState->boardWidth, -1.0f, 0.0f }, sideWallDim, OBSTACLE_TYPE_BOX, DARKPURPLE);
    Vector3 vertWallDim = { gameState->boardWidth, 2.0f*gameState->boardHeight, 0.5f };
    MakeObstacle(gameState, { 0.0f, -1.0f, 0.5f*gameState->boardLength }, vertWallDim, OBSTACLE_TYPE_BOX, DARKPURPLE);
    MakeObstacle(gameState, { 0.0f, -1.0f, -0.5f*gameState->boardLength }, vertWallDim, OBSTACLE_TYPE_BOX, DARKPURPLE);

    for (int i = -2; i < 2; ++i)
    {
        Vector3 obstaclePosInBoard = { (float)i*4, 0, (float)i*4 };
        Vector3 obstacleSize = { 2,2,2 };
        ObstacleType type = OBSTACLE_TYPE_CYLINDER;
        if (i % 2 == 0)
        {
            type = OBSTACLE_TYPE_BOX;
        }
        MakeObstacle(gameState, obstaclePosInBoard, obstacleSize, type, DARKPURPLE);
    }
#endif
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
