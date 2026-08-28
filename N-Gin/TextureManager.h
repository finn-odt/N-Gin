#pragma once
#include <d3d11.h>
#include <vector>
#include <string>
#include <wrl/client.h>

#include "Usings.h"

struct Texture
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
};

class TextureManager
{
	private:
	    ID3D11Device* device = nullptr;

	    // index 0 = INVALID_TEXTURE
	    std::vector<Texture> textures;

	public:
	    TextureManager(ID3D11Device* device);

	    TextureHandle LoadTexture(const std::string& path);

	    const Texture& GetTexture(TextureHandle handle) const;
};
