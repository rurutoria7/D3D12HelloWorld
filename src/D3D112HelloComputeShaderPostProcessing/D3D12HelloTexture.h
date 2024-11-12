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

#pragma once

#include "DXSample.h"
#include "WaveFrontReader.h"
#include <locale>
#include <codecvt>
#include <string>
#include <chrono>
#include <algorithm>
#include "MyCamera.h"



using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12HelloTexture : public DXSample
{
public:
    D3D12HelloTexture(UINT width, UINT height, std::wstring name);

    void OnInit() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnDestroy() override;
    void OnKeyDown(UINT8 key) override;
    void OnKeyUp(UINT8 key) override;

    struct GBufferPassUniforms {
        XMMATRIX u_mvp;
        XMMATRIX u_model;
        XMFLOAT4 u_ambient;
        XMFLOAT4 u_lightPos;
        XMFLOAT4 u_viewPos;
        float u_shines;
        float u_haveBumpTex;
        float u_haveSpecularTex;
        float u_haveAlphaTex;
    };

private:
    static const INT MAX_INSTANCE_COUNT = 30;
    static const UINT FrameCount = 2;

    std::chrono::high_resolution_clock::time_point m_prevFrameTime;

    bool m_keys[256] = { false };

    // uniforms
    XMFLOAT4 m_uAmbient;
    XMFLOAT4 m_uLightPos;
    XMFLOAT4 m_uViewPos;
    float m_uShines;
   
    // D3D12 Component
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12Resource> m_swapChainRes[FrameCount];

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    // ------------ pass 1: gBuffer ------------------------//
    ComPtr<ID3D12RootSignature> m_gBufferPass_rootSig;
    ComPtr<ID3D12PipelineState> m_gBufferPass_pipelineState;
    // depth stencil
    ComPtr<ID3D12Resource> m_gBufferPass_depthStencil;
    ComPtr<ID3D12DescriptorHeap> m_gBufferPass_dsvHeap;
    // vertices
    ComPtr<ID3D12Resource> m_gBufferPass_vertexURes;
    ComPtr<ID3D12Resource> m_gBufferPass_vertexDRes;
    D3D12_VERTEX_BUFFER_VIEW m_gBufferPass_vbv;
    // indices
    ComPtr<ID3D12Resource> m_gBufferPass_indexDRes;
    ComPtr<ID3D12Resource> m_gBufferPass_indexURes;
    D3D12_INDEX_BUFFER_VIEW m_gBufferPass_ibv[MAX_INSTANCE_COUNT];
    // textures
    ComPtr<ID3D12DescriptorHeap> m_gBufferPass_srvHeap;                 // N = MAX_INSTANCE_COUNT
    ComPtr<ID3D12Resource> m_gBufferPass_diffDRes[MAX_INSTANCE_COUNT];  // srv: 0 ... N-1
    ComPtr<ID3D12Resource> m_gBufferPass_diffURes[MAX_INSTANCE_COUNT];
    ComPtr<ID3D12Resource> m_gBufferPass_specDRes[MAX_INSTANCE_COUNT];  // srv: N ... 2N - 1
    ComPtr<ID3D12Resource> m_gBufferPass_specURes[MAX_INSTANCE_COUNT];
    ComPtr<ID3D12Resource> m_gBufferPass_bumpDRes[MAX_INSTANCE_COUNT];  // srv: 2N ... 3N-1
    ComPtr<ID3D12Resource> m_gBufferPass_bumpURes[MAX_INSTANCE_COUNT];
    ComPtr<ID3D12Resource> m_gBufferPass_alphaDRes[MAX_INSTANCE_COUNT]; // srv: 3N ... 4N-1
    ComPtr<ID3D12Resource> m_gBufferPass_alphaURes[MAX_INSTANCE_COUNT];
    // constants
    ComPtr<ID3D12Resource> m_gBufferPass_constantsURes;
    // render target
    ComPtr<ID3D12DescriptorHeap> m_gBufferPass_rtvHeap; // ---> m_lightingPass_gBufferDRes

    // ------------ pass 2: lighting ------------------------//
    ComPtr<ID3D12PipelineState> m_lightingPass_pipelineState;
    ComPtr<ID3D12RootSignature> m_lightingPass_rootSig;
    //  GBuffer
    ComPtr<ID3D12Resource> m_lightingPass_gPosDRes;
    ComPtr<ID3D12Resource> m_lightingPass_gNormDRes;
    ComPtr<ID3D12Resource> m_lightingPass_gAlbedoDRes;
    ComPtr<ID3D12Resource> m_lightingPass_gSpecularDRes;
    ComPtr<ID3D12DescriptorHeap> m_lightingPass_srvUavHeap; // 0 ... 3: GBuffer, 4 := UAV Rendering target

    // ------------ pass 3: present ------------------------//
    ComPtr<ID3D12PipelineState> m_presentPass_pipelineState;
    // vertices (quad)
    ComPtr<ID3D12Resource> m_presentPass_vertexURes;
    ComPtr<ID3D12Resource> m_presentPass_vertexDRes;
    D3D12_VERTEX_BUFFER_VIEW m_presentPass_vbv;
    // frame buffer
    ComPtr<ID3D12Resource> m_presentPass_frameDRes;
    ComPtr<ID3D12DescriptorHeap> m_presentPass_srvHeap;
    // render target
    ComPtr<ID3D12DescriptorHeap> m_presentPass_rtvHeap; // ---> m_swapChainDRes
    UINT m_rtvIncSize;
    UINT m_cbvSrvUavIncSize;


    // Assets
    struct Instance
    {
        uint32_t offset;
        uint32_t size;
        uint32_t material_id;
    };
    std::vector<int> m_IndicesIdx;
    std::vector<Instance> m_instance;
    std::vector<uint32_t> m_sorted_inds;
    WaveFrontReader<uint32_t> m_wfReader;
    typedef WaveFrontReader<uint32_t>::Vertex Vertex;
    XMMATRIX m_model;
    XMMATRIX m_mvp;
    MyCamera m_camera;

    float m_scale = 0.03f;
    std::string m_modelDir = "D:\\Documents\\2024_FALL\\D3D\\DirectX-Graphics-Samples\\Samples\\Desktop\\D3D12HelloWorld\\src\\Assets\\sponza\\";
    std::string m_modelName = "sponza.obj";

    D3D12_INDEX_BUFFER_VIEW GetInstanceVBV(UINT ins_id) { return m_gBufferPass_ibv[ins_id]; }

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void LoadPipeline();
    void InitGBufferPass();
    void InitPresentPass();
    void InitlightingPass();
    void RecGBufferPassCmd(int);
    void RecPresentPassCmd();
    void RecLightingPassCmd();
    void WaitForPreviousFrame();
    void ResetCommandList();
    void SubmitCommandList();
};
