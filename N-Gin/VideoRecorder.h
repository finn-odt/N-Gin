#pragma once

#include <d3d11.h>
#include <string>
#include <wrl/client.h>

class VideoRecorder
{
private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> resolveTexture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;

    FILE* ffmpeg = nullptr;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameRate = 60;

    bool CreateCaptureTextures();

public:
	VideoRecorder(ID3D11Device* device, ID3D11DeviceContext* context);

	~VideoRecorder();

    bool Start(const std::string& outputPath, uint32_t width, uint32_t height, uint32_t fps);

    void CaptureFrame(ID3D11Texture2D* source);

    void Stop();

    bool IsRecording() const
    {
        return ffmpeg != nullptr;
    }
};
