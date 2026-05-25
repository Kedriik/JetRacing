if(NormalizedAge != 0) return;

float3 OutPos;
float  OutScale;
bool   OutValid;
int    Index = ID.Index;
SpawnBuffer.ReadSpawnPosition(Index, OutPos, OutScale, OutValid);

Position  = OutPos;
Scale     = fmod(OutScale, 10.0);
MeshIndex = (int)(OutScale / 10.0);

static const float PlanetRadius = 16000000.0f;
static const float PI           = 3.14159265f;
static const float CellUV       = 100.0f / PlanetRadius * (2.0f / PI); // match compute shader

// -------------------------------------------------------------------------
// Project OutPos onto cube face — same logic as compute shader.
// -------------------------------------------------------------------------
float3 dir    = normalize(OutPos);
float3 absDir = abs(dir);

int    faceIndex;
float2 uv;
if (absDir.x >= absDir.y && absDir.x >= absDir.z)
{
    faceIndex = dir.x > 0 ? 0 : 1;
    float s   = 1.0f / absDir.x;
    uv = dir.x > 0 ? float2(-dir.z,  dir.y) * s
                   : float2( dir.z,  dir.y) * s;
}
else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
{
    faceIndex = dir.y > 0 ? 2 : 3;
    float s   = 1.0f / absDir.y;
    uv = dir.y > 0 ? float2( dir.x, -dir.z) * s
                   : float2( dir.x,  dir.z) * s;
}
else
{
    faceIndex = dir.z > 0 ? 4 : 5;
    float s   = 1.0f / absDir.z;
    uv = dir.z > 0 ? float2( dir.x,  dir.y) * s
                   : float2(-dir.x,  dir.y) * s;
}

// Global cell address on this face — matches gridSeed in compute shader.
int globalX = (int)floor(uv.x / CellUV);
int globalY = (int)floor(uv.y / CellUV);

float2 seed = float2(
    (float)globalX + (float)faceIndex * 100000.0f,
    (float)globalY + (float)faceIndex * 99999.0f
);

// -------------------------------------------------------------------------
// Build stable Up from cell-centre direction — immune to OutPos flicker.
// -------------------------------------------------------------------------
float2 cellCentreUV = float2(((float)globalX + 0.5f) * CellUV,
                              ((float)globalY + 0.5f) * CellUV);

float3 Up;
if      (faceIndex == 0) Up = normalize(float3( 1,  cellCentreUV.y, -cellCentreUV.x));
else if (faceIndex == 1) Up = normalize(float3(-1,  cellCentreUV.y,  cellCentreUV.x));
else if (faceIndex == 2) Up = normalize(float3( cellCentreUV.x,  1, -cellCentreUV.y));
else if (faceIndex == 3) Up = normalize(float3( cellCentreUV.x, -1,  cellCentreUV.y));
else if (faceIndex == 4) Up = normalize(float3( cellCentreUV.x,  cellCentreUV.y,  1));
else                     Up = normalize(float3(-cellCentreUV.x,  cellCentreUV.y, -1));

// Build tangent frame from stable Up
float3 Ref     = abs(Up.z) < 0.9f ? float3(0, 0, 1) : float3(1, 0, 0);
float3 Right   = normalize(cross(Ref, Up));
float3 Forward = cross(Up, Right);

// Stable random rotation around Up
float Hash  = frac(sin(dot(seed, float2(127.1f, 311.7f))) * 43758.5453f);
float Angle = Hash * 6.28318530f;
float CosA  = cos(Angle);
float SinA  = sin(Angle);

float3 RRight   = Right   * CosA + Forward * SinA;
float3 RForward = Forward * CosA - Right   * SinA;

float trace = RRight.x + RForward.y + Up.z;
float4 Q;
if (trace > 0)
{
    float s = 0.5f / sqrt(trace + 1.0f);
    Q = float4(
        (RForward.z - Up.y)        * s,
        (Up.x       - RRight.z)    * s,
        (RRight.y   - RForward.x)  * s,
        0.25f / s
    );
}
else if (RRight.x > RForward.y && RRight.x > Up.z)
{
    float s = 2.0f * sqrt(1.0f + RRight.x - RForward.y - Up.z);
    Q = float4(
        0.25f * s,
        (RRight.y   + RForward.x)  / s,
        (Up.x       + RRight.z)    / s,
        (RForward.z - Up.y)        / s
    );
}
else if (RForward.y > Up.z)
{
    float s = 2.0f * sqrt(1.0f + RForward.y - RRight.x - Up.z);
    Q = float4(
        (RRight.y   + RForward.x)  / s,
        0.25f * s,
        (RForward.z + Up.y)        / s,
        (Up.x       - RRight.z)    / s
    );
}
else
{
    float s = 2.0f * sqrt(1.0f + Up.z - RRight.x - RForward.y);
    Q = float4(
        (Up.x       + RRight.z)    / s,
        (RForward.z + Up.y)        / s,
        0.25f * s,
        (RRight.y   - RForward.x)  / s
    );
}
Rotation = normalize(Q);
