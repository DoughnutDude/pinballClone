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
                DrawPoint3D(obstacle.pos, BLACK);
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
                //accel[ballIndex] += ball.pos;
                //DrawLine3D(ball.pos, accel[ballIndex], BLUE);
                DrawLine3D(ball.desiredRay.position, ball.desiredRay.direction + ball.desiredRay.position, WHITE);
            }
            //Draw simplex for GJK/EPA
            if (simplex.count > 0)
            {
                DrawLine3D(simplex.vertices[0].minkowski, simplex.vertices[1].minkowski, WHITE);
            }
            if (simplex.count >= 3)
            {
                DrawLine3D(simplex.vertices[2].minkowski, simplex.vertices[1].minkowski, WHITE);
                DrawLine3D(simplex.vertices[2].minkowski, simplex.vertices[0].minkowski, WHITE);
            }
            if (simplex.count == 4)
            {
                DrawLine3D(simplex.vertices[3].minkowski, simplex.vertices[1].minkowski, WHITE);
                DrawLine3D(simplex.vertices[3].minkowski, simplex.vertices[2].minkowski, WHITE);
                DrawLine3D(simplex.vertices[3].minkowski, simplex.vertices[0].minkowski, WHITE);
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

        DrawFPS(20,20);
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

internal Obstacle MakeObstacle(GameState * gameState, const char* name, Vector3 posInBoard, Vector3 size, ObstacleType type, float elasticity, Color color)
{
    Obstacle result = { 0 };
    result.name = (char*)name;
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
            result.centerOfRot = result.pos;
            break;
        }
        case OBSTACLE_TYPE_BOX:
        {
            result.pos.y += 0.5f*result.size.y;
            result.mesh = GenMeshCube(result.size.x, result.size.y, result.size.z);
            result.centerOfRot = result.pos;
            break;
        }
    }
    // As obstacle is made, automatically rebase from board coords to real world coords.
    //result.pos = Vector3Transform(result.pos, GetBoardTransform(gameState));
    result.color = color;
    result.elasticity = elasticity;

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
    result.inverseMass = 1.0f/result.mass;
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
// TODO: Should also create support functions for line(capsule collision)
//Finds the vertex with the greatest distance in a particular direction.
internal Vector3 Support(Vector3 direction, Vector3 * vertices, int count)
{
    int indexOfMax = 0;
    float maxDot = Vector3DotProduct(*vertices, direction);
    for (int i = 1; i < count; ++i) {
        float dot = Vector3DotProduct(vertices[i], direction);
        if (dot > maxDot)
        {
            indexOfMax = i;
            maxDot = dot;
        }
    }
    Vector3 result = vertices[indexOfMax];
    return result;
}

internal void CopySimplex(Vector3* arrDest, Vector3* arrSource, int count)
{
    for (int i = 0; i < count; ++i)
    {
        arrDest[i] = Vector3Copy(arrSource[i]);
    }
}

// ClosestPointOnLine
// ClosestPointOnTriangle
// ClosestPointOnTetrahedron

internal void GJKAnalyze(CollisionData * simplex)
{
    float denom = 0;
    for (int i = 0; i < simplex->count; i++)
    {
        denom += simplex->barycentricCoords[i];
    }
    if (denom <= 0.f)
    {
        denom = 1.f;
    }
    switch (simplex->count)
    {
        case 1:
        {
            simplex->closestFeatureI[0] = simplex->closestPointI;
            simplex->closestFeatureIvertexCount = 1;
            simplex->closestFeatureJ[0] = simplex->closestPointJ;
            simplex->closestFeatureJvertexCount = 1;
            break;
        }
        case 2:
        {
            VertexGJK a = simplex->vertices[1];
            VertexGJK b = simplex->vertices[0];
            float u = simplex->barycentricCoords[1];
            float v = simplex->barycentricCoords[0];
            Vector3 iA = a.i * (u/denom);
            Vector3 iB = b.i * (v/denom);
            Vector3 jA = a.j * (u/denom);
            Vector3 jB = b.j * (v/denom);
            simplex->closestPointI = iA + iB;
            simplex->closestPointJ = jA + jB;
            break;
        }
        case 3:
        {
            VertexGJK a = simplex->vertices[2];
            VertexGJK b = simplex->vertices[1];
            VertexGJK c = simplex->vertices[0];
            float u = simplex->barycentricCoords[0];
            float v = simplex->barycentricCoords[1];
            float w = simplex->barycentricCoords[2];
            Vector3 iA = a.i * (u/denom);
            Vector3 iB = b.i * (v/denom);
            Vector3 iC = c.i * (w/denom);
            Vector3 jA = a.j * (u/denom);
            Vector3 jB = b.j * (v/denom);
            Vector3 jC = c.j * (w/denom);
            simplex->closestPointI = iA + iB + iC;
            simplex->closestPointJ = jA + jB + jC;
            break;
        }
        case 4:
        {
            VertexGJK a = simplex->vertices[3];
            VertexGJK b = simplex->vertices[2];
            VertexGJK c = simplex->vertices[1];
            VertexGJK d = simplex->vertices[0];
            float u = simplex->barycentricCoords[3];
            float v = simplex->barycentricCoords[2];
            float w = simplex->barycentricCoords[1];
            float x = simplex->barycentricCoords[0];
            Vector3 iA = a.i * (u/denom);
            Vector3 iB = b.i * (v/denom);
            Vector3 iC = c.i * (w/denom);
            Vector3 iD = d.i * (x/denom);
            Vector3 jA = a.j * (u/denom);
            Vector3 jB = b.j * (v/denom);
            Vector3 jC = c.j * (w/denom);
            Vector3 jD = d.j * (x/denom);
            simplex->closestPointI = iA + iB + iC + iD;
            simplex->closestPointJ = jA + jB + jC + jD;
            break;
        }
    }
    // compute the distance between the 2 shapes.
    if (!simplex->hit)
    {
        Vector3 ij = simplex->closestPointJ - simplex->closestPointI;
        simplex->distanceSquared = Vector3DotProduct(ij, ij);
    }
    else simplex->distanceSquared = 0.f;
}

