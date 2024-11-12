//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
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

Texture2D g_texture : register(t0);
Texture2D g_specular : register(t1);
Texture2D g_bump : register(t2);

SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 normal : NORMAL, float4 uv : TEXCOORD)
{
    PSInput result;

    result.position = mul(position, u_mvp);
    result.worldPos = mul(position, u_model);
    result.uv = float2(uv.x, 1.0 - uv.y);
    result.normal = normal;

    return result;
}

struct PSOutput
{
    float4 gPos : SV_Target0;
    float4 gNorm : SV_Target1;
    float4 gAlbedo : SV_Target2;
    float4 gSpecular : SV_Target3;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    output.gAlbedo = g_texture.Sample(g_sampler, input.uv);
    output.gNorm = normalize(input.normal);
    output.gPos = input.worldPos;
    
    if (u_haveSpecularTex > 0.5)
        output.gSpecular = g_specular.Sample(g_sampler, input.uv);
    else
        output.gSpecular = float4(0, 0, 0, 0);
    
    return output;
}