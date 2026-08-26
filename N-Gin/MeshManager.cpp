#include "MeshManager.h"
#include "Usings.h"

MeshManager::MeshManager(ID3D11Device* device)
    : device(device)
{
    meshes.emplace_back();  // Index 0 = INVALID_MESH
}

MeshHandle MeshManager::CreateMesh(const std::vector<Vertex>& vertices)
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

CpuMeshData MeshManager::LoadMeshCpuWithUfbx(const std::string& path)
{
	ufbx_load_opts opts = {};
	ufbx_error error = {};

	// load the fbx file into a scene
	ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

	if (!scene)
		throw std::runtime_error(error.description.data);

	CpuMeshData result;

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

			// iterate through every Vertex of all triangles (therefore: 3 * triNo)
			for (uint32_t i = 0; i < numTriangles * 3; i++)
			{
				uint32_t index = triIndices[i];

				ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
				ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
				ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);

				Vertex vertex = {};
				vertex.position = {
					static_cast<float>(pos.x),
					static_cast<float>(pos.y),
					static_cast<float>(pos.z)
				};

				vertex.normal = {
					static_cast<float>(normal.x),
					static_cast<float>(normal.y),
					static_cast<float>(normal.z)
				};

				vertex.texCoord = {
					static_cast<float>(uv.x),
					static_cast<float>(uv.y)
				};

				result.vertices.push_back(vertex);
				result.indices.push_back(static_cast<uint32_t>(result.indices.size()));
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