internal bool GJKDoSimplex(CollisionData *simplex, Vector3 * direction)
{
    // By convention, a always represents the newest vector to have been added.
    switch (simplex->count)
    {
        case 1: break; // Point | invalid code path?
        case 2: // Line
        {
            Vector3 a = Vector3Copy(simplex->vertices[1].minkowski);
            Vector3 b = Vector3Copy(simplex->vertices[0].minkowski);
            Vector3 ao = Vector3Copy(Vector3Negate(a));
            Vector3 bo = Vector3Copy(Vector3Negate(b));

            Vector3 ab = b - a;
            Vector3 ba = a - b;

            float u = Vector3DotProduct(b, ab);
            float v = Vector3DotProduct(a, ba);

            if (v <= 0.f)
            {
                //in region A
                simplex->vertices[0].minkowski = Vector3Copy(a);
                simplex->count = 1;
                *direction = ao;

            }
            else if (u <= 0.f)
            {
                //in region B
                simplex->vertices[0].minkowski = Vector3Copy(b);
                simplex->count = 1;
                *direction = bo;
            }
            else
            {
                //in region AB
                simplex->vertices[0].minkowski = b;
                simplex->vertices[1].minkowski = a;
                simplex->count = 2;
                *direction = Vector3TripleProduct(ab, Vector3Negate(a), ab);

                simplex->barycentricCoords[0] = v;
                simplex->barycentricCoords[1] = u;
            }
            break;
        }
        case 3: // Triangle
        {
            Vector3 a = Vector3Copy(simplex->vertices[2].minkowski);
            Vector3 b = Vector3Copy(simplex->vertices[1].minkowski);
            Vector3 c = Vector3Copy(simplex->vertices[0].minkowski);
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
                simplex->vertices[0].minkowski = Vector3Copy(a);
                *direction = Vector3Copy(ao);
                break;
            }
            else if ((uAB <= 0.f) && (vBC <= 0.f))
            {//region b
                simplex->count = 1;
                simplex->vertices[0].minkowski = Vector3Copy(b);
                *direction = Vector3Copy(bo);
                break;
            }
            else if ((uBC <= 0.f) && (vCA <= 0.f))
            {//region c
                simplex->count = 1;
                simplex->vertices[0].minkowski = Vector3Copy(c);
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
                simplex->vertices[0].minkowski = b;
                simplex->vertices[1].minkowski = a;
                *direction = Vector3TripleProduct(ab,ao,ab);

                simplex->barycentricCoords[0] = vAB;
                simplex->barycentricCoords[1] = uAB;
                break;
            }
            else if ((uBC > 0.f) && (vBC > 0.f) && (uABC > 0.f))
            {//region BC
                simplex->count = 2;
                simplex->vertices[0].minkowski = c;
                simplex->vertices[1].minkowski = b;
                *direction = Vector3TripleProduct(bc,bo,bc);

                simplex->barycentricCoords[0] = vBC;
                simplex->barycentricCoords[1] = uBC;
                break;
            }
            else if ((uCA > 0.f) && (vCA > 0.f) && (vABC > 0.f))
            {//region CA
                simplex->count = 2;
                simplex->vertices[0].minkowski = c;
                simplex->vertices[1].minkowski = a;
                *direction = Vector3TripleProduct(ac,co,ac);

                simplex->barycentricCoords[0] = vCA;
                simplex->barycentricCoords[1] = uCA;
                break;
            }
            else if ((uABC > 0.f) && (vABC > 0.f) && (wABC > 0.f))
            {//region ABC
                simplex->count = 3;
                simplex->vertices[0].minkowski = c;
                simplex->vertices[1].minkowski = b;
                simplex->vertices[2].minkowski = a;
                if (Vector3DotProduct(abXac, ao) > 0.f)
                {
                    *direction = abXac;
                }
                else
                {
                    *direction = -1.f * abXac;
                }

                simplex->barycentricCoords[0] = wABC;
                simplex->barycentricCoords[1] = vABC;
                simplex->barycentricCoords[2] = uABC;
            }
            break;
        }
        case 4: // Tetrahedron
        {
            bool faceChecks[3];

            Vector3 a = Vector3Copy(simplex->vertices[3].minkowski);
            Vector3 b = Vector3Copy(simplex->vertices[2].minkowski);
            Vector3 c = Vector3Copy(simplex->vertices[1].minkowski);
            Vector3 d = Vector3Copy(simplex->vertices[0].minkowski);
            Vector3 ao = -1.f * a;
            Vector3 bo = -1.f * b;
            Vector3 co = -1.f * c;
            Vector3 dNeg = -1.f * d; // aka: do, obviously can't call it that cause that's a syntax word

            Vector3 ab = b - a;
            Vector3 ba = a - b;
            Vector3 bc = c - b;
            Vector3 cb = b - c;
            Vector3 ca = a - c;
            Vector3 ac = c - a;
            Vector3 db = b - d;
            Vector3 bd = d - b;
            Vector3 dc = c - d;
            Vector3 cd = d - c;
            Vector3 da = d - a;
            Vector3 ad = a - d;

            float uAB = Vector3DotProduct(b, ab);
            float vAB = Vector3DotProduct(a, ba);
            float uBC = Vector3DotProduct(c, bc);
            float vBC = Vector3DotProduct(b, cb);
            float uCA = Vector3DotProduct(a, ca);
            float vCA = Vector3DotProduct(c, ac);
            float uBD = Vector3DotProduct(d, bd);
            float vBD = Vector3DotProduct(b, db);
            float uDC = Vector3DotProduct(c, dc);
            float vDC = Vector3DotProduct(d, cd);
            float uAD = Vector3DotProduct(d, ad);
            float vAD = Vector3DotProduct(a, da);
 
            if ((uCA <= 0.f) && (vAB <= 0.f) && (vAD <= 0.f))
            {//region a
                simplex->count = 1;
                simplex->vertices[0].minkowski = Vector3Copy(a);
                *direction = Vector3Copy(ao);
                break;
            }
            else if ((uAB <= 0.f) && (vBC <= 0.f) && (vBD <= 0.f))
            {//region b
                simplex->count = 1;
                simplex->vertices[0].minkowski = Vector3Copy(b);
                *direction = Vector3Copy(bo);
                break;
            }
            else if ((uBC <= 0.f) && (vCA <= 0.f) && (uDC <= 0.f))
            {//region c
                simplex->count = 1;
                simplex->vertices[0].minkowski = Vector3Copy(c);
                *direction = Vector3Copy(co);
                break;
            }
            else if ((uBD <= 0.f) && (vDC <= 0.f) && (uAD <= 0.f))
            {//region d
                simplex->count = 1;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                *direction = Vector3Copy(dNeg);
            }

            Vector3 abXac = Vector3CrossProduct(ab, ac);
            Vector3 bXc = Vector3CrossProduct(b, c);
            Vector3 cXa = Vector3CrossProduct(c, a);
            Vector3 aXb = Vector3CrossProduct(a, b);
            float uABC = Vector3DotProduct(bXc, abXac);
            float vABC = Vector3DotProduct(cXa, abXac);
            float wABC = Vector3DotProduct(aXb, abXac);

            Vector3 adXab = Vector3CrossProduct(ad, ab);
            Vector3 dXb = Vector3CrossProduct(d, b);
            Vector3 bXa = Vector3CrossProduct(b, a);
            Vector3 aXd = Vector3CrossProduct(a, d);
            float uADB = Vector3DotProduct(dXb, adXab);
            float vADB = Vector3DotProduct(bXa, adXab);
            float wADB = Vector3DotProduct(aXd, adXab);

            Vector3 acXad = Vector3CrossProduct(ab, ac);
            Vector3 cXd = Vector3CrossProduct(c, d);
            Vector3 dXa = Vector3CrossProduct(d, a);
            Vector3 aXc = Vector3CrossProduct(a, c);
            float uACD = Vector3DotProduct(cXd, acXad);
            float vACD = Vector3DotProduct(dXa, acXad);
            float wACD = Vector3DotProduct(aXc, acXad);

            Vector3 cbXcd = Vector3CrossProduct(cb, cd);
            Vector3 bXd = Vector3CrossProduct(b, d);
            Vector3 dXc = Vector3CrossProduct(d, c);
            Vector3 cXb = Vector3CrossProduct(c, b);
            float uCBD = Vector3DotProduct(bXd, cbXcd);
            float vCBD = Vector3DotProduct(dXc, cbXcd);
            float wCBD = Vector3DotProduct(cXb, cbXcd);
           
            if ((wABC <= 0.f) && (vADB <= 0.f) && (uAB > 0.f) && (vAB > 0.f))
            {
                //region AB
                simplex->count = 2;
                simplex->vertices[0].minkowski = Vector3Copy(b);
                simplex->vertices[1].minkowski = Vector3Copy(a);
                *direction = Vector3TripleProduct(ab, ao, ab);

                simplex->barycentricCoords[0] = vAB;
                simplex->barycentricCoords[1] = uAB;
            }
            else if ((uABC <= 0.f) && (wCBD <= 0.f) && (uBC > 0.f) && (vBC > 0.f))
            {
                //region BC
                simplex->count = 2;
                simplex->vertices[0].minkowski = Vector3Copy(c);
                simplex->vertices[1].minkowski = Vector3Copy(b);
                *direction = Vector3TripleProduct(bc, bo, bc);

                simplex->barycentricCoords[0] = vBC;
                simplex->barycentricCoords[1] = uBC;
            }
            else if ((vABC <= 0.f) && (wACD <= 0.f) && (uCA > 0.f) && (vCA > 0.f))
            {
                //region CA
                simplex->count = 2;
                simplex->vertices[0].minkowski = Vector3Copy(c);
                simplex->vertices[1].minkowski = Vector3Copy(a);
                *direction = Vector3TripleProduct(ca, co, ca);

                simplex->barycentricCoords[0] = vCA;
                simplex->barycentricCoords[1] = uCA;
            }
            else if ((wADB <= 0.f) && (uCBD <= 0.f) && (uBD > 0.f) && (vBD > 0.f))
            {
                //region BD
                simplex->count = 2;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(b);
                *direction = Vector3TripleProduct(bd, bo, bd);

                simplex->barycentricCoords[0] = vBD;
                simplex->barycentricCoords[1] = uBD;
            }
            else if ((uACD <= 0.f) && (vCBD <= 0.f) && (uDC > 0.f) && (vDC > 0.f))
            {
                //region DC
                simplex->count = 2;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(c);
                *direction = Vector3TripleProduct(dc, dNeg, dc);

                simplex->barycentricCoords[0] = vDC;
                simplex->barycentricCoords[1] = uDC;
            }
            else if ((wADB <= 0.f) && (vACD <= 0.f) && (uAD > 0.f) && (vAD > 0.f))
            {
                //region AD
                simplex->count = 2;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(a);
                *direction = Vector3TripleProduct(ad, ao, ad);

                simplex->barycentricCoords[0] = vAD;
                simplex->barycentricCoords[1] = uAD;
            }

            float denom = Vector3DotProduct(Vector3CrossProduct(ba, bc), bd); //what is this?????
            float volume = (denom == 0.f) ? (1.f) : (1.f/denom);
            float uABCD = Vector3DotProduct(Vector3CrossProduct(c, d), b) * volume;
            float vABCD = Vector3DotProduct(Vector3CrossProduct(c, a), d) * volume;
            float wABCD = Vector3DotProduct(Vector3CrossProduct(d, a), b) * volume;
            float xABCD = Vector3DotProduct(Vector3CrossProduct(b, a), c) * volume;

            if ((xABCD < 0.f) && (uABC > 0.f) && (vABC > 0.f) && (wABC > 0.f))
            {//region ABC
                simplex->count = 3;
                simplex->vertices[0].minkowski = Vector3Copy(c);
                simplex->vertices[1].minkowski = Vector3Copy(b);
                simplex->vertices[2].minkowski = Vector3Copy(a);
                *direction = Vector3CrossProduct(ab, ac);

                simplex->barycentricCoords[0] = wABC;
                simplex->barycentricCoords[1] = vABC;
                simplex->barycentricCoords[2] = uABC;
            }
            else if ((wABCD < 0.f) && (uADB > 0.f) && (vADB > 0.f) && (wADB > 0.f))
            {//region ADB
                simplex->count = 3;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(c);
                simplex->vertices[2].minkowski = Vector3Copy(b);
                *direction = Vector3CrossProduct(ad, ab);

                simplex->barycentricCoords[0] = wADB;
                simplex->barycentricCoords[1] = vADB;
                simplex->barycentricCoords[2] = uADB;
            }
            else if ((vABCD < 0.f) && (uACD > 0.f) && (vACD > 0.f) && (wACD > 0.f))
            {//region ACD
                simplex->count = 3;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(c);
                simplex->vertices[2].minkowski = Vector3Copy(a);
                *direction = Vector3CrossProduct(ac, ad);

                simplex->barycentricCoords[0] = wACD;
                simplex->barycentricCoords[1] = vACD;
                simplex->barycentricCoords[2] = uACD;
            }
            else if ((uABCD < 0.f) && (uCBD > 0.f) && (vCBD > 0.f) && (wCBD > 0.f))
            {//region CBD
                simplex->count = 3;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(c);
                simplex->vertices[2].minkowski = Vector3Copy(b);
                *direction = Vector3CrossProduct(cb, cd);

                simplex->barycentricCoords[0] = wCBD;
                simplex->barycentricCoords[1] = vCBD;
                simplex->barycentricCoords[2] = uCBD;
            }
            else if ((uABCD >= 0.f) && (vABCD >= 0.f) && (wABCD >= 0.f) && (xABCD >= 0.f))
            {//inside tetrahedron, collision found
                simplex->count = 4;
                simplex->vertices[0].minkowski = Vector3Copy(d);
                simplex->vertices[1].minkowski = Vector3Copy(c);
                simplex->vertices[2].minkowski = Vector3Copy(b);
                simplex->vertices[3].minkowski = Vector3Copy(a);
                simplex->hit = true;

                simplex->barycentricCoords[0] = xABCD;
                simplex->barycentricCoords[1] = wABCD;
                simplex->barycentricCoords[2] = vABCD;
                simplex->barycentricCoords[3] = uABCD;
            }
        }
        break;
    }
    return simplex->hit;
}

