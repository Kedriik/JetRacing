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
static const float CellUV       = 100.0f / PlanetRadius * (2.0f / PI);

// -------------------------------------------------------------------------
// Project OutPos onto cube face — for seed only.
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


// Snap using plain CellUV — matches compute shader exactly.
int globalX = (int)floor(uv.x / CellUV);
int globalY = (int)floor(uv.y / CellUV);

float2 seed = float2(
    (float)globalX + (float)faceIndex * 100000.0f,
    (float)globalY + (float)faceIndex * 99999.0f
);


// -------------------------------------------------------------------------
// Build Up directly from OutPos — no cell grid involved, no boundary hops.
// OutPos is placed by the compute shader at the terrain surface point.
// Its direction IS the radial outward normal — exactly what we need for Up.
// We snap it to a coarse angular grid to kill sub-unit float noise.
// -------------------------------------------------------------------------
static const float SnapAngle = 0.0001f; // ~1600m at 16M radius, sub-cell snapping
float3 rawUp   = normalize(OutPos);
float3 snappedUp = normalize(float3(
    floor(rawUp.x / SnapAngle + 0.5f) * SnapAngle,
    floor(rawUp.y / SnapAngle + 0.5f) * SnapAngle,
    floor(rawUp.z / SnapAngle + 0.5f) * SnapAngle
));
float3 Up = snappedUp;

// Build tangent frame from stable Up
float3 Ref     = abs(Up.z) < 0.9f ? float3(0, 0, 1) : float3(1, 0, 0);
float3 Right   = normalize(cross(Ref, Up));
float3 Forward = cross(Up, Right);

// Derive rotation seed from world position snapped to 10-unit grid.
// Snapping kills sub-unit float noise — same physical cell always gives same seed.
float3 snappedPos = floor(OutPos / 10.0f) * 10.0f;
float2 rotSeed    = float2(
    snappedPos.x + snappedPos.z * 3001.0f,
    snappedPos.y + snappedPos.z * 3001.0f
);

// Stable random rotation around Up
float Hash  = frac(sin(dot(rotSeed, float2(127.1f, 311.7f))) * 43758.5453f);
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
