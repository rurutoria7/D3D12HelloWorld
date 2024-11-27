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
    float2 uv : TEXCOORD;
};

Texture2D<float4> g_texture : register(t0);

SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 normal : NORMAL, float4 uv : TEXCOORD)
{
    PSInput result;

    result.position = position;
    result.uv = float2(uv.x, 1.0 - uv.y);

    return result;
}

float4 PSMain(PSInput input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv);
}