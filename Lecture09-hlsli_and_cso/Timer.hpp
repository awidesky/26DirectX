#pragma once
#include "Framework.hpp"

class DeltaTime {
    std::chrono::high_resolution_clock::time_point prevTime;
public:
    DeltaTime() {
        prevTime = std::chrono::high_resolution_clock::now();
    }

    float GetDelta() {
        auto currTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currTime - prevTime).count();
        prevTime = currTime;
        return dt;
    }
};