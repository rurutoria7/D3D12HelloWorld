#pragma once
#include "stdafx.h"
using namespace DirectX;

struct MyCamera
{
    XMMATRIX m_view = XMMatrixIdentity();
    XMMATRIX m_proj = XMMatrixIdentity();
    XMVECTOR m_position = XMVectorZero();
    XMVECTOR m_viewDir = XMVectorZero();
    XMVECTOR m_up = XMVectorZero();

    const float speed = 2.0;
    const float fovDeg = 45.0f;
    const float nearZ = 0.01f;
    const float farZ = 100.0f;
    const float rotSpeed = 0.5f;


    XMMATRIX GetViewProj()
    {
        return m_view * m_proj;
    }

    void Init(float aspectRatio)
    {
        m_position = XMVectorSet(-13.0f, 3.0f, 0.0f, 1.0f);
        m_viewDir = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        m_up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        m_view = XMMatrixLookAtLH(m_position, m_viewDir + m_position, m_up);
        m_proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovDeg), aspectRatio, nearZ, farZ);
    }

    void Update(bool* keys, float deltaTime)
    {
        if (keys['W'])
        {
            m_position += m_viewDir * speed * deltaTime;
        }
        if (keys['S'])
        {
            m_position -= m_viewDir * speed * deltaTime;
        }
        if (keys['Q'])
        {
            m_position += m_up * speed * deltaTime;
        }
        if (keys['E'])
        {
            m_position -= m_up * speed * deltaTime;
        }
        if (keys['A'])
        {
            m_position += XMVector3Cross(m_viewDir, m_up) * speed * deltaTime;

        }
        if (keys['D'])
        {
            m_position -= XMVector3Cross(m_viewDir, m_up) * speed * deltaTime;
        }
        if (keys['Z'])
        {
            auto rot = XMMatrixRotationAxis(m_up, -rotSpeed * deltaTime);
            m_viewDir = XMVector3Transform(m_viewDir, rot);
        }
        if (keys['X'])
        {
            auto rot = XMMatrixRotationAxis(m_up, rotSpeed * deltaTime);
            m_viewDir = XMVector3Transform(m_viewDir, rot);
        }

        m_view = XMMatrixLookAtLH(m_position, m_viewDir + m_position, m_up);
    }
};
