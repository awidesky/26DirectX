
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>


#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") // 하위 시스템을 콘솔로 설정하고, 진입점은 WinMain으로 강제 지정
//#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:windows") // 하위 시스템을 윈도우로 설정하고, 진입점은 WinMain으로 강제 지정
//#pragma comment(linker, "/subsystem:console")

// # 원래는 하나의 라이브러리였음
#pragma comment(lib, "d3d11.lib") // # 컴팩트한 기능 제공
#pragma comment(lib, "dxgi.lib")  // # 하드웨어 추상화
#pragma comment(lib, "d3dcompiler.lib") // # programmable pipeline, shader, gpgpu 등등



ID3D11Device* g_pd3dDevice = nullptr;          // 리소스 생성자 (공장)
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // 그리기 명령 수행 (일꾼)
IDXGISwapChain* g_pSwapChain = nullptr;          // 화면 전환 (더블 버퍼링)
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // 그림을 그릴 도화지(View)