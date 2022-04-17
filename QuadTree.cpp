#include "Framework.h"
#include "QuadTree.h"
#include "Environment/Terrain.h"
#include "Viewer/Frustum.h"

const UINT QuadTree::DivideCount = 1024; //1024, 32x32

//TerrainŬ�����κ��� ������ ���� ������ ������ ����Ʈ���� ����� ���� �� 
//���������� �þ� ���� ������ �� Ŭ�������� ������ ����
QuadTree::QuadTree(Terrain* terrain, Frustum* frustum)
	: terrain(terrain), frustum(frustum), drawCount(0)
{
}

QuadTree::~QuadTree()
{
	DeleteNode(parent);
	SAFE_DELETE(parent);
	SAFE_DELETE(frustum);

	SAFE_DELETE(csData);

	SAFE_DELETE(material);
	
	SAFE_DELETE(perspectiveForCulling);

	SAFE_DELETE(csHeightTex);
}

void QuadTree::Initialize()
{
	material = new Material(Shaders + L"QuadTree_Noise_ver2.fx"); //������ �׽�Ʈ��
	//material->SetDiffuseMap(Textures + L"DirtH.dds"); //�����ؽ���

	material->SetDiffuseMap(Textures + L"ground5_Diffuse.dds");
	material->SetNormalMap(Textures + L"ground5_Normal.tga");

	vertexCount = terrain->VertexCount();
	triangleCount = vertexCount / 3;

	vertices = new VertexTextureNormalTangent[vertexCount];
	terrain->CopyVertices((void *)vertices);

	centerX = 0.0f, centerZ = 0.0f, width = 0.0f;

	//�߾����� ������ �ִ� �ʺ� ���
	CalcMeshDimensions(vertexCount, centerX, centerZ, width);

	parent = new NodeType();
	CreateTreeNode(parent, centerX, centerZ, width);

	csData = new CSData();

	//�߰� - �ø��� ����ü ũ�� ����
	if (this->frustum == NULL)
	{
		D3DDesc desc;
		D3D::GetDesc(&desc);
		//perspectiveForCulling = new Perspective(desc.Width, desc.Height, (float)D3DX_PI, 0.01f, 1000.0f); //(float)D3DX_PI * 0.7f
		perspectiveForCulling = new Perspective(desc.Width, desc.Height, (float)D3DX_PI * 0.6f);
		this->frustum = new Frustum(1000, Context::Get()->GetMainCamera(), perspectiveForCulling); //Context::Get()->GetPerspective()
	}

	InitCSDatas(vertices); //CSTexture����

	SAFE_DELETE_ARRAY(vertices);

	D3DXMatrixIdentity(&world);

}

void QuadTree::Ready()
{
}

void QuadTree::Update()
{
	frustum->Update();
}

void QuadTree::Render()
{
	drawCount = 0;
	RenderNode(parent);

	ImGui::Text("Terrain Draw : %d", drawCount);
}

