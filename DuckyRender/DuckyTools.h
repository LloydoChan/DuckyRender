#pragma once
using namespace DirectX;

namespace DuckyLog
{
    bool Initialise();
    void Shutdown();

    void Info(std::wstring_view message);
    void Warning(std::wstring_view message);
    void Error(std::wstring_view message);

    bool CheckHRESULT(HRESULT result, std::wstring_view context);
}

void DebugMatrix(XMMATRIX& nextTransform, std::wofstream& logFile);

static std::wstring GetLatestWinPixGpuCapturerPath_Cpp17()
{
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

    std::filesystem::path pixInstallationPath = programFilesPath;
    pixInstallationPath /= "Microsoft PIX";

    std::wstring newestVersionFound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
    {
        if (directory_entry.is_directory())
        {
            if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
            {
                newestVersionFound = directory_entry.path().filename().c_str();
            }
        }
    }

    if (newestVersionFound.empty())
    {
        // TODO: Error, no PIX installation found
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

constexpr UINT64 AlignConstantBufferSize(UINT64 size)
{
	return (size + 255) & ~255;
}

std::filesystem::path GetExecutableDirectory();