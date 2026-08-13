#include "pch.h"
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

void DuckyCamera::Update(float DeltaTime, const MovementStruct& Movement, float ViewLength, float MouseDeltaX, float MouseDeltaY, bool Rotate)
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

    mAt = XMVectorAdd(mAt, forward);
    mAt = XMVectorAdd(mAt, right);
    mAt = XMVectorAdd(mAt, relativeUp);

    mEye = XMVectorAdd(mAt,XMVectorScale(viewVector, ViewLength));

    mView = XMMatrixLookAtLH(mEye, mAt, up);
    mViewProjection = mView * mProjection;
}