void QuadTree::RenderNode(NodeType * node)
{
	D3DXVECTOR3 center(node->X, 0.0f, node->Z);

	D3DXVECTOR3 cameraPos;
	Context::Get()->GetMainCamera()->Position(&cameraPos);
	float zDistance = center.z - cameraPos.z;
	D3DXVECTOR3 distance = center - cameraPos;

	float d = node->Width / 2.0f;
	//����ü�� ������ �鸸 �׷���, �߰��� �ٰŸ��� �ø� ����
	if((distance.x > 20.0f || distance.x < -20.0f) && distance.z > 20.0f)
	{
		if (frustum->ContainCube(center, d) == false)
			return;
	}

	UINT count = 0;
	for (int i = 0; i < 4; i++)
	{
		if (node->Childs[i] != NULL)
		{
			count++;

			RenderNode(node->Childs[i]); //�ڽ� ��� ��� 
		}
	}
	//�� ������ �ݺ��� �� ��忡�� ���� �� & ����
	if (count != 0)
		return;

	UINT stride = sizeof(VertexTextureNormalTangent);
	UINT offset = 0;

	D3D::GetDC()->IASetVertexBuffers(0, 1, &node->VertexBuffer, &stride, &offset);
	D3D::GetDC()->IASetIndexBuffer(node->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	D3D::GetDC()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	material->SetWorld(world);

	UINT indexCount = node->TriangleCount * 3;
	material->GetShader()->DrawIndexed(0, 0, indexCount);

	drawCount += node->TriangleCount;
}

void QuadTree::DeleteNode(NodeType * node)
{
	for (int i = 0; i < 4; i++)
	{
		if (node->Childs[i] != NULL)
			DeleteNode(node->Childs[i]);
		
		//SAFE_DELETE�� NULLüũ�� �ϱ⶧���� if������ �ڽĳ�� NULLüũ ���ʿ� ����
		SAFE_DELETE(node->Childs[i]); 
	}

	SAFE_RELEASE(node->VertexBuffer);
	SAFE_RELEASE(node->IndexBuffer);
}

//�ڽ� ��� ������ �Լ�
void QuadTree::CalcMeshDimensions(UINT vertexCount, float & centerX, float & centerZ, float & meshWidth)
{
	centerX = centerZ = 0.0f;

	for (UINT i = 0; i < vertexCount; i++)
	{
		centerX += vertices[i].Position.x;
		centerZ += vertices[i].Position.z;
	}
	//���� �߾� ���
	centerX = centerX / (float)vertexCount;
	centerZ = centerZ / (float)vertexCount;

	float maxWidth = 0.0f;
	float maxDepth = 0.0f;

	//�������κ��� �߾ӱ����� ����, ����(���밪 ����)
	float minWidth = fabsf(vertices[0].Position.x - centerX);
	float minDepth = fabsf(vertices[0].Position.z - centerZ);

	for (UINT i = 0; i < vertexCount; i++) //centerX, Z�������� ��������, �� �ȿ��� �ִ�, �ּҳ���, ���� ã�� �κ�
	{
		float width = fabsf(vertices[i].Position.x - centerX);
		float depth = fabsf(vertices[i].Position.z - centerZ);

		if (width > maxWidth) maxWidth = width;
		if (depth > maxDepth) maxDepth = depth;
		if (width < minWidth) minWidth = width;
		if (depth < minDepth) minDepth = depth;
	}

	float maxX = (float)max(fabsf(minWidth), fabsf(maxWidth));
	float maxZ = (float)max(fabsf(minDepth), fabsf(maxDepth));

	meshWidth = (float)max(maxX, maxZ) * 2.0f;
}
//�ڽ� ��� ����, positionX, Z���� �� ���� ������ �߾���ġ���� ���� 
//width���� �� ���������� ���̰� ����, ��ͷ� Ʈ�� ����
void QuadTree::CreateTreeNode(NodeType * node, float positionX, float positionZ, float width)
{

	node->X = positionX; //centerX
	node->Z = positionZ; //centerZ
	
	node->TriangleCount = 0;

	node->VertexBuffer = NULL;
	node->IndexBuffer = NULL;

	for (UINT i = 0; i < 4; i++)
		node->Childs[i] = NULL;

	//case1 ���� ������ ������
	//tirangles -> �ڽĳ�尡 �����ϴ� �ﰢ�� ����
	//�����ϴ� �����κп� ���ϴ� �ﰢ�� ���� ����
	UINT triangles = ContainTriangleCount(positionX, positionZ, width);
	if (triangles == 0) //�����ϴ� �ﰢ���� ������ ����
		return;

	//case2 �� ���� ���� ����
	if (triangles > DivideCount)
	{
		for (UINT i = 0; i < 4; i++)
		{
			//centerX, Z �������� ���� 4�� ���� �� �߽���
			float offsetX = (((i % 2) < 1) ? -1.0f : 1.0f) * (width / 4.0f); //-1,  1, -1,  1
			float offsetZ = (((i % 4) < 2) ? -1.0f : 1.0f) * (width / 4.0f); //-1, -1,  1,  1

			UINT count = ContainTriangleCount((positionX + offsetX), (positionZ + offsetZ), (width / 2.0f));
			//���� �� �ȿ� �ﰢ���� �����ϸ� �ڽĳ�� ����
			//-> ��ͷ� �ﰢ���� ���� ��ȿ� �������� ���������� �ݺ��ؼ� Ʈ������
			if (count > 0) 
			{
				node->Childs[i] = new NodeType();

				CreateTreeNode(node->Childs[i], (positionX + offsetX), (positionZ + offsetZ), (width / 2.0f));
			}
		}

		return;
	}

	//case3 ���� ������ ������(triangles <= DivideCount), �������� �ϴ� ���� �ּ� �ﰢ�� �������� ������
	node->TriangleCount = triangles;

	//��� ������ ����� ��� ����
	UINT vertexCount = triangles * 3;
	
	VertexTextureNormalTangent* vertices = new VertexTextureNormalTangent[vertexCount];
	UINT* indices = new UINT[vertexCount];

	UINT index = 0, vertexIndex = 0;
	for (UINT i = 0; i < triangleCount; i++)
	{
		//���ԵǾ��ִ��� �ٽ� Ȯ��
		if (IsTriangleContained(i, positionX, positionZ, width) == true)
		{
			vertexIndex = i * 3;

			vertices[index].Position = this->vertices[vertexIndex].Position;
			node->Vertices.push_back(this->vertices[vertexIndex].Position);
			vertices[index].Uv = this->vertices[vertexIndex].Uv;
			vertices[index].Normal = this->vertices[vertexIndex].Normal;
			vertices[index].Tangent = this->vertices[vertexIndex].Tangent;
			indices[index] = index;

			index++;
			vertexIndex++;

			vertices[index].Position = this->vertices[vertexIndex].Position;
			node->Vertices.push_back(this->vertices[vertexIndex].Position);
			vertices[index].Uv = this->vertices[vertexIndex].Uv;
			vertices[index].Normal = this->vertices[vertexIndex].Normal;
			vertices[index].Tangent = this->vertices[vertexIndex].Tangent;
			indices[index] = index;

			index++;
			vertexIndex++;

			vertices[index].Position = this->vertices[vertexIndex].Position;
			node->Vertices.push_back(this->vertices[vertexIndex].Position);
			vertices[index].Uv = this->vertices[vertexIndex].Uv;
			vertices[index].Normal = this->vertices[vertexIndex].Normal;
			vertices[index].Tangent = this->vertices[vertexIndex].Tangent;
			indices[index] = index;

			index++;
		}
	}

	//CreateVertexBuffer
	{
		D3D11_BUFFER_DESC desc = { 0 };
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.ByteWidth = sizeof(VertexTextureNormalTangent) * vertexCount;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		
		D3D11_SUBRESOURCE_DATA data = { 0 };
		data.pSysMem = vertices;

		HRESULT hr = D3D::GetDevice()->CreateBuffer(&desc, &data, &(node->VertexBuffer));
		assert(SUCCEEDED(hr));
	}

	//CreateIndexBuffer
	{
		D3D11_BUFFER_DESC desc = { 0 };
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.ByteWidth = sizeof(UINT) * vertexCount;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA data = { 0 };
		data.pSysMem = indices;

		HRESULT hr = D3D::GetDevice()->CreateBuffer(&desc, &data, &(node->IndexBuffer));
		assert(SUCCEEDED(hr));
	}
	SAFE_DELETE_ARRAY(vertices);
	SAFE_DELETE_ARRAY(indices);
}

UINT QuadTree::ContainTriangleCount(float positionX, float positionZ, float width)
{
	UINT count = 0;

	for (UINT i = 0; i < triangleCount; i++)
	{
		//�ﰢ���� ���ȿ� �����ִ��� üũ
		if (IsTriangleContained(i, positionX, positionZ, width) == true)
			count++;
	}

	return count;
}

//�ﰢ���� ���ȿ� �����ִ��� üũ
bool QuadTree::IsTriangleContained(UINT index, float positionX, float positionZ, float width)
{
	float radius = width / 2.0f;
	//index�� �ﰢ���� �ε����̹Ƿ� 3���ؼ� �ﰢ���� �̷�� ������ �ε����� ��ȯ
	UINT vertexIndex = index * 3; 
	//�ﰢ���� �� ���� ��ġ
	float x1 = vertices[vertexIndex].Position.x;
	float z1 = vertices[vertexIndex].Position.z;
	vertexIndex++;

	float x2 = vertices[vertexIndex].Position.x;
	float z2 = vertices[vertexIndex].Position.z;
	vertexIndex++;

	float x3 = vertices[vertexIndex].Position.x;
	float z3 = vertices[vertexIndex].Position.z;

	//�簢�� �߽ɰ� ���������� �簢�� �ȿ� �ﰢ���� ���� �ִ��� Ȯ��
	float minimumX = min(x1, min(x2, x3));
	if (minimumX > (positionX + radius))  
		return false;
	
	float maximumX = max(x1, max(x2, x3));
	if (maximumX < (positionX - radius))
		return false;
	
	float minimumZ = min(z1, min(z2, z3));
	if (minimumZ > (positionZ + radius))
		return false;
	
	float maximumZ = max(z1, max(z2, z3));
	if (maximumZ < (positionZ - radius))
		return false;

	return true;
}

void QuadTree::InitCSDatas(VertexTextureNormalTangent* terrainData)
{	
	vector<D3DXVECTOR3> tPos;

	for (UINT i = 0; i < vertexCount; i++)
	{
		tPos.push_back(terrainData[i].Position);
	}

	HRESULT hr;

	//Bind TerrainVertices
	D3D11_BUFFER_DESC inputDesc;
	inputDesc.Usage = D3D11_USAGE_DEFAULT;
	inputDesc.ByteWidth = sizeof(D3DXVECTOR3) * vertexCount; //node->Vertices.size() * sizeof(D3DXVECTOR3);
	inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	inputDesc.CPUAccessFlags = 0; //cpu���� ���� �����Ƿ� 0 
	inputDesc.StructureByteStride = sizeof(D3DXVECTOR3); //structeredBuffer�ϳ��� ũ��
	inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; //���ۿ뵵

	D3D11_SUBRESOURCE_DATA data = { 0 };
	data.pSysMem = &tPos[0];

	ID3D11Buffer* buffer = NULL;
	hr = D3D::GetDevice()->CreateBuffer(&inputDesc, &data, &buffer);
	assert(SUCCEEDED(hr));
	
	{
		//�� ����
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		//structuredBuffer�� BUFFEREX�� ����
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srvDesc.BufferEx.FirstElement = 0;
		srvDesc.BufferEx.Flags = 0;
		srvDesc.BufferEx.NumElements = vertexCount;

		hr = D3D::GetDevice()->CreateShaderResourceView(buffer, &srvDesc, &csSrv);
		assert(SUCCEEDED(hr));

		material->GetShader()->AsSRV("CSVertexData")->SetResource(csSrv);
	}

	//Bind Texture
	D3D11_TEXTURE2D_DESC desc = { 0 };
	desc.Width = 256;
	desc.Height = 256;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	//desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS; //D3D11_BIND_SHADER_RESOURCE
	desc.CPUAccessFlags = 0; //D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;

	hr = D3D::GetDevice()->CreateTexture2D(&desc, NULL, &csTexture);
	assert(SUCCEEDED(hr));
	
	//CreateSRV
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;//DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
	
		ID3D11ShaderResourceView* inputSRV;
		hr = D3D::GetDevice()->CreateShaderResourceView(csTexture, &srvDesc, &inputSRV);
	
		material->GetShader()->AsSRV("CSInputTexture")->SetResource(inputSRV);
	}

	//CreateUAV
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		ID3D11UnorderedAccessView* outputUAV;
		hr = D3D::GetDevice()->CreateUnorderedAccessView(csTexture, &uavDesc, &outputUAV);

		material->GetShader()->AsUAV("CSOutputTexture")->SetUnorderedAccessView(outputUAV);
	}
	
	//�׽�Ʈ�� -> ���� ������ �ؽ��� CS������ ����
	csHeightTex = new Texture(Contents + L"HeightMaps/HeightMap256.dds");
	material->GetShader()->AsSRV("CSHeightTex")->SetResource(csHeightTex->SRV());
}
