#pragma once
// precompiled header!
#include <vector>
#include <array>
#include <errno.h>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <winrt/base.h>
#include <wrl/client.h>
#include <tchar.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXTex.h>
#include <D3dx12.h>
#include <d3d12shader.h>
#include <DirectXMath.h>
#include <d3d12compiler.h>
#include <dxcapi.h>

#include <pix3.h>
#include <shlobj.h>

#include "D3DDeviceManager.h"

#pragma comment(lib, "WindowsApp.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTex.lib")
