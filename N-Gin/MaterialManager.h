#pragma once

#include <vector>
#include <stdexcept>
#include <DirectXMath.h>

#include "Usings.h"

struct Material
{
    DirectX::XMFLOAT4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	uint8_t isMetallic = 0;  // 0 or 1
	TextureHandle albedoTexture = INVALID_TEXTURE;
	TextureHandle normalTexture = INVALID_TEXTURE;
	TextureHandle smoothnessTexture = INVALID_TEXTURE;
	TextureHandle heightTexture = INVALID_TEXTURE;
};

class MaterialManager
{
	private:
	    // index 0 = INVALID_MATERIAL
	    std::vector<Material> materials;

	    MaterialHandle defaultMaterial = INVALID_MATERIAL;

	public:
	    MaterialManager();

	    MaterialHandle CreateMaterial(const Material& material);

	    const Material& GetMaterial(MaterialHandle handle) const;

	    MaterialHandle GetDefaultMaterial() const
	    {
	        return defaultMaterial;
	    }
};