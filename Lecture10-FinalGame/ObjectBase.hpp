#pragma once
#include "Framework.hpp"
#include "GraphicsContext.hpp"

// 전방 선언: "자세한 건 나중에 알려줄게, 일단 이런 클래스가 있어"
class GameObject;

class Component
{
public:
    GameObject* pOwner = nullptr;
    bool isStarted = false;

    virtual void Start(GraphicsContext* gfx) = 0;
    virtual void Input() = 0;
    virtual void Update(float dt) = 0;
    virtual void Render(GraphicsContext* gfx) = 0;
    virtual ~Component() {}

    // ★ 유니티 스타일 충돌 이벤트 인터페이스 추가
    virtual void OnCollisionEnter(GameObject* other) {}
    virtual void OnCollisionStay(GameObject* other) {}
    virtual void OnCollisionExit(GameObject* other) {}
};

class GameObject
{
public:
    XMFLOAT3 pos = { 0, 0, 0 };
    XMFLOAT3 rot = { 0, 0, 0 };
    XMFLOAT3 scale = { 1, 1, 1 };
    std::vector<Component*> components;

    GameObject(float x, float y, float z) { pos = { x, y, z }; }
    ~GameObject() { for (auto c : components) delete c; }

    void AddComponent(Component* c)
    {
        c->pOwner = this;
        components.push_back(c);
    }

    void Input() { for (auto c : components) c->Input(); }
    void Update(float dt, GraphicsContext* gfx)
    {
        for (auto c : components)
        {
            if (!c->isStarted)
            {
                c->Start(gfx);
                c->isStarted = true;
            }
            c->Update(dt);
        }
    }
    void Render(GraphicsContext* gfx) { for (auto c : components) c->Render(gfx); }

    // ★ 충돌 발생 시 모든 컴포넌트에 이벤트를 전파하는 라우팅 함수
    void FireCollisionEnter(GameObject* other) { for (auto c : components) c->OnCollisionEnter(other); }
    void FireCollisionStay(GameObject* other) { for (auto c : components) c->OnCollisionStay(other); }
    void FireCollisionExit(GameObject* other) { for (auto c : components) c->OnCollisionExit(other); }
};