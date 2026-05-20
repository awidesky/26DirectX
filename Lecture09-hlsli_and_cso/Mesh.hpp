#pragma once
#include "Framework.hpp"
#include "GraphicsContext.hpp"

struct Mesh {
    ID3D11Buffer* vBuffer = nullptr;
    UINT vertexCount = 0;

    ~Mesh() {
        if (vBuffer) { vBuffer->Release(); vBuffer = nullptr; }
    }

    void Create(GraphicsContext* gfx, const std::vector<Vertex>& vertices) {
        vertexCount = (UINT)vertices.size();

        D3D11_BUFFER_DESC bd = { 0 };
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(Vertex) * vertexCount;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd = { 0 };
        sd.pSysMem = vertices.data();

        gfx->Device->CreateBuffer(&bd, &sd, &vBuffer);
    }
};