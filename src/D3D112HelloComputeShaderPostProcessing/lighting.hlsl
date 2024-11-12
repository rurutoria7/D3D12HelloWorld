struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 u_mvp;
    float4x4 u_model;
    float4 u_ambient;
    float4 u_lightPos;
    float4 u_viewPos;
    float u_shines;
    float u_haveBumpTex;
    float u_haveSpecularTex;
    float u_haveAlphaTex;
};

Texture2D<float4> posTex : register(t0);
Texture2D<float4> normTex : register(t1);
Texture2D<float4> albedoTex : register(t2);
Texture2D<float4> specularTex : register(t3);

RWTexture2D<float4> renderTargetTex : register(u0);

[numthreads(16, 16, 1)]
void CSMain(ComputeShaderInput IN)
{
    float3 fragPos = posTex.Load(int3(IN.DispatchThreadID));
    float3 fragNorm = normTex.Load(int3(IN.DispatchThreadID));
    float3 fragAlbedo = albedoTex.Load(int3(IN.DispatchThreadID));
    float3 fragSpecular = specularTex.Load(int3(IN.DispatchThreadID));

    float3 lightDir = normalize(u_lightPos.xyz - fragPos);
    float3 viewDir = normalize(u_viewPos.xyz - fragPos);
    float3 hwayDir = normalize(lightDir + viewDir);
    
    float amb = u_ambient.x;
    float diff = max(dot(lightDir, fragNorm.xyz), 0);
    float spec = pow(max(dot(hwayDir, fragNorm), 0), u_shines) * fragSpecular;
    
    float4 outColor = float4((amb + diff + spec) * fragAlbedo, 0);
    renderTargetTex[int3(IN.DispatchThreadID).xy] = outColor;
}

