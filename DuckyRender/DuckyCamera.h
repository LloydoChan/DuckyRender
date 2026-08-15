#pragma once

struct MovementStruct
{
	float xMovement = 0.f;
	float yMovement = 0.f;
	float zMovement = 0.f;
};

class DuckyCamera
{
public:
	DuckyCamera(const XMVECTOR& At, const XMVECTOR& Eye, float FOV, float AspectRatio, float NearZ, float FarZ);
	void Update(float DeltaTime, const MovementStruct& Movement, float MouseDeltaX, float MouseDeltaY, bool Rotate);

	XMVECTOR GetViewVector() { return XMVector4Normalize(XMVectorSubtract(mEye, mAt)); }

	const XMMATRIX& GetProjection() const { return mProjection; }
	const XMMATRIX& GetView() const { return mView; }
	const XMMATRIX& GetViewProjection() const { return mViewProjection; }

	const XMVECTOR& GetEye() const { return mEye; }

	DuckyFrustum GetViewFrustum();
private:
	XMVECTOR mAt;
	XMVECTOR mEye;
	float mFOV, mAspectRatio, mNearZ, mFarZ;

	const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	XMMATRIX mProjection;
	XMMATRIX mView;
	XMMATRIX mViewProjection;

	float mViewLength = 5.f;
};