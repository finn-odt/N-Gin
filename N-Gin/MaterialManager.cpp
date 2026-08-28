#include "MaterialManager.h"

MaterialManager::MaterialManager()
{
    // Index 0 is reserved for INVALID_MATERIAL
    materials.emplace_back();

    // Index 1 becomes the default white material with alpha = 1
    defaultMaterial = CreateMaterial(Material{});
}

MaterialHandle MaterialManager::CreateMaterial(const Material& material)
{
    materials.push_back(material);

    return static_cast<MaterialHandle>(materials.size() - 1);
}

const Material& MaterialManager::GetMaterial(MaterialHandle handle) const
{
    if (handle == INVALID_MATERIAL || handle >= materials.size())
        throw std::runtime_error("Invalid material handle.");

    return materials[handle];
}