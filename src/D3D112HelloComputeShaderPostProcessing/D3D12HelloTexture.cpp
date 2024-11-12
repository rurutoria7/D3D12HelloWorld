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

#include "stdafx.h"
#include "D3D12HelloTexture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <sstream>

void* GenTextureData(int width, int height)
{
    int texWidth = width;
    int texHeight = height;
    int texChannels = 4;
    int texSize = texWidth * texHeight * texChannels;
    unsigned char* texture = new unsigned char[texSize];

    for (int i = 0; i < texSize; i += 4)
    {
        if ( (i/16) & 1 )
        {
            texture[i] = 255;
            texture[i + 1] = 255;
            texture[i + 2] = 255;
            texture[i + 3] = 255;
            continue;
        }
        texture[i] = 255;
        texture[i + 1] = 0;
        texture[i + 2] = 0;
        texture[i + 3] = 255;
    }

    return texture;
}

std::string ConvertWstringToString(wchar_t* wstring)
{
    std::stringstream ss;
    for (int i = 0; wstring[i]; i++)
    {
        char cc = (char)wstring[i];
        ss << cc;
    }
    OutputDebugStringA(ss.str().c_str());
    OutputDebugStringA("\n");
    std::string res = ss.str();
    return res;
}

D3D12HelloTexture::D3D12HelloTexture(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvIncSize(0)
{
}

void D3D12HelloTexture::OnInit()
{    
    m_model = XMMatrixScaling(m_scale, m_scale, m_scale);
    m_camera.Init(m_aspectRatio);
    LoadPipeline();

    InitGBufferPass();
    InitPresentPass();
    InitlightingPass();
}

void D3D12HelloTexture::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    // Create DXGI factory
    {
        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

        if (m_useWarpDevice)
        {
            ComPtr<IDXGIAdapter> warpAdapter;
            ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

            ThrowIfFailed(D3D12CreateDevice(
                warpAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&m_device)
                ));
        }
        else
        {
            ComPtr<IDXGIAdapter1> hardwareAdapter;
            GetHardwareAdapter(factory.Get(), &hardwareAdapter);

            ThrowIfFailed(D3D12CreateDevice(
                hardwareAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&m_device)
                ));
        }
    }

    // Set descriptor handle increment size
    {
        m_rtvIncSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_cbvSrvUavIncSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Create command queue.
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    }

    // Create swap chain.
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = FrameCount;
        swapChainDesc.Width = m_width;
        swapChainDesc.Height = m_height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
            Win32Application::GetHwnd(),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain
            ));

        // This sample does not support fullscreen transitions.
        ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

        ThrowIfFailed(swapChain.As(&m_swapChain));
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_presentPass_rtvHeap)));

        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = MAX_INSTANCE_COUNT * 4;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_gBufferPass_srvHeap)));

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc2 = {};
        srvHeapDesc2.NumDescriptors = 1;
        srvHeapDesc2.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc2.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc2, IID_PPV_ARGS(&m_presentPass_srvHeap)));

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc2 = {};
        rtvHeapDesc2.NumDescriptors = 4;
        rtvHeapDesc2.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc2.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc2, IID_PPV_ARGS(&m_gBufferPass_rtvHeap)));

    }

    // Create frame buffer
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_presentPass_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_swapChainRes[n])));
            m_device->CreateRenderTargetView(m_swapChainRes[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvIncSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void D3D12HelloTexture::InitGBufferPass()
{
    // create command list
    {
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_gBufferPass_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
    }

    // Load wavefront
    {
        std::string filename = m_modelDir + m_modelName;;
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring filenameW = converter.from_bytes(filename);

        HRESULT hr = m_wfReader.Load(filenameW.c_str());
    }

    // create root signature
    {
        /*
        * slot, type, comment, register
        * 0, cbv, uniform, b0
        * 1, srvTable, diffusion, t0
        * 2, srvTable, specular, t1
        * 3, srvTable, bump, t2
        */
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[4];
        rootParameters[0].InitAsConstantBufferView(0, 0);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);


        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_gBufferPass_rootSig)));
    }

    // Create pipeline state
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_gBufferPass_rootSig.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 4;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        psoDesc.RTVFormats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        psoDesc.RTVFormats[3] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_gBufferPass_pipelineState)));
    }

    // Create vertex resource
    {
        const UINT vertexBufferSize = m_wfReader.GetVerticesSize();

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_gBufferPass_vertexURes)));

        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);     
        ThrowIfFailed(m_gBufferPass_vertexURes->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, m_wfReader.vertices.data(), vertexBufferSize );
        m_gBufferPass_vertexURes->Unmap(0, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_gBufferPass_vertexDRes)));

        m_commandList->CopyBufferRegion(m_gBufferPass_vertexDRes.Get(), 0, m_gBufferPass_vertexURes.Get(), 0, vertexBufferSize);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferPass_vertexDRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
    }

    // Create indices resource
    {
        auto& attr = m_wfReader.attributes;
        auto& inds = m_wfReader.indices;

        for (int i = 0; i < inds.size(); i++) m_IndicesIdx.push_back(i);
        std::stable_sort(m_IndicesIdx.begin(), m_IndicesIdx.end(), [&](int a, int b) { return attr[a/3] < attr[b/3]; });

        for (int i = 0; i < m_IndicesIdx.size(); i++) {
            if (m_instance.empty() || m_instance.back().material_id != attr[m_IndicesIdx[i]/3])
                m_instance.push_back({ static_cast<uint32_t>(i), 1, attr[m_IndicesIdx[i] / 3]});
            else m_instance.back().size++;
            m_sorted_inds.push_back(inds[m_IndicesIdx[i]]);
        }

        const UINT indicesBufferSize = m_wfReader.GetIndicesSize();

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indicesBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_gBufferPass_indexURes)));

        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_gBufferPass_indexURes->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, m_sorted_inds.data(), indicesBufferSize);
        m_gBufferPass_indexURes->Unmap(0, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indicesBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_gBufferPass_indexDRes)));
        
        m_commandList->CopyBufferRegion(m_gBufferPass_indexDRes.Get(), 0, m_gBufferPass_indexURes.Get(), 0, indicesBufferSize);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferPass_indexDRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
    }

    // Create GBuffer pass uniforms resource
    {
        const UINT constantBufferSize = sizeof(D3D12HelloTexture::GBufferPassUniforms);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_gBufferPass_constantsURes)));
    }

    // Create diffusion resource
    {
        D3D12_SUBRESOURCE_DATA* textureData = new D3D12_SUBRESOURCE_DATA[m_instance.size()];

        for (int i = 0; i < m_instance.size(); i++)
        {
            auto& mat = m_wfReader.materials[m_instance[i].material_id];

            if (mat.strTexture[0] == L'\0') continue;

            // stbi load image
            int texWidth, texHeight, texChannels;
            std::string filename = m_modelDir + ConvertWstringToString(mat.strTexture);
            UINT8* texture = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texWidth;
            textureDesc.Height = texHeight;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_gBufferPass_diffDRes[i])));

            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_gBufferPass_diffDRes[i].Get(), 0, 1);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_gBufferPass_diffURes[i])));

            textureData[i].pData = &texture[0];
            textureData[i].RowPitch = texWidth * 4;
            textureData[i].SlicePitch = textureData[i].RowPitch * texHeight;

            UpdateSubresources(m_commandList.Get(), m_gBufferPass_diffDRes[i].Get(), m_gBufferPass_diffURes[i].Get(), 0, 0, 1, &textureData[i]);
            m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferPass_diffDRes[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            auto srvHandle = m_gBufferPass_srvHeap->GetCPUDescriptorHandleForHeapStart();
            srvHandle.ptr += i * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_device->CreateShaderResourceView(m_gBufferPass_diffDRes[i].Get(), &srvDesc, srvHandle);
        }

        delete[] textureData;
    }
    
    // Create specular resource
    {
        D3D12_SUBRESOURCE_DATA* textureData = new D3D12_SUBRESOURCE_DATA[m_instance.size()];

        for (int i = 0; i < m_instance.size(); i++)
        {
            auto& mat = m_wfReader.materials[m_instance[i].material_id];

            if (mat.strSpecularTexture[0] == L'\0') continue;

            // stbi load image
            int texWidth, texHeight, texChannels;
            std::string filename = m_modelDir + ConvertWstringToString(mat.strSpecularTexture);
            UINT8* texture = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texWidth;
            textureDesc.Height = texHeight;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_gBufferPass_specDRes[i])));

            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_gBufferPass_specDRes[i].Get(), 0, 1);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_gBufferPass_specURes[i])));

            textureData[i].pData = &texture[0];
            textureData[i].RowPitch = texWidth * 4;
            textureData[i].SlicePitch = textureData[i].RowPitch * texHeight;

            UpdateSubresources(m_commandList.Get(), m_gBufferPass_specDRes[i].Get(), m_gBufferPass_specURes[i].Get(), 0, 0, 1, &textureData[i]);
            m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferPass_specDRes[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            auto srvHandle = m_gBufferPass_srvHeap->GetCPUDescriptorHandleForHeapStart();
            srvHandle.ptr += (i+ MAX_INSTANCE_COUNT) * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_device->CreateShaderResourceView(m_gBufferPass_specDRes[i].Get(), &srvDesc, srvHandle);
        }

        delete[] textureData;
    }    
    
    // Create bump resource
    {
        D3D12_SUBRESOURCE_DATA* textureData = new D3D12_SUBRESOURCE_DATA[m_instance.size()];

        for (int i = 0; i < m_instance.size(); i++)
        {
            auto& mat = m_wfReader.materials[m_instance[i].material_id];

            if (mat.strBump[0] == L'\0') continue;

            // stbi load image
            int texWidth, texHeight, texChannels;
            std::string filename = m_modelDir + ConvertWstringToString(mat.strBump);
            UINT8* texture = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texWidth;
            textureDesc.Height = texHeight;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_gBufferPass_bumpDRes[i])));

            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_gBufferPass_bumpDRes[i].Get(), 0, 1);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_gBufferPass_bumpURes[i])));

            textureData[i].pData = &texture[0];
            textureData[i].RowPitch = texWidth * 4;
            textureData[i].SlicePitch = textureData[i].RowPitch * texHeight;

            UpdateSubresources(m_commandList.Get(), m_gBufferPass_bumpDRes[i].Get(), m_gBufferPass_bumpURes[i].Get(), 0, 0, 1, &textureData[i]);
            m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferPass_bumpDRes[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            auto srvHandle = m_gBufferPass_srvHeap->GetCPUDescriptorHandleForHeapStart();
            srvHandle.ptr += (i + MAX_INSTANCE_COUNT*2) * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_device->CreateShaderResourceView(m_gBufferPass_bumpDRes[i].Get(), &srvDesc, srvHandle);
        }

        delete[] textureData;
    }

    // Create depth buffer
    {
        D3D12_RESOURCE_DESC depthStencilDesc = {};
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = m_width;
        depthStencilDesc.Height = m_height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.SampleDesc.Count = 1;
        depthStencilDesc.SampleDesc.Quality = 0;
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = DXGI_FORMAT_D32_FLOAT;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optClear,
            IID_PPV_ARGS(&m_gBufferPass_depthStencil)
        ));

        SetName(m_gBufferPass_depthStencil.Get(), L"DepthStencil");

        // create dsv heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_gBufferPass_dsvHeap)));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        m_device->CreateDepthStencilView(m_gBufferPass_depthStencil.Get(), &dsvDesc, m_gBufferPass_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Execute command list
    {
        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    }

    // Create fence
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        WaitForPreviousFrame();
    }

    // Create vbv & ibv
    {
        const UINT vertexBufferSize = m_wfReader.GetVerticesSize();
        m_gBufferPass_vbv.BufferLocation = m_gBufferPass_vertexDRes->GetGPUVirtualAddress();
        m_gBufferPass_vbv.StrideInBytes = sizeof(WaveFrontReader<uint32_t>::Vertex);
        m_gBufferPass_vbv.SizeInBytes = vertexBufferSize;

        for (int i = 0; i < m_instance.size(); i++)
        {
            m_gBufferPass_ibv[i].BufferLocation =  m_gBufferPass_indexDRes->GetGPUVirtualAddress() + (uint64_t)m_instance[i].offset * sizeof(uint32_t);
            m_gBufferPass_ibv[i].SizeInBytes = m_instance[i].size * sizeof(uint32_t);
            m_gBufferPass_ibv[i].Format = DXGI_FORMAT_R32_UINT;
        }
    }
}