// This takes in the simplex from GJK and finds a penetration vector,
//which can later be used for colision handling.
//Algorithm/pseudocode from: https://www.youtube.com/watch?v=6rgiPrzqt9w
//    Eventually do not use, instead calculate closest features from GJK for Conservative Advancement TOI solver.
internal Vector3 RunEPA(CollisionData simplex, float * aVertices, int aCount, float* bVertices, int bCount)
{
    Vector3 polytope [256][3];
    int polytopeFaceCount = 4;
    // Load simplex into polytope    
    polytope[0][0] = simplex.vertices[3].minkowski;//ABC
    polytope[0][1] = simplex.vertices[2].minkowski;
    polytope[0][2] = simplex.vertices[1].minkowski;

    polytope[1][0] = simplex.vertices[3].minkowski;//ADB
    polytope[1][1] = simplex.vertices[0].minkowski;
    polytope[1][2] = simplex.vertices[2].minkowski;

    polytope[2][0] = simplex.vertices[3].minkowski;//ACD
    polytope[2][1] = simplex.vertices[1].minkowski;
    polytope[2][2] = simplex.vertices[0].minkowski;

    polytope[3][0] = simplex.vertices[1].minkowski;//CBD
    polytope[3][1] = simplex.vertices[2].minkowski;
    polytope[3][2] = simplex.vertices[0].minkowski;

    while (true)
    {
        // Find closest face (to origin)
        float min = 256;
        int closest = 0;
        Vector3 faceNormal = {};
        Vector3 minFaceNormal = {};
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
                minFaceNormal = Vector3Copy(faceNormal);
            }
        }
        Vector3 suppA = Support(minFaceNormal, (Vector3*)aVertices, aCount);
        Vector3 suppB = Support(Vector3Negate(minFaceNormal), (Vector3*)bVertices, bCount);
        Vector3 extendPoint = suppA - suppB;
        Vector3 extendPointProj = Vector3Project(extendPoint, faceNormal);
        float distToExtendPoint = Vector3Length(extendPointProj);
        float diff = distToExtendPoint - min;
        if (fabsf(diff) < 0.0001f)
        {
            return Vector3Negate(planeToOrigin);
        }

        // Repair polytope/fix edges
        Vector3 edges[256][2];
        int edgeCount = 0;
        for (int faceIndex = 0; faceIndex < polytopeFaceCount; ++faceIndex)
        {
            if (Vector3DotProduct(faceNormal, extendPoint) > 0) //shouldn't this be minFaceNormal?
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
// as well as from: https://box2d.org/files/ErinCatto_GJK_GDC2010.pdf
internal CollisionData RunGJK(GameState*gameState, Mesh iMesh, Matrix iTransform, Mesh jMesh, Matrix jTransform)
{
    float * iVertices = iMesh.vertices;
    int iCount = iMesh.vertexCount;
    for (int vertIndexI = 0; vertIndexI < iCount; ++vertIndexI)
    {
        Vector3 * vertex = ((Vector3 *)iVertices + vertIndexI);
        *vertex = Vector3Transform(*vertex, iTransform);
    }
    
    float * jVertices = jMesh.vertices;
    int jCount = jMesh.vertexCount;
    for (int vertIndexJ = 0; vertIndexJ < jCount; ++vertIndexJ)
    {
        Vector3 * vertex = ((Vector3 *)jVertices + vertIndexJ);
        *vertex = Vector3Transform(*vertex, jTransform);
    }

    CollisionData simplex = { 0 };
    Vector3 suppI = Support(Vector3One(), (Vector3*)iVertices, iCount);
    Vector3 suppJ = Support(Vector3Negate(Vector3One()), (Vector3*)jVertices, jCount);
    simplex.closestPointI = Vector3Copy(suppI);
    simplex.closestPointJ = Vector3Copy(suppJ);
    simplex.vertices[0].minkowski = suppI - suppJ;
    //simplex.closestPointMinkowski = simplex.vertices[0].minkowski;
    ++simplex.count;
    Vector3 direction = Vector3Negate(simplex.vertices[0].minkowski);

    while (true)
    {
        Vector3 suppI = Support(direction, (Vector3*)iVertices, iCount);
        Vector3 suppJ = Support(Vector3Negate(direction), (Vector3*)jVertices, jCount);
        VertexGJK newSimplexVertex = {0};
        newSimplexVertex.i = suppI;
        newSimplexVertex.j = suppJ;
        newSimplexVertex.minkowski = suppI - suppJ;
        if (Vector3DotProduct(newSimplexVertex.minkowski, direction) <= EPSILON)
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
                GJKAnalyze(&simplex);
                // TODO: Add more termination conditions? Repeated support point vertices. Distance always decreasing. 
                simplex.penetrationVec = RunEPA(simplex, iVertices, iCount, jVertices, jCount);
            }
            return simplex;
            break;
        }
    }

    return { 0 };
}

