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
    };

private:
    static const INT MAX_INSTANCE_COUNT = 30;
    static const UINT FrameCount = 2;

    std::chrono::high_resolution_clock::time_point m_prevFrameTime;

    bool m_keys[256] = { false };


    // Pipeline objects
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    ComPtr<ID3D12Resource> m_swapChainRes[FrameCount];
    ComPtr<ID3D12Resource> m_gBufferPass_depthStencil;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_gBufferPass_rootSig;
    ComPtr<ID3D12DescriptorHeap> m_presentPass_rtvHeap; UINT m_rtvIncSize;
    ComPtr<ID3D12DescriptorHeap> m_gBufferPass_dsvHeap;
    ComPtr<ID3D12PipelineState> m_gBufferPass_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;


    // Render to texture resources
    ComPtr<ID3D12Resource> m_gBufferPass_vertexURes;
    ComPtr<ID3D12Resource> m_gBufferPass_vertexDRes;
    D3D12_VERTEX_BUFFER_VIEW m_gBufferPass_vbv;

    ComPtr<ID3D12Resource> m_gBufferPass_indexURes;
    ComPtr<ID3D12Resource> m_gBufferPass_indexDRes;
    D3D12_INDEX_BUFFER_VIEW m_gBufferPass_ibv[MAX_INSTANCE_COUNT];


    ComPtr<ID3D12Resource> m_gBufferPass_diffDRes[MAX_INSTANCE_COUNT];
    ComPtr<ID3D12Resource> m_gBufferPass_diffURes[MAX_INSTANCE_COUNT];
    ComPtr<ID3D12Resource> m_gBufferPass_constantsURes;
    ComPtr<ID3D12DescriptorHeap> m_gBufferPass_srvHeap;


    // Render from texture resources
    ComPtr<ID3D12PipelineState> m_presentPass_pipelineState;

    ComPtr<ID3D12Resource> m_presentPass_vertexURes;
    ComPtr<ID3D12Resource> m_presentPass_vertexDRes;
    D3D12_VERTEX_BUFFER_VIEW m_presentPass_vbv;

    ComPtr<ID3D12Resource> m_rt_textureRes_upload;
    ComPtr<ID3D12Resource> m_presentPass_frameDRes;
    ComPtr<ID3D12DescriptorHeap> m_presentPass_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_gBufferPass_rtvHeap;

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
    std::string m_modelDir = "sponza\\";
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
    void RecGBufferPassCmd(int);
    void RecPresentPassCmd();
    void WaitForPreviousFrame();
    void ResetCommandList();
    void SubmitCommandList();
};