void D3D12HelloTexture::InitPresentPass()
{
    ResetCommandList();

    // Create PSO
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"present.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"present.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_gBufferPass_rootSig.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_NONE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_presentPass_pipelineState)));
    }

    // Create vertex buffer
    {
        const UINT vertexBufferSize = sizeof(Vertex) * 6;

        Vertex vertices[6] =
        {
            { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
            { { 1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
            { { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
            { { 1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
            { { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
            { { 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } }
        };

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_presentPass_vertexURes)));

        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_presentPass_vertexURes->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, &vertices, vertexBufferSize);
        m_presentPass_vertexURes->Unmap(0, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_presentPass_vertexDRes)));

        m_commandList->CopyBufferRegion(m_presentPass_vertexDRes.Get(), 0, m_presentPass_vertexURes.Get(), 0, vertexBufferSize);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_presentPass_vertexDRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
    }

    // Create texture buffer
    {
        D3D12_SUBRESOURCE_DATA *textureData = new D3D12_SUBRESOURCE_DATA();

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = m_width;
        textureDesc.Height = m_height;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_presentPass_frameDRes)));

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_presentPass_frameDRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // create texture srv
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        auto srvHandle = m_presentPass_srvHeap->GetCPUDescriptorHandleForHeapStart();

        m_device->CreateShaderResourceView(m_presentPass_frameDRes.Get(), &srvDesc, srvHandle);


    }

    SubmitCommandList();

    // create vbv & ibv
    {
        const UINT vertexBufferSize = sizeof(Vertex) * 6;
        m_presentPass_vbv.BufferLocation = m_presentPass_vertexDRes->GetGPUVirtualAddress();
        m_presentPass_vbv.StrideInBytes = sizeof(Vertex);
        m_presentPass_vbv.SizeInBytes = vertexBufferSize;
    }
}

