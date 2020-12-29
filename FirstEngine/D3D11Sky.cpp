#include "D3D11Sky.h"

#include "FreeCameraNode.h"
#include "SceneTree.h"
#include "memoryUtility.h"
#include "LightManager.h"
#include "MeshComponent.h"
#include "SystemClass.h"

D3D11Sky::D3D11Sky(DirectX::XMFLOAT4X4* pMatrix, ID3D11Device* device, const std::wstring& texture) : D3D11Drawable(nullptr, pMatrix) {

	float skySphereRadius = 5000.0f;
	GeometryGenerator::MeshData sphere;
	GeometryGenerator geoGen;
	geoGen.CreateSphere(skySphereRadius, 30, 30, sphere);
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	IndexedTriangleList vertices;
	vertices.vertices.reserve(sphere.Vertices.size());
	for (UINT i = 0; i < sphere.Vertices.size(); ++i) {
		vertices.vertices.push_back(
			Vertex(
				DirectX::XMFLOAT4(sphere.Vertices[i].Position.x,	sphere.Vertices[i].Position.y,	sphere.Vertices[i].Position.z, 1.0f),
				DirectX::XMFLOAT3(sphere.Vertices[i].Normal.x,		sphere.Vertices[i].Normal.y,	sphere.Vertices[i].Normal.z),
				DirectX::XMFLOAT2(sphere.Vertices[i].TexC.x,		sphere.Vertices[i].TexC.y),
				DirectX::XMFLOAT3(sphere.Vertices[i].TangentU.x,	sphere.Vertices[i].TangentU.y,	sphere.Vertices[i].TangentU.z)
			)
		);
	}
	vertices.indices.reserve(sphere.Indices.size());
	for (UINT i = 0; i < sphere.Indices.size(); ++i) {
		vertices.indices.push_back(sphere.Indices[i]);
	}

	std::unique_ptr<VertexBuffer> vb = std::make_unique<VertexBuffer>(device, vertices.vertices);
	AddBind(std::move(vb));

	std::unique_ptr<IndexBuffer> ibuf = std::make_unique<IndexBuffer>(device, vertices.indices);
	AddIndexBuffer(std::move(ibuf));

	ShaderClass& vs = ShaderHolder::GetShader(L"SkyVertexShader.fx");
	auto pvs = std::make_unique<VertexShader>(device, vs.GetBytecode(), vs.GetVertexShader());
	ID3DBlob* pvsbc = pvs->GetBytecode();
	AddBind(std::move(pvs));

	ShaderClass& ps = ShaderHolder::GetShader(L"SkyPixelShader.fx");
	auto pps = std::make_unique<PixelShader>(device, ps.GetBytecode(), ps.GetPixelShader());
	ID3DBlob* ppsbc = pps->GetBytecode();
	AddBind(std::move(pps));

	DirectX::CreateDDSTextureFromFile(device, SystemClass::GetGraphics().GetD3D()->GetDeviceContext(), texture.c_str(), m_texture.GetAddressOf(), m_textureView.GetAddressOf());

	std::unique_ptr<ShaderResource> srv = std::make_unique<ShaderResource>(device, m_textureView.Get());
	AddBind(std::move(srv));

	std::unique_ptr<InputLayout> inputLayout = std::make_unique<InputLayout>(device, pvsbc);
	AddBind(std::move(inputLayout));

	std::unique_ptr<Topology> topology = std::make_unique<Topology>(device, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	AddBind(std::move(topology));

	std::unique_ptr<SamplerState> samplerState = std::make_unique<SamplerState>(device);
	AddBind(std::move(samplerState));

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	std::unique_ptr<DepthStencilState> depthStencilState = std::make_unique<DepthStencilState>(device, depthStencilDesc);
	AddBind(std::move(depthStencilState));

	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	std::unique_ptr<RasterizerState> rasterizerState = std::make_unique<RasterizerState>(device, rasterDesc);
	AddBind(std::move(rasterizerState));

	std::unique_ptr<BlendState> blendState = std::make_unique<BlendState>(device);
	AddBind(std::move(blendState));

	cbPerObject mt;
	DirectX::XMStoreFloat4x4(&mt.worldMatrix, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&mt.viewMatrix, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&mt.projectionMatrix, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&mt.worldInvTranspose, DirectX::XMMatrixIdentity());

	std::unique_ptr<VertexConstantBuffer<cbPerObject>> cbvs = std::make_unique<VertexConstantBuffer<cbPerObject>>(device, mt);
	m_cbvs = cbvs.get();
	AddBind(std::move(cbvs));

	std::unique_ptr<PixelConstantBuffer<cbPerObject>> cbps0 = std::make_unique<PixelConstantBuffer<cbPerObject>>(device, mt);
	m_cbps0 = cbps0.get();
	AddBind(std::move(cbps0));

	cbPerFrame lt;
	std::unique_ptr<PixelConstantBuffer<cbPerFrame>> cbps1 = std::make_unique<PixelConstantBuffer<cbPerFrame>>(device, lt, 1u);
	m_cbps1 = cbps1.get();
	AddBind(std::move(cbps1));
}

D3D11Sky::~D3D11Sky() {}

HRESULT D3D11Sky::VOnUpdate(SceneTree* pScene, float const elapsedMs, ID3D11DeviceContext* deviceContext) {

	return S_OK;
}

HRESULT D3D11Sky::VPreRender(SceneTree* pScene, ID3D11DeviceContext* deviceContext) {
	SceneNode::VPreRender(pScene, deviceContext);

	const std::shared_ptr<FreeCameraNode> camera = pScene->GetCamera();

	cbPerObject mt;
	mt.worldMatrix = pScene->GetTopMatrixT();
	mt.viewMatrix = camera->GetViewMatrix4x4T();
	mt.projectionMatrix = camera->GetProjectionMatix4x4T();
	// Inverse-transpose is just applied to normals.  So zero out 
	// translation row so that it doesn't get into our inverse-transpose
	// calculation--we don't want the inverse-transpose of the translation.
	DirectX::XMMATRIX A = DirectX::XMLoadFloat4x4(&mt.worldMatrix);
	A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);
	DirectX::XMStoreFloat4x4(&mt.worldInvTranspose, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A)));

	m_cbvs->Update(deviceContext, mt);
	m_cbps0->Update(deviceContext, mt);

	cbPerFrame lt;
	m_cbps1->Update(deviceContext, lt);

	return S_OK;
}

