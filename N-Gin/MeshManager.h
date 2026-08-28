#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "Components/Components.h"

#include "ufbx.h"  // FBX Library

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texCoord;
};

struct CpuMeshData
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

struct Mesh
{
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	uint32_t stride = sizeof(Vertex);

	struct WorldBounds
	{
		BoundingBox box;
		bool dirty = true;
	};

	BoundingBox localBounds;
	WorldBounds worldBounds;
};


class MeshManager
{
private:
    ID3D11Device* device = nullptr;
    std::vector<Mesh> meshes;  // index 0 is reserved for INVALID_MESH

	MeshHandle CreateMeshFromCpuData(const CpuMeshData& data);

	CpuMeshData LoadMeshCpuWithUfbx(const std::string& path);

public:
    explicit MeshManager(ID3D11Device* device);

    MeshHandle CreateMesh(const std::vector<Vertex>& vertices);

	MeshHandle LoadMesh(const std::string& path);
	BoundingBox GetBoundingBoxForMesh(const std::vector<Vertex>& vertices);

    const Mesh& GetMesh(MeshHandle handle) const;
};