void D3D12HelloTexture::InitlightingPass()
{
    // create descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
        srvUavHeapDesc.NumDescriptors = 5;
        srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_lightingPass_srvUavHeap)));
    }

    // create root sig
    {
        /*
        * slot, type, comment, register
        * 0, cbv, uniform, b0
        * 1, srvTable, diffusion, t0
        * 2, srvTable, specular, t1
        * 3, srvTable, bump, t2
        * 4, uavTable, renderTarget, t3
        */
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[6];
        rootParameters[0].InitAsConstantBufferView(0);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[2]);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[3]);
        rootParameters[5].InitAsDescriptorTable(1, &ranges[4]);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(),signature->GetBufferSize(),
            IID_PPV_ARGS(&m_lightingPass_rootSig)));
    }

    // create pipeline state
    {
        ComPtr<ID3DBlob> computeShader;
        ComPtr<ID3DBlob> errorBlob;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"lighting.hlsl").c_str(), nullptr, nullptr, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, &errorBlob));

        D3D12_COMPUTE_PIPELINE_STATE_DESC psDesc = {};
        psDesc.pRootSignature = m_lightingPass_rootSig.Get();
        psDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
        psDesc.NodeMask = 0;
        psDesc.CachedPSO = { nullptr, 0 };
        psDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        m_device->CreateComputePipelineState(&psDesc, IID_PPV_ARGS(&m_lightingPass_pipelineState));
    }

    // create texture resource
    {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.Width = m_width;
        textureDesc.Height = m_height;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_lightingPass_gPosDRes)));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_lightingPass_gNormDRes)));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_lightingPass_gAlbedoDRes)));
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_lightingPass_gSpecularDRes)));

        // create gBuffer pass rtv
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = textureDesc.Format;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;

            // gPos, gNorm, gAlbedo, gSpecular
            // 0, 1, 2, 3
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_gBufferPass_rtvHeap->GetCPUDescriptorHandleForHeapStart());
            m_device->CreateRenderTargetView(m_lightingPass_gPosDRes.Get(), &rtvDesc, rtvHandle);
            rtvHandle.Offset(1, m_rtvIncSize);
            m_device->CreateRenderTargetView(m_lightingPass_gNormDRes.Get(), &rtvDesc, rtvHandle);
            rtvHandle.Offset(1, m_rtvIncSize);
            m_device->CreateRenderTargetView(m_lightingPass_gAlbedoDRes.Get(), &rtvDesc, rtvHandle);
            rtvHandle.Offset(1, m_rtvIncSize);
            m_device->CreateRenderTargetView(m_lightingPass_gSpecularDRes.Get(), &rtvDesc, rtvHandle);
        }

        // create lighting pass srv
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_lightingPass_srvUavHeap->GetCPUDescriptorHandleForHeapStart());
            m_device->CreateShaderResourceView(m_lightingPass_gPosDRes.Get(), &srvDesc, srvHandle);
            srvHandle.Offset(1, m_cbvSrvUavIncSize);
            m_device->CreateShaderResourceView(m_lightingPass_gNormDRes.Get(), &srvDesc, srvHandle);
            srvHandle.Offset(1, m_cbvSrvUavIncSize);
            m_device->CreateShaderResourceView(m_lightingPass_gAlbedoDRes.Get(), &srvDesc, srvHandle);
            srvHandle.Offset(1, m_cbvSrvUavIncSize);
            m_device->CreateShaderResourceView(m_lightingPass_gSpecularDRes.Get(), &srvDesc, srvHandle);
        }

        // create lighting pass uav
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = 0;
            uavDesc.Texture2D.PlaneSlice = 0;

            CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_lightingPass_srvUavHeap->GetCPUDescriptorHandleForHeapStart(),
                4, m_cbvSrvUavIncSize);
            m_device->CreateUnorderedAccessView(m_presentPass_frameDRes.Get(), nullptr, &uavDesc, uavHandle);
        }
    }
}

