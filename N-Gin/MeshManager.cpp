#include "MeshManager.h"
#include "Usings.h"

MeshManager::MeshManager(ID3D11Device* device)
    : device(device)
{
    meshes.emplace_back();  // Index 0 = INVALID_MESH
}

/*MeshHandle MeshManager::CreateMesh(const std::vector<Vertex>& vertices)
{
	if (vertices.empty())
		return INVALID_MESH;

	CpuMeshData data;

	data.vertices = vertices;

	data.indices.reserve(vertices.size());

	for (uint32_t i = 0; i < static_cast<uint32_t>(vertices.size()); i++)
	{
		data.indices.push_back(i);
	}

	return CreateMeshFromCpuData(data);
}*/

BoundingBox MeshManager::GetBoundingBoxForMesh(const std::vector<Vertex>& vertices)
{
	XMFLOAT3 minPoint{ FLT_MAX, FLT_MAX, FLT_MAX };  // init: biggest values possible
	XMFLOAT3 maxPoint{ -FLT_MAX, -FLT_MAX, -FLT_MAX };  // init: smallest values possible

	// search maxima and minima of every axis in all vertices
	for (const Vertex& vertex : vertices)
	{
		minPoint.x = std::min<float>(minPoint.x, vertex.position.x);
		minPoint.y = std::min<float>(minPoint.y, vertex.position.y);
		minPoint.z = std::min<float>(minPoint.z, vertex.position.z);

		maxPoint.x = std::max<float>(maxPoint.x, vertex.position.x);
		maxPoint.y = std::max<float>(maxPoint.y, vertex.position.y);
		maxPoint.z = std::max<float>(maxPoint.z, vertex.position.z);
	}

	XMFLOAT3 center{
	(minPoint.x + maxPoint.x) * 0.5f,
	(minPoint.y + maxPoint.y) * 0.5f,
	(minPoint.z + maxPoint.z) * 0.5f
	};

	XMFLOAT3 extents{  // from the center (so like a radius)
		(maxPoint.x - minPoint.x) * 0.5f,
		(maxPoint.y - minPoint.y) * 0.5f,
		(maxPoint.z - minPoint.z) * 0.5f
	};

	return { center, extents };
}

MeshHandle MeshManager::CreateMeshFromCpuData(const CpuMeshData& data)
{
	if (!device)
		throw std::runtime_error("MeshManager has no D3D11 device.");

	if (data.vertices.empty() || data.indices.empty())
		throw std::runtime_error("Cannot create mesh from empty data.");

	Mesh mesh;
	mesh.vertexCount = static_cast<uint32_t>(data.vertices.size());
	mesh.indexCount = static_cast<uint32_t>(data.indices.size());
	mesh.stride = sizeof(Vertex);
	mesh.localBounds = GetBoundingBoxForMesh(data.vertices);  // create Bounding Box

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * data.vertices.size());
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = data.vertices.data();

	HRESULT hr = device->CreateBuffer(
		&vertexBufferDesc,
		&vertexData,
		mesh.vertexBuffer.GetAddressOf()
	);

	if (!CheckHR(hr, "Failed to create vertex buffer."))
		return false;

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * data.indices.size());
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = data.indices.data();

	hr = device->CreateBuffer(
		&indexBufferDesc,
		&indexData,
		mesh.indexBuffer.GetAddressOf()
	);

	if (!CheckHR(hr, "Failed to create index buffer."))
		return false;

	meshes.push_back(std::move(mesh));

	return static_cast<MeshHandle>(meshes.size() - 1);
}


MeshHandle MeshManager::LoadMesh(const std::string& path)
{
	CpuMeshData data = LoadMeshCpuWithUfbx(path);
	return CreateMeshFromCpuData(data);
}

void MeshManager::GenerateFallbackTangent(Vertex& vertex)
{
	XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&vertex.normal));

	XMFLOAT3 normalFloat;
	XMStoreFloat3(&normalFloat, normal);

	// Choose an axis that isn't almost parallel to Normal
	XMVECTOR reference =
		std::abs(normalFloat.y) < 0.999f
		? XMVectorSet(0, 1, 0, 0)
		: XMVectorSet(1, 0, 0, 0);

	XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(reference, normal));

	XMFLOAT3 tangentFloat;
	XMStoreFloat3(&tangentFloat, tangent);

	vertex.tangent = {
		tangentFloat.x,
		tangentFloat.y,
		tangentFloat.z,
		1.0f
	};
}

