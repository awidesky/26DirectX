#pragma once
#include "ObjectBase.hpp"
#include "Mesh.hpp"
#include "Material.hpp"

class MeshRenderer : public Component {
public:
    Mesh* pMeshData = nullptr;
    Material* pMaterial = nullptr;
    ID3D11Buffer* cBuffer = nullptr;

    MeshRenderer(Mesh* mesh, Material* mat) : pMeshData(mesh), pMaterial(mat) {}

    ~MeshRenderer() {
        if (cBuffer) cBuffer->Release();
    }

    void Start(GraphicsContext* gfx) override {
        D3D11_BUFFER_DESC cbd = { 0 };
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ConstantBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        gfx->Device->CreateBuffer(&cbd, nullptr, &cBuffer);
    }

    void Render(GraphicsContext* gfx) override {
        if (!pMeshData || !pMaterial) return;

        pMaterial->Bind(gfx->ImmediateContext);

        float s = 1.0f / (pOwner->pos.z + 1.0f);
        XMMATRIX world = XMMatrixScaling(s * pOwner->scale.x, s * pOwner->scale.y, 1.0f) *
            XMMatrixRotationZ(pOwner->rot.z) *
            XMMatrixTranslation(pOwner->pos.x, pOwner->pos.y, 0.0f);

        ConstantBuffer cb;
        cb.matWorld = XMMatrixTranspose(world);
        gfx->ImmediateContext->UpdateSubresource(cBuffer, 0, nullptr, &cb, 0, 0);
        gfx->ImmediateContext->VSSetConstantBuffers(0, 1, &cBuffer);

        UINT stride = sizeof(Vertex), offset = 0;
        gfx->ImmediateContext->IASetVertexBuffers(0, 1, &pMeshData->vBuffer, &stride, &offset);
        gfx->ImmediateContext->Draw(pMeshData->vertexCount, 0);
    }

    void Input() override {}
    void Update(float dt) override {}
};