void D3D12HelloTexture::OnUpdate()
{
    auto nw = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(nw - m_prevFrameTime).count();
    m_prevFrameTime = nw;

    m_camera.Update(m_keys, deltaTime);
    m_mvp = m_model * m_camera.GetViewProj();

    XMStoreFloat4(&m_uViewPos, m_camera.m_position);
    //XMStoreFloat4(&m_uLightPos, m_camera.m_position);
    XMStoreFloat4(&m_uLightPos, XMVectorSet(-13.0f, 3.0f, 0.0f, 1.0f));
    m_uAmbient = XMFLOAT4(0.1, 0.0, 0.0, 1);
    m_uShines = 64;
}

void D3D12HelloTexture::OnRender()
{
    WaitForPreviousFrame();

    // gBuffer pass: clear render target and depth stencil, set rtv resource state
    {
        ResetCommandList();
        
        // gBuffer pass: clear rt & ds
        m_commandList->ClearDepthStencilView(m_gBufferPass_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        
        // gBuffer pass: set render target (lightingPass_gBuffer) resource state
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gPosDRes.Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gNormDRes.Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gAlbedoDRes.Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gSpecularDRes.Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        
        SubmitCommandList();
    }

    // gBuffer pass: render
    {
        for (int i = 0; i < m_instance.size(); i++)
        {
            ResetCommandList();
            RecGBufferPassCmd(i);
            SubmitCommandList();
        }
    }

    // lighting pass: set resource state
    //      lighting pass gBuffers: RENDER_TARGET -> ALL_SHADER_RESOURCE
    //      present pass frame: ALL_SHADER_RESOURCE -> UNORDERED_ACCESS
    {
        ResetCommandList();
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gPosDRes.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gNormDRes.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gAlbedoDRes.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_lightingPass_gSpecularDRes.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_presentPass_frameDRes.Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        
        SubmitCommandList();
    }

    // light pass: dispatch compute
    {
        ResetCommandList();
        RecLightingPassCmd();
        SubmitCommandList();
    }

    // present pass: set resource state
    //      swap chain: PRESENT -> RENDER_TARGET
    //      present pass frame: UNORDERED_ACCESS -> ALL_SHADER_RESOURCE
    {
        ResetCommandList();
        
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_swapChainRes[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET));

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_presentPass_frameDRes.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
        ));

        SubmitCommandList();
    }

    // present pass: render
    {
        ResetCommandList();
        RecPresentPassCmd();
        SubmitCommandList();
    }

    // set swap chain resource state
    //      swap chain: RENDER_TARGET -> PRESENT
    {
        ResetCommandList();
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_swapChainRes[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT));
        SubmitCommandList();
    }

    ThrowIfFailed(m_swapChain->Present(1, 0));
}

