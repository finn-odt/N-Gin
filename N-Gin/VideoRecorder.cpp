#include "VideoRecorder.h"

#include <sstream>

VideoRecorder::VideoRecorder(ID3D11Device* device, ID3D11DeviceContext* context) : device(device), context(context) {}

VideoRecorder::~VideoRecorder()
{
    Stop();
}

bool VideoRecorder::Start(const std::string& outputPath, uint32_t width, uint32_t height, uint32_t fps)
{
    // #### DEBUG ####
    char workingDirectory[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, workingDirectory);

    std::string message =
        std::string("Working directory:\n") +
        workingDirectory +
        "\n\nVideo should be written to:\n" +
        workingDirectory +
        "\\output.mp4";

    MessageBoxA(
        nullptr,
        message.c_str(),
        "Video Recorder Path",
        MB_OK
    );
    // #### DEBUG END ####

    if (!device || !context)
        return false;

    if (IsRecording())
        Stop();

    this->width = width;
    this->height = height;
    this->frameRate = fps;

    if (width == 0 || height == 0 || fps == 0)
        return false;

    if (!CreateCaptureTextures())
        return false;

    std::ostringstream command;

    command
        << "ffmpeg "
        << "-y "
        << "-f rawvideo "
        << "-pixel_format rgba "
        << "-video_size "
        << width << "x" << height << " "
        << "-framerate " << fps << " "
        << "-i - "
        << "-an "

        // H.264
        << "-c:v libx264 "

        // Makes odd viewport dimensions compatible with yuv420p
        << "-vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\" "

        << "-pix_fmt yuv420p "
        << "\"" << outputPath << "\"";

    ffmpeg = _popen( command.str().c_str(), "wb" );

    if (!ffmpeg)
    {
        resolveTexture.Reset();
        stagingTexture.Reset();

        return false;
    }

    return true;
}

void VideoRecorder::Stop()
{
    if (ffmpeg)
    {
        // Closing tells FFmpeg that the video is finished.
        // FFmpeg then writes/finalizes the MP4 container.
        _pclose(ffmpeg);
        ffmpeg = nullptr;
    }

    resolveTexture.Reset();
    stagingTexture.Reset();
}

void VideoRecorder::CaptureFrame(ID3D11Texture2D* sourceTexture)
{
    if (!ffmpeg || !sourceTexture)
        return;

    D3D11_TEXTURE2D_DESC sourceDesc{};
    sourceTexture->GetDesc(&sourceDesc);

    // The capture textures must have exactly the same dimensions.
    if (sourceDesc.Width != width || sourceDesc.Height != height)
    {
        return;
    }

    // Your current backbuffer uses 4x MSAA.
    if (sourceDesc.SampleDesc.Count > 1)
    {
        context->ResolveSubresource(
            resolveTexture.Get(),
            0,
            sourceTexture,
            0,
            DXGI_FORMAT_R8G8B8A8_UNORM
        );
    }
    else
    {
        context->CopyResource(resolveTexture.Get(), sourceTexture);
    }

    // GPU -> CPU-readable resource
    context->CopyResource(stagingTexture.Get(), resolveTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};

    HRESULT hr = context->Map(
        stagingTexture.Get(),
        0,
        D3D11_MAP_READ,
        0,
        &mapped
    );

    if (FAILED(hr))
        return;

    const uint8_t* source = static_cast<const uint8_t*>(mapped.pData);

    const size_t rowSize = static_cast<size_t>(width) * 4;

    for (uint32_t y = 0; y < height; ++y)
    {
        fwrite(
            source + y * mapped.RowPitch,
            1,
            rowSize,
            ffmpeg
        );
    }

    context->Unmap(stagingTexture.Get(), 0);
}

bool VideoRecorder::CreateCaptureTextures()
{
    resolveTexture.Reset();
    stagingTexture.Reset();

    D3D11_TEXTURE2D_DESC desc{};

    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;  // IMPORTANT: video texture isn't multisampled
    desc.SampleDesc.Quality = 0;

    // GPU-side resolved frame
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &resolveTexture);

    if (FAILED(hr))
        return false;

    // CPU-readable version
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = device->CreateTexture2D(&desc, nullptr, &stagingTexture);

    return SUCCEEDED(hr);
}