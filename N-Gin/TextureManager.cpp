#include "TextureManager.h"

#include <D3DX11.h>
#include <stdexcept>

TextureManager::TextureManager(ID3D11Device* device) : device(device)
{
    // Index 0 = INVALID_TEXTURE
    textures.emplace_back();
}

TextureHandle TextureManager::LoadTexture(const std::string& path)
{
    if (!device)
        throw std::runtime_error(
            "TextureManager has no D3D11 device."
        );

    Texture texture;

    HRESULT hr =
        D3DX11CreateShaderResourceViewFromFileA(
            device,
            path.c_str(),
            nullptr,
            nullptr,
            texture.srv.GetAddressOf(),
            nullptr
        );

    if (FAILED(hr))
    {
        throw std::runtime_error( "Failed to load texture: " + path);
    }

    textures.push_back(std::move(texture));  // texture has to be moved to avoid copies

    return static_cast<TextureHandle>(textures.size() - 1);
}

const Texture& TextureManager::GetTexture(TextureHandle handle) const
{
    if (handle == INVALID_TEXTURE || handle >= textures.size())
        throw std::runtime_error("Invalid texture handle.");

    return textures[handle];
}

const Texture& TextureManager::GetDefaultAlbedoTexture()
{
    if (defaultAlbedoMap != INVALID_TEXTURE)
        return textures[defaultAlbedoMap];

    TextureHandle handle = LoadTexture(defaultAlbedoMapPath);

    if (handle == INVALID_TEXTURE)
        throw std::runtime_error("Default Texture couldn't be found.");

    defaultAlbedoMap = handle;
    return textures[handle];
}

const Texture& TextureManager::GetDefaultNormalTexture()
{
    if (defaultNormalMap != INVALID_TEXTURE)
        return textures[defaultNormalMap];

    TextureHandle handle = LoadTexture(defaultNormalMapPath);

    if (handle == INVALID_TEXTURE)
        throw std::runtime_error("Default Texture couldn't be found.");

    defaultNormalMap = handle;
    return textures[handle];
}

const Texture& TextureManager::GetDefaultSmoothnessTexture()
{
    if (defaultSmoothnessMap != INVALID_TEXTURE)
        return textures[defaultSmoothnessMap];

    TextureHandle handle = LoadTexture(defaultSmoothnessMapPath);

    if (handle == INVALID_TEXTURE)
        throw std::runtime_error("Default Texture couldn't be found.");

    defaultSmoothnessMap = handle;
    return textures[handle];
}

const Texture& TextureManager::GetDefaultHeightTexture()
{
    if (defaultHeightMap != INVALID_TEXTURE)
        return textures[defaultHeightMap];

    TextureHandle handle = LoadTexture(defaultHeightMapPath);

    if (handle == INVALID_TEXTURE)
        throw std::runtime_error("Default Texture couldn't be found.");

    defaultHeightMap = handle;
    return textures[handle];
}