void MeshManager::GenerateTriangleTangents(Vertex& v0, Vertex& v1, Vertex& v2)
{
	// get positions
	XMVECTOR p0 = XMLoadFloat3(&v0.position);
	XMVECTOR p1 = XMLoadFloat3(&v1.position);
	XMVECTOR p2 = XMLoadFloat3(&v2.position);

	// create two edges from p0
	XMVECTOR edge1 = p1 - p0;
	XMVECTOR edge2 = p2 - p0;

	float du1 = v1.texCoord.x - v0.texCoord.x;
	float dv1 = v1.texCoord.y - v0.texCoord.y;

	float du2 = v2.texCoord.x - v0.texCoord.x;
	float dv2 = v2.texCoord.y - v0.texCoord.y;

	float determinant = du1 * dv2 - du2 * dv1;

	// Degenerate UV triangle: tangent cannot be calculated.
	if (std::abs(determinant) < 1e-8f)
	{
		GenerateFallbackTangent(v0);
		GenerateFallbackTangent(v1);
		GenerateFallbackTangent(v2);
		return;
	}

	float r = 1.0f / determinant;

	XMVECTOR tangent = (edge1 * dv2 - edge2 * dv1) * r;
	tangent = XMVector3Normalize(tangent);

	XMVECTOR bitangent = (edge2 * du1 - edge1 * du2) * r;

	float tangentLengthSq = XMVectorGetX( XMVector3LengthSq(tangent) );

	if (tangentLengthSq < 1e-12f)
	{
		GenerateFallbackTangent(v0);
		GenerateFallbackTangent(v1);
		GenerateFallbackTangent(v2);
		return;
	}

	// Same triangle tangent/bitangent, but calculate
	// handedness relative to each vertex's normal.
	auto StoreTangent =
		[&](Vertex& vertex)
		{
			XMVECTOR normal = XMVector3Normalize( XMLoadFloat3(&vertex.normal) );

			// Gram-Schmidt:
			// make the Tangent perpendicular to this vertex's Normal
			// by using Gram-Schmidtsches Orthogonalisierungsverfahren
			XMVECTOR vertexTangent = tangent -
				normal * XMVectorGetX(
					XMVector3Dot(normal, tangent)
				);

			vertexTangent = XMVector3Normalize(vertexTangent);

			// find out sign on basis of dot(generatedBitangent, realBitangent)
			float sign =
				XMVectorGetX(
					XMVector3Dot(
						XMVector3Cross(
							normal,
							vertexTangent
						),
						bitangent
					)
				) < 0.0f
				? -1.0f
				: 1.0f;

			XMFLOAT3 tangentFloat;
			XMStoreFloat3( &tangentFloat, vertexTangent );

			// only tangent, and the handedness are send
			// (bitangent can be reconstructed due to Normal, Tangent and Sign)
			vertex.tangent = {
				tangentFloat.x,
				tangentFloat.y,
				tangentFloat.z,
				sign
			};
		};

	StoreTangent(v0);
	StoreTangent(v1);
	StoreTangent(v2);
}
CpuMeshData MeshManager::LoadMeshCpuWithUfbx(const std::string& path)
{
	ufbx_load_opts opts = {};
	ufbx_error error = {};

	// load the fbx file into a scene
	ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

	if (!scene)
		throw std::runtime_error(error.description.data);

	CpuMeshData result;

	auto CreateVertex = [](ufbx_mesh* mesh, uint32_t index)
		{
			Vertex vertex{};

			ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
			ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
			ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);

			vertex.position = { static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z) };
			vertex.normal = { static_cast<float>(normal.x), static_cast<float>(normal.y), static_cast<float>(normal.z) };
			vertex.texCoord = { static_cast<float>(uv.x), 1.0f - static_cast<float>(uv.y) };

			return vertex;
		};

	// go through every mesh in the scene
	for (size_t meshIndex = 0; meshIndex < scene->meshes.count; meshIndex++)
	{
		ufbx_mesh* mesh = scene->meshes.data[meshIndex];

		// index-vector for triangles in the mesh
		std::vector<uint32_t> triIndices(mesh->max_face_triangles * 3);

		// iterate through all faces in the mesh (triangles, quads or n-gons)
		for (size_t faceIndex = 0; faceIndex < mesh->faces.count; faceIndex++)
		{
			ufbx_face face = mesh->faces.data[faceIndex];

			// triangulate the face to get the number of triangles needed to describe this face
			uint32_t numTriangles = ufbx_triangulate_face(
				triIndices.data(),
				triIndices.size(),
				mesh,
				face
			);

			// iterate through every Vertex of all triangles
			for (uint32_t triangle = 0; triangle < numTriangles; ++triangle)
			{
				uint32_t index0 = triIndices[triangle * 3 + 0];  // first vertex index
				uint32_t index1 = triIndices[triangle * 3 + 1];  // second vertex index
				uint32_t index2 = triIndices[triangle * 3 + 2];  // third vertex index

				// create 3 Vertices
				Vertex vertex0 = CreateVertex(mesh, index0);
				Vertex vertex1 = CreateVertex(mesh, index1);
				Vertex vertex2 = CreateVertex(mesh, index2);

				GenerateTriangleTangents(vertex0, vertex1, vertex2);

				uint32_t baseIndex = static_cast<uint32_t>(result.vertices.size());

				result.vertices.push_back(vertex0);
				result.vertices.push_back(vertex1);
				result.vertices.push_back(vertex2);

				result.indices.push_back(baseIndex + 0);
				result.indices.push_back(baseIndex + 1);
				result.indices.push_back(baseIndex + 2);
			}
		}
	}

	ufbx_free_scene(scene);  // delete the scene again after loading all data
	return result;
}

const Mesh& MeshManager::GetMesh(MeshHandle handle) const
{
	if (handle == INVALID_MESH || handle >= meshes.size())  // id not possible?
		throw std::runtime_error("Invalid mesh handle.");

	return meshes[handle];  // return Mesh from meshes-vector
}


/*


// USAGE
EntityId entity = entityManager.AddEntity();

MeshHandle playerMesh = meshManager.LoadMeshFromObj("Assets/player.obj");
//LoadMesh("Assets/cube.obj");
//LoadMesh("Assets/character.fbx");

entityManager.AddComponent(entity, Transform{});
entityManager.AddComponent(entity, MeshRenderer{ playerMesh });




const Mesh& mesh = meshManager.GetMesh(renderer.mesh);

UINT stride = mesh.stride;
UINT offset = 0;

deviceContext->IASetVertexBuffers(
	0,
	1,
	mesh.vertexBuffer.GetAddressOf(),
	&stride,
	&offset
);

deviceContext->IASetIndexBuffer(
	mesh.indexBuffer.Get(),
	DXGI_FORMAT_R32_UINT,
	0
);

deviceContext->DrawIndexed(mesh.indexCount, 0, 0);


*/