void D3D12HelloTexture::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void D3D12HelloTexture::RecGBufferPassCmd(int instance_id)
{ 
    m_commandList->SetPipelineState(m_gBufferPass_pipelineState.Get());

    // Set descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_gBufferPass_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    
    // Set root arguments
    m_commandList->SetGraphicsRootSignature(m_gBufferPass_rootSig.Get());
    auto& mat = m_wfReader.materials[m_instance[instance_id].material_id];
    GBufferPassUniforms cb;
    cb.u_mvp = XMMatrixTranspose(m_mvp);
    cb.u_model = XMMatrixTranspose(m_model);
    cb.u_ambient = m_uAmbient;
    cb.u_lightPos = m_uLightPos;
    cb.u_viewPos = m_uViewPos;
    cb.u_shines = m_uShines;

    auto srvHeapStart = m_gBufferPass_srvHeap->GetGPUDescriptorHandleForHeapStart();
    auto srvIncSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (mat.strTexture[0] != L'\0')
    {
        UINT64 offset = instance_id * srvIncSize;
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(srvHeapStart, offset);
        m_commandList->SetGraphicsRootDescriptorTable(1, srvHandle);
    }
    if (mat.strSpecularTexture[0] != L'\0')
    {
        UINT64 offset = (MAX_INSTANCE_COUNT + instance_id) * srvIncSize;
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(srvHeapStart, offset);
        m_commandList->SetGraphicsRootDescriptorTable(2, srvHandle);
        cb.u_haveSpecularTex = 1;
    }
    if (mat.strBump[0] != L'\0')
    {
        UINT64 offset = (MAX_INSTANCE_COUNT * 2 + instance_id) * srvIncSize;
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(srvHeapStart, offset);
        m_commandList->SetGraphicsRootDescriptorTable(3, srvHandle);
        cb.u_haveBumpTex = 1;
    }

    UINT8* pConstantDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(m_gBufferPass_constantsURes->Map(0, &readRange, reinterpret_cast<void**>(&pConstantDataBegin)));
    memcpy(pConstantDataBegin, &cb, sizeof(cb));
    m_gBufferPass_constantsURes->Unmap(0, nullptr);

    m_commandList->SetGraphicsRootConstantBufferView(0, m_gBufferPass_constantsURes->GetGPUVirtualAddress());

    // Set OM
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_gBufferPass_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_gBufferPass_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->OMSetRenderTargets(4, &rtvHandle, TRUE, &dsvHandle);

    // Set RS
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Set IA
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_gBufferPass_vbv);
    m_commandList->IASetIndexBuffer(&m_gBufferPass_ibv[instance_id]);

    // Draw
    m_commandList->DrawIndexedInstanced(m_instance[instance_id].size, 1, 0, 0, 0);
}

