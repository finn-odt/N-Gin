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
		std::string defaultAlbedoMapPath = "Configs/Textures/defaultTexture_albedo.png";
		std::string defaultNormalMapPath = "Configs/Textures/defaultTexture_normal.png";
		std::string defaultSmoothnessMapPath = "Configs/Textures/defaultTexture_smoothness.png";
		std::string defaultHeightMapPath = "Configs/Textures/defaultTexture_height.png";

		// will be initialized after first load to avoid reloading
		TextureHandle defaultAlbedoMap = INVALID_TEXTURE;
		TextureHandle defaultNormalMap = INVALID_TEXTURE;
		TextureHandle defaultSmoothnessMap = INVALID_TEXTURE;
		TextureHandle defaultHeightMap = INVALID_TEXTURE;

	    // index 0 = INVALID_TEXTURE
	    std::vector<Texture> textures;

	public:
	    TextureManager(ID3D11Device* device);

	    TextureHandle LoadTexture(const std::string& path);

	    const Texture& GetTexture(TextureHandle handle) const;

		const Texture& GetDefaultAlbedoTexture();
		const Texture& GetDefaultNormalTexture();
		const Texture& GetDefaultSmoothnessTexture();
		const Texture& GetDefaultHeightTexture();
};
