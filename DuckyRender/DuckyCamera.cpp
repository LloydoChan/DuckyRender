#include "pch.h"
#include "DuckyMesh.h"
#include "DuckyCamera.h"

constexpr float MOUSE_YAW_SENSITIVITY = 0.003f;
constexpr float MOUSE_PITCH_SENSITIVITY = 0.003f;

DuckyCamera::DuckyCamera(const XMVECTOR& At, const XMVECTOR& Eye, float FOV, float AspectRatio, float NearZ, float FarZ) : 
	mAt(At), mEye(Eye), mFOV(FOV), mAspectRatio(AspectRatio), mNearZ(NearZ), mFarZ(FarZ)
{
	mProjection = XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, mNearZ, mFarZ);
	mView = XMMatrixLookAtLH(mEye, mAt, up);
	mViewProjection = mView * mProjection;
}

void DuckyCamera::Update(float DeltaTime, const MovementStruct& Movement, float MouseDeltaX, float MouseDeltaY, bool Rotate)
{
    XMVECTOR viewVector = GetViewVector();

    viewVector = XMVector3Normalize(viewVector);

    if (Rotate && MouseDeltaX != 0.0f)
    {
        const float yaw = MouseDeltaX * MOUSE_YAW_SENSITIVITY;

        XMVECTOR quat = XMQuaternionRotationAxis(up, yaw);

        viewVector = XMVector3Normalize(XMVector3Rotate(viewVector, quat));
    }

    XMVECTOR rightVector =XMVector3Normalize(XMVector3Cross(viewVector, up));

    if (Rotate && MouseDeltaY != 0.0f)
    {
        const float pitch = MouseDeltaY * MOUSE_PITCH_SENSITIVITY;

        XMVECTOR quat = XMQuaternionRotationAxis(rightVector, -pitch);

        XMVECTOR possibleViewVector = XMVector3Normalize(XMVector3Rotate(viewVector, quat));

        constexpr float pitchLimit = 0.99f;

        if (std::abs( XMVectorGetX(XMVector3Dot(possibleViewVector,up))) < pitchLimit) viewVector = possibleViewVector;
    }

    XMVECTOR right = XMVector3Cross(viewVector, up);
    right = XMVector3Normalize(right);

    XMVECTOR relativeUp = XMVector3Cross(viewVector, right);

    XMVECTOR forward = XMVectorScale(viewVector, Movement.zMovement);

    right = XMVectorScale(right, Movement.xMovement);

    relativeUp = XMVectorScale(relativeUp, Movement.yMovement);

    mViewLength += Movement.zMovement;

    mAt = XMVectorAdd(mAt, right);
    mAt = XMVectorAdd(mAt, relativeUp);

    XMVECTOR ScaledView = XMVectorScale(viewVector, mViewLength);
    mEye = XMVectorAdd(mAt, ScaledView);

    mView = XMMatrixLookAtLH(mEye, mAt, up);
    mViewProjection = mView * mProjection;
}

DuckyFrustum DuckyCamera::GetViewFrustum()
{
    DuckyFrustum frustum;
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, mViewProjection);

    // left
    frustum.mPlanes[0] = { m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41 };

    // right
    frustum.mPlanes[1] = { m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41 };

    // bottom
    frustum.mPlanes[2] = { m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42 };

    // top
    frustum.mPlanes[3] = { m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42 };

    // near
    frustum.mPlanes[4] = { m._13, m._23, m._33, m._43 };

    // far
    frustum.mPlanes[5] = { m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43 };

    for (int i = 0; i < 6; ++i)
    {
        XMVECTOR p = XMLoadFloat4(&frustum.mPlanes[i]);
        p = XMPlaneNormalize(p);
        XMStoreFloat4(&frustum.mPlanes[i], p);
    }

    return frustum;

}