internal Vector3 MoveBall(GameState * gameState, Ball * ball, float dt, Vector3 force)
{
    Vector3 oldPos = ball->pos;
    Vector3 accel = force * ball->inverseMass;
    //accel.y -= 9.807f; // Gravity
    //accel -= 2.0f * ball->velocity; // Replace -2.0f with some better friction coefficient

    ball->momentum = ball->momentum + force * dt;
    ball->velocity = ball->momentum * ball->inverseMass;
    Vector3 playerDelta = (0.5f * accel * square(dt)) + (ball->velocity * dt);
    //ball->velocity = accel * dt + ball->velocity;

    // Broad?-phase: Compute swept collision hulls based on rotation and translation of each obstacle and compare to the swept sphere (capsule) of the ball to see which ones the ball could potentially collide with.
    Obstacle potentialColliders[MAX_OBSTACLES];
    int colliderCount = 0;
    for (int obstacleIndex = 1; obstacleIndex < gameState->obstacleCount; ++obstacleIndex)
    {
        Obstacle obstacle = gameState->obstacles[obstacleIndex];
        float* obstVertices = obstacle.mesh.vertices;
        int obstVertexCount = obstacle.mesh.vertexCount;
        // Find point furthest from center of rotation to compute radius of sweptated hull
        int indexOfMax = 0;
        float maxDist = 0.f;
        for (int vertIndex = 0; vertIndex < obstVertexCount; ++vertIndex)
        {
            Vector3 vertex = Vector3Copy(*((Vector3*)obstVertices + vertIndex));
            Matrix obstacleTranslation = MatrixTranslate(obstacle.pos.x, obstacle.pos.y, obstacle.pos.z);
            Matrix obstacleRotation = MatrixRotate({ 0,0,0 }, 0.f);
            Matrix obstacleTransform = MatrixMultiply(obstacleRotation, obstacleTranslation);
            vertex = Vector3Transform(vertex, obstacleTransform);
            float dist = Vector3Length(vertex - obstacle.centerOfRot);
            if (dist > maxDist)
            {
                indexOfMax = vertIndex;
                maxDist = dist;
            }
        }
        // TODO: When we get moving obstacles, they need to be a capsule as well. Currently, only ball is swept.
        float obstSweptRadius = maxDist;
        Vector3 ballStart = ball->pos;
        Vector3 ballEnd = ballStart + playerDelta;
        Vector3 ballSweep = ballEnd - ballStart;
        Vector3 obstStart = obstacle.pos;

        Vector3 ballToObst = obstacle.centerOfRot - ballStart;
        float u = Vector3DotProduct(ballToObst, ballSweep) / Vector3DotProduct(ballSweep, ballSweep);
        if (u > 1.f)
        {
            u = 1.f;
        }
        else if (u < 0.f)
        {
            u = 0.f;
        }
        Vector3 closestPointOnLineSeg = (u * ballSweep) + ballStart;
        float distBetweenSphereCenters = Vector3Length(obstacle.centerOfRot - closestPointOnLineSeg);
        if (distBetweenSphereCenters < (ball->radius + obstSweptRadius))
        {
            // Swept hulls collide, so add the obstacle to the list
            potentialColliders[colliderCount] = obstacle;
            ++colliderCount;
        }
    }


    // Maybe should move the Broad?-phase inside this loop so that potentialColliders is recomputed after each subtick movement?
    for (int collisionIterator = 0; collisionIterator < 4; ++collisionIterator)
    {
        float tMin = 1.0f;
        Vector3 desiredPos = ball->pos + playerDelta;

        //int collidedObstacleType = 0;
        Vector3 closestPoint;
        Vector3 collidingNormal;
        bool hitActive = false;
        Obstacle collidedObstacle = {};
        for (int colliderIndex = 0; colliderIndex < colliderCount; ++colliderIndex)
        {
            Obstacle collider = potentialColliders[colliderIndex];

            Matrix colliderTransform = { 0 };
            Matrix colliderTranslation = { 0 };
            Matrix colliderRotation = { 0 };
            Vector3 colliderPos = { 0 };
            Mesh colliderMesh = { 0 };
            if (colliderIndex == 0)
            {
                //Vector3 minkowskiSize = gameState->boardDim + 2.0f * ball->radius * Vector3One();
                //GenMeshCube(minkowskiSize.x, minkowskiSize.y, minkowskiSize.z);
                colliderPos = gameState->boardPos;
                colliderMesh = gameState->boardMesh;
                colliderTranslation = MatrixTranslate(gameState->boardPos.x, gameState->boardPos.y, gameState->boardPos.z);
                colliderRotation = MatrixRotate({ 0,0,0 }, 0.f);
                colliderTransform = GetBoardTransform(gameState);
            }
            else
            {
                colliderPos = collider.pos;
                colliderTranslation = MatrixTranslate(colliderPos.x, colliderPos.y, colliderPos.z);
                colliderRotation = MatrixRotate({ 0,0,0 }, 0.f);
                //Matrix rotation = MatrixRotate(gameState->boardAxis, gameState->boardAngle * DEG2RAD);
                colliderTransform = MatrixMultiply(colliderRotation, colliderTranslation);

                colliderMesh = collider.mesh;
            }

            Vector3* colliderVertices = (Vector3*)colliderMesh.vertices;
            int obstVertexCount = colliderMesh.vertexCount;
            for (int vertIndex = 0; vertIndex < obstVertexCount; ++vertIndex)
            {
                Vector3* vertex = colliderVertices + vertIndex;
                *vertex = Vector3Transform(*vertex, colliderTransform);
            }

            //Matrix ballTranslation = MatrixTranslate(ball->pos.x, ball->pos.y, ball->pos.z);
            //Matrix ballRotation = MatrixRotate({ 0,0,0 }, 0.f);
            //Matrix ballTransform = MatrixMultiply(ballRotation, ballTranslation);
            //CollisionData simplex = RunGJK(gameState, ball->mesh, ballTransform, collidingMesh, collidingTransform);//RunGJK(ball->mesh, ball->pos, ballRotation, collidingMesh, collidingPos, collidingRotation,gameState);


            int simplexCount = 0;
            Vector3 pointToBall = {};
            Vector3 pointOnBall = {};
            Vector3 TRUEclosestPoint = {};
            float pointToBallDotPlaneNormal = 0.f;
            float bracketStart = 0.f;
            float bracketEnd = 1.f;
            float bracketMid = (bracketEnd - bracketStart)/2.f;
            float distance = FLOAT_MAX;
            Vector3 planeNormalSA = {};
            Vector3 q = Vector3Copy(ball->pos) + (bracketMid*playerDelta);
            int gjkCount = 0;//temp debug output
            while ((float)(bracketEnd - bracketStart) > (float)EPSILON)
            {
                ++gjkCount;
                distance = FLOAT_MAX;

                Vector3 supportPoint = Support(Vector3One(), (Vector3*)colliderVertices, obstVertexCount);
                Vector3 direction = q - supportPoint;
                float barycentricCoords[4];
                Vector3 closestFeature[4] = {};
                Vector3 closestPointOnBall = {};
                Vector3 simplex[4] = {};
                Vector3 oldSimplex[4] = {};
                int oldSimplexCount;
                simplex[0] = supportPoint;
                simplexCount = 1;
                CopySimplex(oldSimplex, simplex, simplexCount);
                oldSimplexCount = simplexCount;
                bool hit = false;
                while (true)
                {
                    CopySimplex(oldSimplex, simplex, simplexCount);
                    oldSimplexCount = simplexCount;
                    Vector3 newSupport = Support(direction, (Vector3*)colliderVertices, obstVertexCount);
                    bool vertexAlreadyInSimplex = false;
                    for (int i = 0; i < oldSimplexCount; ++i)
                    {
                        if ((oldSimplex[i].x == newSupport.x) && (oldSimplex[i].y == newSupport.y) && (oldSimplex[i].z == newSupport.z))
                        {
                            vertexAlreadyInSimplex = true;
                            break;
                        }
                    }
                    if (vertexAlreadyInSimplex)// || (Vector3DotProduct(newSupport - q, direction) < EPSILON))
                    {
                        if (simplexCount == 2)
                            int pen = 0;
                        CopySimplex(simplex, oldSimplex, oldSimplexCount);
                        simplexCount = oldSimplexCount;
                        break;
                    }
                    simplex[simplexCount] = newSupport;
                    ++simplexCount;
                    switch (simplexCount)
                    {
                        case 1: break; //invalid code path?
                        case 2: // Line
                        {
                            Vector3 a = Vector3Copy(simplex[1]);
                            Vector3 b = Vector3Copy(simplex[0]);
                            Vector3 aq = q - a;
                            Vector3 bq = q - b;

                            Vector3 ab = b - a;
                            Vector3 ba = a - b;

                            float u = (Vector3DotProduct(aq,Vector3Normalize(ab)) / Vector3Length(ab));
                            float v = Vector3DotProduct(bq, Vector3Normalize(ba)) / Vector3Length(ba);

                            if (v <= 0.f)
                            {
                                //in region A
                                simplex[0] = Vector3Copy(a);
                                simplexCount = 1;
                                direction = aq;
                                break;

                            }
                            else if (u <= 0.f)
                            {
                                //in region B
                                simplex[0] = Vector3Copy(b);
                                simplexCount = 1;
                                direction = bq;
                                break;
                            }
                            else
                            {
                                //in region AB
                                simplex[0] = b;
                                simplex[1] = a;
                                simplexCount = 2;
                                direction = Vector3TripleProduct(ab, aq, ab);

                                barycentricCoords[0] = v;
                                barycentricCoords[1] = u;
                                break;
                            }
                            break;
                        }
                        case 3: // Triangle
                        {
                            Vector3 a = Vector3Copy(simplex[2]);
                            Vector3 b = Vector3Copy(simplex[1]);
                            Vector3 c = Vector3Copy(simplex[0]);
                            Vector3 aq = q - a;
                            Vector3 bq = q - b;
                            Vector3 cq = q - c;

                            Vector3 ab = b - a;
                            Vector3 ba = a - b;
                            Vector3 bc = c - b;
                            Vector3 cb = b - c;
                            Vector3 ac = c - a;
                            Vector3 ca = a - c;

                            float uAB = (Vector3DotProduct(bq, Vector3Normalize(ba)) / Vector3Length(ba));;
                            float vAB = (Vector3DotProduct(aq, Vector3Normalize(ab)) / Vector3Length(ab));
                            float uBC = (Vector3DotProduct(cq, Vector3Normalize(cb)) / Vector3Length(cb));
                            float vBC = (Vector3DotProduct(bq, Vector3Normalize(bc)) / Vector3Length(bc));
                            float uCA = (Vector3DotProduct(aq, Vector3Normalize(ac)) / Vector3Length(ac));
                            float vCA = (Vector3DotProduct(cq, Vector3Normalize(ca)) / Vector3Length(ca));

                            if ((uCA <= 0.f) && (vAB <= 0.f))
                            {//region a
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(a);
                                direction = Vector3Copy(aq);
                                break;
                            }
                            else if ((uAB <= 0.f) && (vBC <= 0.f))
                            {//region b
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(b);
                                direction = Vector3Copy(bq);
                                break;
                            }
                            else if ((uBC <= 0.f) && (vCA <= 0.f))
                            {//region c
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(c);
                                direction = Vector3Copy(cq);
                                break;
                            }

                            Vector3 abXac = Vector3CrossProduct(ab, ac);
                            Vector3 g = q - Vector3Project(q, abXac);
                            Vector3 ga = a - g;
                            Vector3 gb = b - g;
                            Vector3 gc = c - g;
                            Vector3 gbXgc = Vector3CrossProduct(gb, gc);
                            Vector3 gcXga = Vector3CrossProduct(gc, ga);
                            Vector3 gaXgb = Vector3CrossProduct(ga, gb);

                            float uABC = ((Vector3DotProduct(gbXgc, abXac) >= 0) ? (1.f) : (-1.f)) * Vector3Length(gbXgc)/Vector3Length(abXac);
                            float vABC = ((Vector3DotProduct(gcXga, abXac) >= 0) ? (1.f) : (-1.f)) * Vector3Length(gcXga)/Vector3Length(abXac);
                            float wABC = ((Vector3DotProduct(gaXgb, abXac) >= 0) ? (1.f) : (-1.f)) * Vector3Length(gaXgb)/Vector3Length(abXac);
                            //float uABC = Vector3DotProduct(qbXqc, abXac);
                            //float vABC = Vector3DotProduct(qcXqa, abXac);
                            //float wABC = Vector3DotProduct(qaXqb, abXac);

                            if ((uAB > 0.f) && (vAB > 0.f) && (wABC <= 0.f))
                            {//region AB
                                simplexCount = 2;
                                simplex[0] = b;
                                simplex[1] = a;
                                direction = Vector3TripleProduct(ab, aq, ab);

                                barycentricCoords[0] = vAB;
                                barycentricCoords[1] = uAB;
                                break;
                            }
                            else if ((uBC > 0.f) && (vBC > 0.f) && (uABC <= 0.f))
                            {//region BC
                                simplexCount = 2;
                                simplex[0] = c;
                                simplex[1] = b;
                                direction = Vector3TripleProduct(bc, bq, bc);

                                barycentricCoords[0] = vBC;
                                barycentricCoords[1] = uBC;
                                break;
                            }
                            else if ((uCA > 0.f) && (vCA > 0.f) && (vABC <= 0.f))
                            {//region CA
                                simplexCount = 2;
                                simplex[0] = c;
                                simplex[1] = a;
                                direction = Vector3TripleProduct(ca, cq, ca);

                                barycentricCoords[1] = vCA;
                                barycentricCoords[0] = uCA;
                                break;
                            }
                            else if ((uABC > 0.f) && (vABC > 0.f) && (wABC > 0.f))
                            {//region ABC
                                simplexCount = 3;
                                simplex[0] = c;
                                simplex[1] = b;
                                simplex[2] = a;
                                if (Vector3DotProduct(abXac, aq) > 0.f)
                                {
                                    direction = abXac;
                                }
                                else
                                {
                                    direction = -1.f * abXac;
                                }

                                barycentricCoords[0] = wABC;
                                barycentricCoords[1] = vABC;
                                barycentricCoords[2] = uABC;
                                break;
                            }
                            break;
                        }
                        case 4: // Tetrahedron
                        {
                            bool faceChecks[3];

                            Vector3 a = Vector3Copy(simplex[3]);
                            Vector3 b = Vector3Copy(simplex[2]);
                            Vector3 c = Vector3Copy(simplex[1]);
                            Vector3 d = Vector3Copy(simplex[0]);
                            Vector3 aq = q - a;
                            Vector3 bq = q - b;
                            Vector3 cq = q - c;
                            Vector3 dq = q - d;

                            Vector3 ab = b - a;
                            Vector3 ba = a - b;
                            Vector3 bc = c - b;
                            Vector3 cb = b - c;
                            Vector3 ca = a - c;
                            Vector3 ac = c - a;
                            Vector3 db = b - d;
                            Vector3 bd = d - b;
                            Vector3 dc = c - d;
                            Vector3 cd = d - c;
                            Vector3 ad = d - a;
                            Vector3 da = a - d;

                            float uAB = Vector3DotProduct(bq, Vector3Normalize(ba)) / Vector3Length(ba);
                            float vAB = Vector3DotProduct(aq, Vector3Normalize(ab)) / Vector3Length(ab);
                            float uBC = Vector3DotProduct(cq, Vector3Normalize(cb)) / Vector3Length(cb);
                            float vBC = Vector3DotProduct(bq, Vector3Normalize(bc)) / Vector3Length(bc);
                            float uCA = Vector3DotProduct(aq, Vector3Normalize(ac)) / Vector3Length(ac);
                            float vCA = Vector3DotProduct(cq, Vector3Normalize(ca)) / Vector3Length(ca);
                            float uBD = Vector3DotProduct(dq, Vector3Normalize(db)) / Vector3Length(db);
                            float vBD = Vector3DotProduct(bq, Vector3Normalize(bd)) / Vector3Length(bd);
                            float uDC = Vector3DotProduct(cq, Vector3Normalize(cd)) / Vector3Length(cd);
                            float vDC = Vector3DotProduct(dq, Vector3Normalize(dc)) / Vector3Length(dc);
                            float uAD = Vector3DotProduct(dq, Vector3Normalize(da)) / Vector3Length(da);
                            float vAD = Vector3DotProduct(aq, Vector3Normalize(ad)) / Vector3Length(ad);

                            if ((uCA <= 0.f) && (vAB <= 0.f) && (vAD <= 0.f))
                            {//region a
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(a);
                                direction = Vector3Copy(aq);
                                break;
                            }
                            else if ((uAB <= 0.f) && (vBC <= 0.f) && (vBD <= 0.f))
                            {//region b
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(b);
                                direction = Vector3Copy(bq);
                                break;
                            }
                            else if ((uBC <= 0.f) && (vCA <= 0.f) && (uDC <= 0.f))
                            {//region c
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(c);
                                direction = Vector3Copy(cq);
                                break;
                            }
                            else if ((uBD <= 0.f) && (vDC <= 0.f) && (uAD <= 0.f))
                            {//region d
                                simplexCount = 1;
                                simplex[0] = Vector3Copy(d);
                                direction = Vector3Copy(dq);
                                break;
                            }

                            Vector3 abXac = Vector3CrossProduct(ab, ac);
                            Vector3 gABC = q - Vector3Project(q, abXac);
                            Vector3 gABCa = a - gABC;
                            Vector3 gABCb = b - gABC;
                            Vector3 gABCc = c - gABC;
                            Vector3 gABCd = d - gABC;
                            Vector3 qbXqc = Vector3CrossProduct(gABCb, gABCc);
                            Vector3 qcXqa = Vector3CrossProduct(gABCc, gABCa);
                            Vector3 qaXqb = Vector3CrossProduct(gABCa, gABCb);
                            //float uABC = ((Vector3DotProduct(qbXqc, abXac) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qbXqc) / Vector3Length(abXac);
                            //float vABC = ((Vector3DotProduct(qcXqa, abXac) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qcXqa) / Vector3Length(abXac);
                            //float wABC = ((Vector3DotProduct(qaXqb, abXac) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qaXqb) / Vector3Length(abXac);
                            float uABC = Vector3DotProduct(qbXqc, abXac);
                            float vABC = Vector3DotProduct(qcXqa, abXac);
                            float wABC = Vector3DotProduct(qaXqb, abXac);

                            Vector3 adXab = Vector3CrossProduct(ad, ab);
                            Vector3 gADB = q - Vector3Project(q, adXab);
                            Vector3 gADBa = a - gADB;
                            Vector3 gADBb = b - gADB;
                            Vector3 gADBc = c - gADB;
                            Vector3 gADBd = d - gADB;
                            Vector3 qdXqb = Vector3CrossProduct(gADBd, gADBb);
                            Vector3 qbXqa = Vector3CrossProduct(gADBb, gADBa);
                            Vector3 qaXqd = Vector3CrossProduct(gADBa, gADBd);
                            //float uADB = ((Vector3DotProduct(qdXqb, adXab) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qdXqb) / Vector3Length(adXab);
                            //float vADB = ((Vector3DotProduct(qbXqa, adXab) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qbXqa) / Vector3Length(adXab);
                            //float wADB = ((Vector3DotProduct(qaXqd, adXab) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qaXqd) / Vector3Length(adXab);
                            float uADB = Vector3DotProduct(qdXqb, adXab);
                            float vADB = Vector3DotProduct(qbXqa, adXab);
                            float wADB = Vector3DotProduct(qaXqd, adXab);

                            Vector3 acXad = Vector3CrossProduct(ac, ad);
                            Vector3 gACD = q - Vector3Project(q, acXad);
                            Vector3 gACDa = a - gACD;
                            Vector3 gACDb = b - gACD;
                            Vector3 gACDc = c - gACD;
                            Vector3 gACDd = d - gACD;
                            Vector3 qcXqd = Vector3CrossProduct(gACDc, gACDd);
                            Vector3 qdXqa = Vector3CrossProduct(gACDd, gACDa);
                            Vector3 qaXqc = Vector3CrossProduct(gACDa, gACDc);
                            //float uACD = ((Vector3DotProduct(qcXqd, acXad) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qcXqd) / Vector3Length(acXad);
                            //float vACD = ((Vector3DotProduct(qdXqa, acXad) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qdXqa) / Vector3Length(acXad);
                            //float wACD = ((Vector3DotProduct(qaXqc, acXad) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qaXqc) / Vector3Length(acXad);
                            float uACD = Vector3DotProduct(qcXqd, acXad);
                            float vACD = Vector3DotProduct(qdXqa, acXad);
                            float wACD = Vector3DotProduct(qaXqc, acXad);

                            Vector3 cbXcd = Vector3CrossProduct(cb, cd);
                            Vector3 gCBD = q - Vector3Project(q, cbXcd);
                            Vector3 gCBDa = a - gCBD;
                            Vector3 gCBDb = b - gCBD;
                            Vector3 gCBDc = c - gCBD;
                            Vector3 gCBDd = d - gCBD;
                            Vector3 qbXqd = Vector3CrossProduct(gCBDb, gCBDd);
                            Vector3 qdXqc = Vector3CrossProduct(gCBDd, gCBDc);
                            Vector3 qcXqb = Vector3CrossProduct(gCBDc, gCBDb);
                            //float uCBD = ((Vector3DotProduct(qbXqd, cbXcd) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qbXqd) / Vector3Length(cbXcd);
                            //float vCBD = ((Vector3DotProduct(qdXqc, cbXcd) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qdXqc) / Vector3Length(cbXcd);
                            //float wCBD = ((Vector3DotProduct(qcXqb, cbXcd) >= 0) ? (1.f) : (-1.f)) * Vector3Length(qcXqb) / Vector3Length(cbXcd);
                            float uCBD = Vector3DotProduct(qbXqd, cbXcd);
                            float vCBD = Vector3DotProduct(qdXqc, cbXcd);
                            float wCBD = Vector3DotProduct(qcXqb, cbXcd);

                            if ((wABC <= 0.f) && (vADB <= 0.f) && (uAB > 0.f) && (vAB > 0.f))
                            {
                                //region AB
                                simplexCount = 2;
                                simplex[0] = Vector3Copy(b);
                                simplex[1] = Vector3Copy(a);
                                direction = Vector3TripleProduct(ab, aq, ab);

                                barycentricCoords[0] = vAB;
                                barycentricCoords[1] = uAB;
                                break;
                            }
                            else if ((uABC <= 0.f) && (wCBD <= 0.f) && (uBC > 0.f) && (vBC > 0.f))
                            {
                                //region BC
                                simplexCount = 2;
                                simplex[0] = Vector3Copy(c);
                                simplex[1] = Vector3Copy(b);
                                direction = Vector3TripleProduct(bc, bq, bc);

                                barycentricCoords[0] = vBC;
                                barycentricCoords[1] = uBC;
                                break;
                            }
                            else if ((vABC <= 0.f) && (wACD <= 0.f) && (uCA > 0.f) && (vCA > 0.f))
                            {
                                //region CA
                                simplexCount = 2;
                                simplex[0] = Vector3Copy(c);
                                simplex[1] = Vector3Copy(a);
                                direction = Vector3TripleProduct(ca, cq, ca);

                                barycentricCoords[1] = vCA;
                                barycentricCoords[0] = uCA;
                                break;
                            }
                            else if ((wADB <= 0.f) && (uCBD <= 0.f) && (uBD > 0.f) && (vBD > 0.f))
                            {
                                //region BD
                                simplexCount = 2;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(b);
                                direction = Vector3TripleProduct(bd, bq, bd);

                                barycentricCoords[0] = vBD;
                                barycentricCoords[1] = uBD;
                                break;
                            }
                            else if ((uACD <= 0.f) && (vCBD <= 0.f) && (uDC > 0.f) && (vDC > 0.f))
                            {
                                //region DC
                                simplexCount = 2;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(c);
                                direction = Vector3TripleProduct(dc, dq, dc);

                                barycentricCoords[1] = vDC;
                                barycentricCoords[0] = uDC;
                                break;
                            }
                            else if ((wADB <= 0.f) && (vACD <= 0.f) && (uAD > 0.f) && (vAD > 0.f))
                            {
                                //region AD
                                simplexCount = 2;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(a);
                                direction = Vector3TripleProduct(ad, aq, ad);

                                barycentricCoords[0] = vAD;
                                barycentricCoords[1] = uAD;
                                break;
                            }

                            float denom = Vector3DotProduct(da, Vector3CrossProduct(db, dc));
                            float volume = (denom == 0.f) ? (1.f) : (1.f / denom);
                            Vector3 qa = a - q;
                            Vector3 qb = b - q;
                            Vector3 qc = c - q;
                            Vector3 qd = d - q;
                            float uABCD = Vector3DotProduct(qc, Vector3CrossProduct(qb, qd)) * volume;
                            float vABCD = Vector3DotProduct(qa, Vector3CrossProduct(qc, qd)) * volume;
                            float wABCD = Vector3DotProduct(qa, Vector3CrossProduct(qd, qb)) * volume;
                            float xABCD = Vector3DotProduct(qa, Vector3CrossProduct(qb, qc)) * volume;

                            if ((xABCD < 0.f) && (uABC > 0.f) && (vABC > 0.f) && (wABC > 0.f))
                            {//region ABC
                                simplexCount = 3;
                                simplex[0] = Vector3Copy(c);
                                simplex[1] = Vector3Copy(b);
                                simplex[2] = Vector3Copy(a);
                                direction = Vector3CrossProduct(ab, ac);

                                barycentricCoords[0] = wABC;
                                barycentricCoords[1] = vABC;
                                barycentricCoords[2] = uABC;
                                break;
                            }
                            else if ((wABCD < 0.f) && (uADB > 0.f) && (vADB > 0.f) && (wADB > 0.f))
                            {//region ADB
                                simplexCount = 3;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(c);
                                simplex[2] = Vector3Copy(b);
                                direction = Vector3CrossProduct(ad, ab);

                                barycentricCoords[0] = wADB;
                                barycentricCoords[1] = vADB;
                                barycentricCoords[2] = uADB;
                                break;
                            }
                            else if ((vABCD < 0.f) && (uACD > 0.f) && (vACD > 0.f) && (wACD > 0.f))
                            {//region ACD
                                simplexCount = 3;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(c);
                                simplex[2] = Vector3Copy(a);
                                direction = Vector3CrossProduct(ac, ad);

                                barycentricCoords[0] = wACD;
                                barycentricCoords[1] = vACD;
                                barycentricCoords[2] = uACD;
                                break;
                            }
                            else if ((uABCD < 0.f) && (uCBD > 0.f) && (vCBD > 0.f) && (wCBD > 0.f))
                            {//region CBD
                                simplexCount = 3;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(c);
                                simplex[2] = Vector3Copy(b);
                                direction = Vector3CrossProduct(cb, cd);

                                barycentricCoords[0] = wCBD;
                                barycentricCoords[1] = vCBD;
                                barycentricCoords[2] = uCBD;
                                break;
                            }
                            else if ((uABCD > 0.f) && (vABCD > 0.f) && (wABCD > 0.f) && (xABCD > 0.f))
                            {//inside tetrahedron, collision found
                                simplexCount = 4;
                                simplex[0] = Vector3Copy(d);
                                simplex[1] = Vector3Copy(c);
                                simplex[2] = Vector3Copy(b);
                                simplex[3] = Vector3Copy(a);

                                barycentricCoords[0] = xABCD;
                                barycentricCoords[1] = wABCD;
                                barycentricCoords[2] = vABCD;
                                barycentricCoords[3] = uABCD;
                                hit = true;
                                break;
                            }
                            break;
                        }
                    }
#if 0
                    TraceLog(LOG_INFO, "gjkCount: %d, simplex:\n", gjkCount);//debug output
                    for (int s = 0; s < simplexCount; ++s)
                    {
                        TraceLog(LOG_INFO, "\t %f, %f, %f\n", simplex[s].x, simplex[s].y, simplex[s].z);//debug output
                    }
#endif
                    // Analyzing the GJK results to get closest points/features and distance between
                    float denom = 0;
                    for (int i = 0; i < simplexCount; i++)
                    {
                        denom += barycentricCoords[i];
                    }
                    if (denom <= 0.f)
                    {
                        denom = 1.f;
                    }
                    switch (simplexCount)
                    {
                        case 1://invalid code path?
                        {
                            closestPoint = simplex[0];
                            closestFeature[0] = simplex[0];
                            planeNormalSA = q - closestPoint;
                            break;
                        }
                        case 2:
                        {
                            Vector3 a = simplex[1];
                            Vector3 b = simplex[0];
                            float v = barycentricCoords[1];
                            float u = barycentricCoords[0];
                            Vector3 aBary = a * (u);
                            Vector3 bBary = b * (v);
                            closestPoint = aBary + bBary;
                            closestPoint *= (1.f / denom);
                            closestFeature[0] = simplex[0];
                            closestFeature[1] = simplex[1];
                            Vector3 ab = b - a;
                            planeNormalSA = Vector3TripleProduct(ab, q - a, ab);
                            break;
                        }
                        case 3:
                        {
                            Vector3 a = simplex[2];
                            Vector3 b = simplex[1];
                            Vector3 c = simplex[0];
                            float u = barycentricCoords[2];
                            float v = barycentricCoords[1];
                            float w = barycentricCoords[0];
                            Vector3 aBary = a * (u);
                            Vector3 bBary = b * (v);
                            Vector3 cBary = c * (w);
                            closestPoint = aBary + bBary + cBary;
                            closestPoint *= (1.f / denom);
                            closestFeature[0] = c;
                            closestFeature[1] = b;
                            closestFeature[2] = a;
                            Vector3 ab = b - a;
                            Vector3 ac = c - a;
                            if ((Vector3DotProduct(Vector3CrossProduct(ab, ac), Vector3Normalize(colliderPos - closestPoint)) > 0.f))
                            {
                                planeNormalSA = Vector3CrossProduct(ac, ab);
                            }
                            else
                            {
                                planeNormalSA = Vector3CrossProduct(ab, ac);
                            }
                            break;
                        }
                        case 4:
                        {
                            Vector3 a = simplex[3];
                            Vector3 b = simplex[2];
                            Vector3 c = simplex[1];
                            Vector3 d = simplex[0];
                            float u = barycentricCoords[3];
                            float v = barycentricCoords[2];
                            float w = barycentricCoords[1];
                            float x = barycentricCoords[0];
                            a = a * (u / denom);
                            b = b * (v / denom);
                            c = c * (w / denom);
                            d = d * (x / denom);
                            closestPoint = a + b + c + d;
                            break;
                        }
                        default: //invalid code path
                            ASSERT(false);
                            break;
                    }

                    float oldDistance = distance;
                    if (!hit)
                    {
                        Vector3 aq = q - closestFeature[0];
                        closestPoint = q - Vector3Project(aq, Vector3Normalize(planeNormalSA));
                        Vector3 closestPointToBallPos = q - closestPoint;
                        distance = Vector3Length(closestPointToBallPos);
                        distance -= ball->radius;
                        //closestPointOnBall = (ball->radius/Vector3Length(closestPointToBallPos)) * closestPointToBallPos + closestPoint;
                        closestPointOnBall = Vector3Normalize(closestPoint - q) + q;
                    }
                    else
                    {
                        distance = 0.f;
                        distance -= ball->radius;
                        break;
                    }
                    if ((oldDistance - distance) < EPSILON)
                    {
                        break;
                    }
                }

                // Analyze again after GJK is done
                float denom = 0;
                for (int i = 0; i < simplexCount; i++)
                {
                    denom += barycentricCoords[i];
                }
                if (denom <= 0.f)
                {
                    denom = 1.f;
                }
                switch (simplexCount)
                {
                case 1:
                {
                    closestPoint = simplex[0];
                    closestFeature[0] = simplex[0];
                    planeNormalSA = q - closestPoint;
                    break;
                }
                case 2:
                {
                    Vector3 a = simplex[1];
                    Vector3 b = simplex[0];
                    float v = barycentricCoords[1];
                    float u = barycentricCoords[0];
                    Vector3 aBary = a * (u);
                    Vector3 bBary = b * (v);
                    closestPoint = aBary + bBary;
                    closestPoint *= (1.f / denom);
                    closestFeature[0] = simplex[0];
                    closestFeature[1] = simplex[1];
                    Vector3 ab = b - a;
                    planeNormalSA = Vector3TripleProduct(ab, q - a, ab);
                    break;
                }
                case 3:
                {
                    Vector3 a = simplex[2];
                    Vector3 b = simplex[1];
                    Vector3 c = simplex[0];
                    float u = barycentricCoords[2];
                    float v = barycentricCoords[1];
                    float w = barycentricCoords[0];
                    Vector3 aBary = a * (u);
                    Vector3 bBary = b * (v);
                    Vector3 cBary = c * (w);
                    closestPoint = aBary + bBary + cBary;
                    closestPoint *= (1.f / denom);// THIS IS TRASH. Barycentric not precise enough. We compute this by projecting onto closest feature instead.
                    closestFeature[0] = c;
                    closestFeature[1] = b;
                    closestFeature[2] = a;
                    Vector3 ab = b - a;
                    Vector3 ac = c - a;
                    if ((Vector3DotProduct(Vector3CrossProduct(ab, ac), Vector3Normalize(colliderPos-closestPoint)) > 0.f))
                    {
                        planeNormalSA = Vector3CrossProduct(ac, ab);
                    }
                    else
                    {
                        planeNormalSA = Vector3CrossProduct(ab, ac);
                    }
                    break;
                }
                case 4:
                {
                    Vector3 a = simplex[3];
                    Vector3 b = simplex[2];
                    Vector3 c = simplex[1];
                    Vector3 d = simplex[0];
                    float u = barycentricCoords[3];
                    float v = barycentricCoords[2];
                    float w = barycentricCoords[1];
                    float x = barycentricCoords[0];
                    Vector3 aBary = a * (u / denom);
                    Vector3 bBary = b * (v / denom);
                    Vector3 cBary = c * (w / denom);
                    Vector3 dBary = d * (x / denom);
                    closestPoint = aBary + bBary + cBary + dBary;
                    break;
                }
                default: //invalid code path
                    ASSERT(false);
                    break;
                }
                
                if (!hit)
                {
                    Vector3 aq = q - closestFeature[0];
                    closestPoint = q - Vector3Project(aq, Vector3Normalize(planeNormalSA));
                    Vector3 closestPointToBallPos = q - closestPoint;
                    distance = Vector3Length(closestPointToBallPos);
                    distance -= ball->radius;
                    //closestPointOnBall = (ball->radius/Vector3Length(closestPointToBallPos)) * Vector3Negate(closestPointToBallPos) + q;
                    closestPointOnBall = Vector3Normalize(closestPoint - q) + q;
                }
                else
                {
                    distance = 0.f;
                    distance -= ball->radius;
                }

                pointOnBall = closestPointOnBall;
                TRUEclosestPoint = closestPoint;
                planeNormalSA = Vector3Normalize(planeNormalSA);
                pointToBall = closestPointOnBall - closestPoint;
                pointToBallDotPlaneNormal = Vector3DotProduct(closestPointOnBall - closestPoint, planeNormalSA);
                if ((hit) || (pointToBallDotPlaneNormal < 0.f) || distance < 0.f)
                {
                    //collidingNormal = planeNormalSA;
                    bracketEnd = bracketStart + bracketMid;
                    hitActive = true;
                }
                else if (((pointToBallDotPlaneNormal < PINBALL_EPSILON) && (pointToBallDotPlaneNormal > 0.f)) && ((distance < PINBALL_EPSILON) && (distance > 0.f)) && hitActive)
                {
                    //collidingNormal = planeNormalSA;
                    break;
                }
                else
                {
                    bracketStart = bracketStart + bracketMid;
                }
                bracketMid = (bracketEnd - bracketStart) / 2.f;
                q = Vector3Copy(ball->pos) + ((bracketStart+bracketMid) * playerDelta);

                if (bracketEnd - bracketStart < EPSILON)
                    int ending = 1;
            }

            if (hitActive)
            {
                if (bracketStart < tMin)
                {
                    tMin = bracketStart;
                    collidingNormal = planeNormalSA;
                    collidedObstacle = collider;
                }
            }
            else
            {
                tMin = 1.f;
            }

            // Undoing the matrix transform on the mesh vertices. Should probably just make a copy of the vertices instead of editing the mesh in place, then undoing the change.
            for (int vertIndex = 0; vertIndex < colliderMesh.vertexCount; ++vertIndex)
            {
                Vector3* vertex = colliderVertices + vertIndex;
                *vertex = Vector3Transform(*vertex, MatrixInvert(colliderTransform));
            }
#if 0
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
        if (hitActive)//(collision.hit && (len > collision.distance))
        {
            ball->pos += tMin * playerDelta;
            float bounceMultip = collidedObstacle.elasticity;
            ball->collisionRay = { closestPoint, closestPoint+collidingNormal };//ball->collisionRay = { collision.point,  collision.normal + collision.point };
            Vector3 velProjObst = Vector3Project(ball->velocity, collidingNormal);
            ball->velocity = ball->velocity - bounceMultip*velProjObst;
            ball->momentum = ball->velocity * ball->mass;

            playerDelta = desiredPos - ball->pos;
            Vector3 deltaProjObst = Vector3Project(playerDelta, collidingNormal);
            playerDelta = playerDelta - bounceMultip*deltaProjObst;
        }
        else
        {
            ball->collisionRay = { 0 };
            ball->pos += playerDelta;
            break;
        }
    }
    return force;
}

// Called on every frame
void Update(GameState *gameState)
{
    ++gameState->frameCount;
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
        Vector3 gravity = Vector3{ 0.0f,-386.f,0.0f } * ball->mass;
        // Instead of rotating every object, instead we only rotate the world/gravity so we can keep everything flat.
        gravity = Vector3RotateByAxisAngle(gravity, gameState->boardAxis, -gameState->boardAngle);
        accel[ballIndex] = MoveBall(gameState, ball, gameState->dt, gravity);

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
        //MoveBall(gameState, ball, gameState->dt, gravity);
    }
    Draw(gameState, simplex);
}