void D3D12HelloTexture::RecLightingPassCmd()
{
    // set pipeline state, set descriptor heap
    {
        m_commandList->SetPipelineState(m_lightingPass_pipelineState.Get());

        ID3D12DescriptorHeap* ppHeaps[] = { m_lightingPass_srvUavHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, ppHeaps);
    }

    // set root arguments
    {
        // uniforms are same as gBuffer pass
        m_commandList->SetComputeRootSignature(m_lightingPass_rootSig.Get());
        m_commandList->SetComputeRootConstantBufferView(0, m_gBufferPass_constantsURes->GetGPUVirtualAddress());

        auto hs = m_lightingPass_srvUavHeap->GetGPUDescriptorHandleForHeapStart();

        CD3DX12_GPU_DESCRIPTOR_HANDLE posSrv(hs, 0, m_cbvSrvUavIncSize);
        m_commandList->SetComputeRootDescriptorTable(1, posSrv);

        CD3DX12_GPU_DESCRIPTOR_HANDLE normSrv(hs, 1, m_cbvSrvUavIncSize);
        m_commandList->SetComputeRootDescriptorTable(2, normSrv);

        CD3DX12_GPU_DESCRIPTOR_HANDLE albedoSrv(hs, 2, m_cbvSrvUavIncSize);
        m_commandList->SetComputeRootDescriptorTable(3, albedoSrv);

        CD3DX12_GPU_DESCRIPTOR_HANDLE specularSrv(hs, 3, m_cbvSrvUavIncSize);
        m_commandList->SetComputeRootDescriptorTable(4, specularSrv);

        CD3DX12_GPU_DESCRIPTOR_HANDLE renderTargetUav(hs, 4, m_cbvSrvUavIncSize);
        m_commandList->SetComputeRootDescriptorTable(5, renderTargetUav);
    }

    // dispatch compute
    {
        m_commandList->Dispatch(m_width / 16, m_height / 16, 1);
    }
}

