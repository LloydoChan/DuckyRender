#include "pch.h"
#include "DuckyTools.h"

namespace DuckyLog
{
    std::wofstream gLogFile;

    bool Initialise()
    {
        gLogFile.open(L"DuckyRenderLog.txt", std::ios::out);
        return gLogFile.is_open();
    }

    void Shutdown()
    {
        if (gLogFile.is_open()) gLogFile.close();
    }

    void Info(std::wstring_view message)
    {
        if (gLogFile.is_open())
        {
            gLogFile << L"[INFO] " << message << L'\n';
            gLogFile.flush();
        }
    }

    void Warning(std::wstring_view message)
    {
        if (gLogFile.is_open())
        {
            gLogFile << L"[WARNING] " << message << L'\n';
            gLogFile.flush();
        }
    }

    void Error(std::wstring_view message)
    {
        if (gLogFile.is_open())
        {
            gLogFile << L"[ERROR] " << message << L'\n';
            gLogFile.flush();
        }
    }

    bool CheckHRESULT(HRESULT result, std::wstring_view context)
    {
        if (SUCCEEDED(result)) return true;

        const winrt::hstring errorMessage = winrt::hresult_error(result).message();

        if (gLogFile.is_open())
        {
            gLogFile
                << L"[ERROR] "
                << context
                << L": "
                << errorMessage.c_str()
                << L" (HRESULT: 0x"
                << std::hex
                << static_cast<unsigned long>(result)
                << std::dec
                << L")\n";

            gLogFile.flush();
        }

        return false;
    }
}

void DebugMatrix(XMMATRIX& nextTransform, std::wofstream& logFile)
{
	XMFLOAT4X4 debugMatrix;
	XMStoreFloat4x4(
		&debugMatrix,
		nextTransform);

	logFile
		<< std::setw(10) << debugMatrix._41 << ", "
		<< std::setw(10) << debugMatrix._42 << ", "
		<< std::setw(10) << debugMatrix._43 << ", "
		<< std::setw(10) << debugMatrix._44 << std::endl;
}