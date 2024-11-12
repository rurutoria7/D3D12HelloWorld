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
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 g_mvp;
    float4 g_diffuse;
    float4 g_ambient;
    float4 g_specular;
    float4 g_emissive;
    float g_shininess;
    float g_alpha;
    float g_use_texture;
    float g_use_lighting;
    float g_use_specular;
    float g_use_emissive;
    float2 padding;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 normal : NORMAL, float4 uv : TEXCOORD)
{
    PSInput result;

    result.position = mul(position, g_mvp);
    result.position = position;
    result.uv = float2(uv.x, 1.0 - uv.y);
    result.normal = normal;

    return result;
}

//float4 PSMain(PSInput input) : SV_TARGET
//{
//    float4 color = g_texture.Sample(g_sampler, input.uv);
//    return color;
//}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 dcolor = g_texture.Sample(g_sampler, input.uv).rgb;
    
    return float4(dcolor, 1.0);
}