void D3D12HelloTexture::RecPresentPassCmd()
{
    m_commandList->SetPipelineState(m_presentPass_pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(m_gBufferPass_rootSig.Get());

    // Set root arguments
    {
        // Set desc. heap = texSRVHeap
        ID3D12DescriptorHeap* ppHeaps[] = { m_presentPass_srvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        // Set root desc. table = texSRVHeap
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_presentPass_srvHeap->GetGPUDescriptorHandleForHeapStart());
        m_commandList->SetGraphicsRootDescriptorTable(1, srvHandle);

        // Set constant buffer
        GBufferPassUniforms cb = { XMMatrixIdentity() };

        UINT8* pConstantDataBegin;
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_gBufferPass_constantsURes->Map(0, &readRange, reinterpret_cast<void**>(&pConstantDataBegin)));
        memcpy(pConstantDataBegin, &cb, sizeof(cb));
        m_gBufferPass_constantsURes->Unmap(0, nullptr);

        m_commandList->SetGraphicsRootConstantBufferView(0, m_gBufferPass_constantsURes->GetGPUVirtualAddress());
    }

    // Set OM
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_presentPass_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvIncSize);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    }

    // Set RS
    {
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
    }

    // Set IA
    {
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_presentPass_vbv);
    }

    // Draw
    m_commandList->DrawInstanced(6, 1, 0, 0);
}

void D3D12HelloTexture::WaitForPreviousFrame()
{
    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12HelloTexture::ResetCommandList()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_gBufferPass_pipelineState.Get()));
}

void D3D12HelloTexture::SubmitCommandList()
{
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForPreviousFrame();
}

void D3D12HelloTexture::OnKeyDown(UINT8 key)
{
    m_keys[key] = true;
}

void D3D12HelloTexture::OnKeyUp(UINT8 key)
{
    m_keys[key] = false;
}