// Called when you recompile the program while its running
void HotReload(GameState *gameState)
{
    TraceLog(LOG_INFO, "HOTRELOAD");

    gameState->frameCount = 0;
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
    Obstacle nullObstacle = MakeObstacle(gameState, "", { 0 }, {0}, OBSTACLE_TYPE_NULL, 0.f, {0});
    MakeObstacle(gameState, "board", gameState->boardPos - Vector3{0,gameState->boardHeight,0},
                 gameState->boardDim, OBSTACLE_TYPE_BOX, 1.f, BROWN);

#if 1
    Vector3 sideWallDim = { 0.5f, 2.0f*gameState->boardHeight, gameState->boardLength };
    MakeObstacle(gameState, "wall 1", { -0.5f * gameState->boardWidth, -1.0f, 0.0f}, sideWallDim, OBSTACLE_TYPE_BOX, 2.f, RED);
    MakeObstacle(gameState, "wall 2", { 0.5f * gameState->boardWidth, -1.0f, 0.0f }, sideWallDim, OBSTACLE_TYPE_BOX, 2.f, BLUE);
    Vector3 vertWallDim = { gameState->boardWidth, 2.0f*gameState->boardHeight, 0.5f };
    MakeObstacle(gameState, "wall 3", { 0.0f, -1.0f, 0.5f*gameState->boardLength }, vertWallDim, OBSTACLE_TYPE_BOX, 2.f, GREEN);
    MakeObstacle(gameState, "wall 4", { 0.0f, -1.0f, -0.5f*gameState->boardLength }, vertWallDim, OBSTACLE_TYPE_BOX, 2.f, GRAY);

    for (int i = -2; i < 2; ++i)
    {
        Vector3 obstaclePosInBoard = { (float)i*4, 0, (float)i*4 };
        Vector3 obstacleSize = { 2,2,2 };
        ObstacleType type = OBSTACLE_TYPE_CYLINDER;
        if (i % 2 == 0)
        {
            type = OBSTACLE_TYPE_BOX;
        }
        MakeObstacle(gameState, "obst "+(char)i, obstaclePosInBoard, obstacleSize, type, 2.f, DARKPURPLE);
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
    TraceLog(LOG_INFO, "total frames elapsed: %d", gameState->frameCount);

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
