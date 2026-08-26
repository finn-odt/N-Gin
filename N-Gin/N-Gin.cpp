// N-Gin.cpp : Definiert den Einstiegspunkt für die Anwendung.
//


// TUTORIAL:
// https://learn.microsoft.com/en-us/windows/win32/direct3dgetstarted/complete-code-sample-for-using-a-corewindow-with-directx


#include "framework.h"
#include "N-Gin.h"

#include <vector>
#include <array>
#include <algorithm> //std::for_each
#include <random>
#include <string>
#include <sstream>
#include <cmath>
#include <chrono>
#include <iostream>

#include <d3d11.h>
#include <D3DX11.h>
#include <D3DX10.h>
#include <DirectXMath.h>

#include "Components.h"
#include "EntityManager.h"
#include "MeshManager.h"

using namespace DirectX;

#include <CommCtrl.h>

#pragma comment(lib, "Comctl32.lib")

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "D3DX11.lib")
#pragma comment (lib, "D3DX10.lib")

#define BASE_COLOR D3DXCOLOR(0.0f, 0.0f, 0.0f, 1.0f)  // BACKGROUND COLOR

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 750

#define SCREEN_WIDTH 920
#define SCREEN_HEIGHT 750

UINT renderWidth = 0;
UINT renderHeight = 0;

constexpr float MIN_NEAR_PLANE = 0.01f;
constexpr float MIN_PLANE_GAP = 0.01f;

struct Position;
struct Rotation;
struct Vertex;
struct Triangle;
struct Matrix3x3; 

EntityManager entityManager;
std::unique_ptr<MeshManager> meshManager;

// direct x pipeline (Interface Direct-X Global Interface Swap Chain)
IDXGISwapChain* swapchain;
ID3D11Device* dev;
ID3D11DeviceContext* devcon;
ID3D11RenderTargetView* backBuffer;
HWND hWnd;  // handler for window

HWND viewportWnd = nullptr;

HWND editPosX = nullptr;
HWND editPosY = nullptr;
HWND editPosZ = nullptr;
HWND editRotX = nullptr;
HWND editRotY = nullptr;
HWND editRotZ = nullptr;
HWND editFov = nullptr;
HWND editFar = nullptr;
HWND editNear = nullptr;

constexpr int INSPECTOR_WIDTH = 280;

// 3D Stuff
ID3D11Texture2D* depthStencilBuffer = nullptr;
ID3D11DepthStencilView* depthStencilView = nullptr;
// MVP matrices for geometry stage (Local -> World -> View -> Clip -> Screen)
XMMATRIX world;
XMMATRIX view;
XMMATRIX projection;

struct MatrixBufferData  // needs to correspond to the Data Structure in the Shader
{
    XMMATRIX world;
    XMMATRIX view;
    XMMATRIX projection;
};

// Shader
ID3D11InputLayout* layout;
ID3D11PixelShader* pixelShader;
ID3D11VertexShader* vertexShader;
ID3D11Buffer* matrixBuffer = nullptr;

// FORWARD METHOD DECLARATIONS

bool InitD3D(HWND Hwnd);
void Render(void);
void FixedUpdate(float fixedDeltaTime);
void CleanD3D(void);
bool InitGraphics(void);
bool InitPipeline(void);  // setup shaders, const. buffers, ...
void UpdateCameraInspector();
XMMATRIX GetWorldMatrix(const Transform& transform);
XMMATRIX GetProjectionMatrix(const Camera& camera, float aspectRatio);

// MOUSE INPUT (e.g. DRAG & DROP)
HWND hoveredEdit = nullptr;
HWND draggedEdit = nullptr;

bool editMouseDown = false;
bool editDragging = false;

int dragStartMouseX = 0;
int previousMouseX = 0;

constexpr int DRAG_THRESHOLD = 4;

bool updatingInspector = false;

// CAMERA
EntityId MainCamera;
bool MainCameraInitialized = false;

#define MAX_LOADSTRING 100

// Globale Variablen:
HINSTANCE hInst;                                // Aktuelle Instanz
WCHAR szTitle[MAX_LOADSTRING];                  // Titelleistentext
WCHAR szWindowClass[MAX_LOADSTRING];            // Der Klassenname des Hauptfensters.

// Vorwärtsdeklarationen der in diesem Codemodul enthaltenen Funktionen:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// APIENTRY 
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,  // handler for the current window-instance (points to current window)
                     _In_opt_ HINSTANCE hPrevInstance,  // handler for the previous window-instance (points to previous window that created current window)
                     _In_ LPWSTR    lpCmdLine,  // Long-Pointer White String [command line parameters]
                     _In_ int       nCmdShow)  // hów many parameters do we have?
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Globale Zeichenfolgen initialisieren
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NGIN, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);  // this function creates a window

    // Anwendungsinitialisierung ausführen:
    if (!InitInstance (hInstance, nCmdShow))  // call InitInstance with our instance
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NGIN));

    MSG msg;  // messages from Windows (OS)

    using Clock = std::chrono::steady_clock;

    constexpr float FIXED_DELTA_TIME = 1.0f / 16.0f; // 16 physics updates per second
    constexpr float MAX_FRAME_TIME   = 0.1f;        // prevents huge physics catch-up

    auto previousTime = Clock::now();
    float accumulator = 0.0f;

    while (true)
    {
        // PM_REMOVE = remove message from "mailbox"/queue
	    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	    {
            TranslateMessage(&msg);  // open (& handle if necessary)
            DispatchMessage(&msg);  // answer
            if (msg.message == WM_QUIT)  // program was exited
                break;
	    } else  // GAME CODE (MAIN LOOP)
	    {
            auto currentTime = Clock::now();

            std::chrono::duration<float> elapsedTime = currentTime - previousTime;
            previousTime = currentTime;
            float deltaTime = elapsedTime.count();

            // Avoid huge deltaTime after debugging, resizing, dragging window, etc.
            if (deltaTime > MAX_FRAME_TIME)
                deltaTime = MAX_FRAME_TIME;

            accumulator += deltaTime;

            while (accumulator >= FIXED_DELTA_TIME)
            {
                FixedUpdate(FIXED_DELTA_TIME);  // Physics Loop
                accumulator -= FIXED_DELTA_TIME;
            }

            Render();  // Game Loop
	    }
    }
    CleanD3D();

    return (int) msg.wParam;
}

float RandomFloat(float min, float max)  // both included
{
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

int RandomInt(int min, int max)  // both included
{
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

bool InitD3D(HWND Hwnd)
{
    HRESULT hr;

    RECT rect{};
    GetClientRect(Hwnd, &rect);
    renderWidth = static_cast<UINT>(rect.right - rect.left);
    renderHeight = static_cast<UINT>(rect.bottom - rect.top);

    // description:
    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));  // clear memory location

    scd.BufferCount = 1;  // only one swapchain
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // use 8-bit channels on colors
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = Hwnd;  // set our window
    scd.SampleDesc.Count = 4;  // anti-aliasing [1 = no, 4 = 4 pixel for anti-aliasing]
    // Multi-Sample-Anti-Aliasing (MSAA) is default in DirectX
    scd.Windowed = TRUE;
    //scd.BufferDesc.RefreshRate  -> limit frame rate with[denominator = 60 -> 60Hz]
    scd.BufferDesc.Width = renderWidth;
    scd.BufferDesc.Height = renderHeight;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL, D3D11_SDK_VERSION, &scd, &swapchain, &dev, NULL, &devcon);
    if (!CheckHR(hr, "Failed to create D3D11 device and swapchain"))
        return false;

    meshManager = std::make_unique<MeshManager>(dev);

    // BACK BUFFER
    ID3D11Texture2D* pBackBuffer;
    hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);  // convert pBackBuffer into void** (long pointer void)
    if (!CheckHR(hr, "Failed to get swapchain back buffer"))
        return false;

    hr = dev->CreateRenderTargetView(pBackBuffer, NULL, &backBuffer);  // pDesc is for defining ZBuffer, Stencil, ...
    pBackBuffer->Release();  // kill buffer afterwards
    if (!CheckHR(hr, "Failed to create render target view"))
        return false;

    // DEPTH BUFFER
    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = renderWidth;
    depthDesc.Height = renderHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 4;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = dev->CreateTexture2D(
        &depthDesc,
        nullptr,
        &depthStencilBuffer
    );

    if (!CheckHR(hr, "Failed to create depth stencil buffer"))
        return false;

    hr = dev->CreateDepthStencilView(
        depthStencilBuffer,
        nullptr,
        &depthStencilView
    );

    if (!CheckHR(hr, "Failed to create depth stencil view"))
        return false;

    devcon->OMSetRenderTargets(1, &backBuffer, depthStencilView);
	// OM = Output Merger
    // if pDepthStencilView is not set -> only 2D -> much faster

    // Viewport
    D3D11_VIEWPORT viewport;  // if variable is no pointer -> ZeroMemory()
    ZeroMemory(&viewport, sizeof(viewport));  // clear memory for use

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(renderWidth);
    viewport.Height = static_cast<float>(renderHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    devcon->RSSetViewports(1, &viewport);  // rasterizer stage -> set viewport

    if (!InitPipeline())  // shader setup
        return false;

    if (!InitGraphics())  // drawing setup
        return false;

    return true;
}

void Render(void)
{
    // do not draw on old frame:
    devcon->ClearRenderTargetView(backBuffer, BASE_COLOR);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (!MainCameraInitialized)
    {
        std::cout << "There is no camera in the scene!";
        return;
    }

    // Get Camera Components
    Transform* cameraTransform = entityManager.GetComponent<Transform>(MainCamera);
    Camera* camera = entityManager.GetComponent<Camera>(MainCamera);

    // Camera matrices
    XMVECTOR position = XMLoadFloat3(&cameraTransform->position);
    XMVECTOR rotation = XMLoadFloat4(&cameraTransform->rotation);

    XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        rotation
    );

    XMVECTOR up = XMVector3Rotate(
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        rotation
    );

    XMVECTOR target = XMVectorAdd(position, forward);

    XMMATRIX view = XMMatrixLookAtLH(  // View Matrix
        position,
        target,
        up
    );

    float aspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);

    projection = GetProjectionMatrix(*camera, aspect);

    // Pipeline state shared by all mesh objects
    devcon->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );
    
    devcon->VSSetConstantBuffers(  // b0 = MatrixBuffer in HLSL
        0,
        1,
        &matrixBuffer
    );

    entityManager.ForEach<Transform, MeshRenderer>(
        [&](EntityId entity, Transform& transform,MeshRenderer& renderer)
        {
            if (!renderer.visible)
                return;

            if (renderer.mesh == INVALID_MESH)
                return;

            // 1. Get GPU mesh from MeshHandle
            const Mesh& mesh = meshManager->GetMesh(renderer.mesh);

            // 2. World matrix comes from THIS entity's transform
            XMMATRIX world = GetWorldMatrix(transform);

            // 3. Upload this entity's W/V/P
            D3D11_MAPPED_SUBRESOURCE mappedResource{};

            HRESULT hr = devcon->Map(
                matrixBuffer,
                0,
                D3D11_MAP_WRITE_DISCARD,
                0,
                &mappedResource
            );

            if (FAILED(hr))
                return;

            MatrixBufferData* matrixData = static_cast<MatrixBufferData*>(mappedResource.pData);

            matrixData->world = XMMatrixTranspose(world);

            matrixData->view = XMMatrixTranspose(view);

            matrixData->projection = XMMatrixTranspose(projection);

            devcon->Unmap(matrixBuffer, 0);

            // 4. Bind THIS entity's mesh

            UINT stride = mesh.stride;
            UINT offset = 0;

            ID3D11Buffer* vertexBuffer = mesh.vertexBuffer.Get();

            devcon->IASetVertexBuffers(  // vertex buffer
                0,
                1,
                &vertexBuffer,
                &stride,
                &offset
            );

            devcon->IASetIndexBuffer(  // index buffer (for vertices)
                mesh.indexBuffer.Get(),
                DXGI_FORMAT_R32_UINT,
                0
            );


            // 5. Draw THIS entity
            devcon->DrawIndexed(mesh.indexCount,0,0);
        }
    );

    // Show finished frame
    swapchain->Present(0,0);
}

void FixedUpdate(float fixedDeltaTime) {
    // Physics goes here
    // Example:
    // position += velocity * fixedDeltaTime;
}

bool InitPipeline()  // CREATE SHADERS
{
    HRESULT hr;

    ID3DBlob *VS, *PS;  // binary linked object (for vertex and pixel shader)
    ID3DBlob *error;

    // COMPILE VERTEX AND PIXEL SHADER
    hr = D3DX11CompileFromFile(L"shader.shader", NULL, NULL, "VS", "vs_4_0", NULL, NULL, NULL, &VS, &error, NULL);
    if (FAILED(hr))
    {
        if (error)
        {
            MessageBoxA(
                nullptr,
                (char*)error->GetBufferPointer(),
                "Vertex Shader Compile Error",
                MB_OK | MB_ICONERROR
            );
            error->Release();
        }
        else
        {
            MessageBoxA(
                nullptr,
                "Could not find or compile shader.shader vertex shader.",
                "Vertex Shader Error",
                MB_OK | MB_ICONERROR
            );
        }

        return false;
    }

    hr = D3DX11CompileFromFile(L"shader.shader", NULL, NULL, "PS", "ps_4_0", NULL, NULL, NULL, &PS, &error, NULL);
    if (FAILED(hr))
    {
        if (error)
        {
            MessageBoxA(
                nullptr,
                (char*)error->GetBufferPointer(),
                "Pixel Shader Compile Error",
                MB_OK | MB_ICONERROR
            );
            error->Release();
        }
        else
        {
            MessageBoxA(
                nullptr,
                "Could not find or compile shader.shader pixel shader.",
                "Pixel Shader Error",
                MB_OK | MB_ICONERROR
            );
        }

        if (VS) VS->Release();
        return false;
    }

    hr = dev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &vertexShader);
    if (!CheckHR(hr, "Failed to create vertex shader"))
    {
        VS->Release();
        PS->Release();
        return false;
    }

    hr = dev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &pixelShader);
    if (!CheckHR(hr, "Failed to create pixel shader"))
    {
        VS->Release();
        PS->Release();
        return false;
    }

    // set shader for use in rendering
    // normally: select shader according to current Mesh/Object
    devcon->VSSetShader(vertexShader, NULL, NULL);
    devcon->PSSetShader(pixelShader, NULL, NULL);

    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,  // 3D -> 12 Byte
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "NORMAL",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,  // 3D -> 12 Byte
            0,
            12,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,  // 2D -> 8 Byte
            0,
            24,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    };

    hr = dev->CreateInputLayout(ied, 3, VS->GetBufferPointer(), VS->GetBufferSize(), &layout);
    if (!CheckHR(hr, "Failed to create input layout"))
    {
        VS->Release();
        PS->Release();
        return false;
    }

    devcon->IASetInputLayout(layout);  // change on different shader/pipeline

    // MATRIX CONSTANT BUFFER
    D3D11_BUFFER_DESC matrixBufferDesc{};
    matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    matrixBufferDesc.ByteWidth = sizeof(MatrixBufferData);
    matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // no reading required for CPU

    hr = dev->CreateBuffer(
        &matrixBufferDesc,
        nullptr,
        &matrixBuffer
    );

    if (!CheckHR(hr, "Failed to create matrix constant buffer"))
    {
        VS->Release();
        PS->Release();
        return false;
    }

    // HLSL: register(b0)
    devcon->VSSetConstantBuffers(0,1, &matrixBuffer);

    VS->Release();
    PS->Release();

    return true;
}

void SpawnObject(MeshHandle handle, const Transform& transform)
{
    EntityId entity = entityManager.AddEntity();

    entityManager.AddComponent(entity, transform);

    entityManager.AddComponent(
        entity,
        MeshRenderer{
            handle,
            INVALID_MATERIAL,
            true
        }
    );
}

void AddCamera(void)
{
    EntityId cameraEntity = entityManager.AddEntity();

    Transform cameraTransform;
    cameraTransform.position = { 0.0f, 0.0f, -15.0f };

    entityManager.AddComponent(cameraEntity, cameraTransform);

    entityManager.AddComponent(cameraEntity, Camera{});

    if (!MainCameraInitialized)
    {
        MainCamera = cameraEntity;
        MainCameraInitialized = true;
    }
}

bool InitGraphics()
{
    AddCamera();  // add main camera

    try
    {

        MeshHandle mesh = meshManager->LoadMesh("Assets/testmodel.fbx");

        int n = 20;
        for (int i = 0; i < n; i++)
        {
            for (int k = 0; k < n; k++)
            {
                Transform transform;
                transform.position = {5.0f*i - n/2.0f, 0.0f, 5.0f*k - n/2.0f };
                XMStoreFloat4(
                    &transform.rotation,
                    XMQuaternionRotationRollPitchYaw(
                        -XM_PIDIV2,       // X (pitch)
                        XM_PI,  // Y (yaw) [PI/2 = 90°]
                        0.0f        // Z (roll)
                    )
                );
                transform.scale = { 1.0f, 1.0f, 1.0f };

                SpawnObject(mesh, transform);
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        MessageBoxA(
            nullptr,
            e.what(),
            "Mesh loading failed",
            MB_OK | MB_ICONERROR
        );

        return false;
    }
}

void CleanD3D(void)
{
    // Release() basically calls free() with internal logic
    if (swapchain)
        swapchain->SetFullscreenState(false, nullptr);

    // 1. Unbind resources from the pipeline/context first
    if (devcon)
    {
        devcon->ClearState();
        devcon->Flush();
    }

    // 2. Release GPU resources / views / pipeline objects
    if (matrixBuffer)
    {
        matrixBuffer->Release();
        matrixBuffer = nullptr;
    }

    if (layout)
    {
        layout->Release();
        layout = nullptr;
    }

    if (vertexShader)
    {
	    vertexShader->Release();
    	vertexShader = nullptr;
    }

    if (pixelShader)
    {
	    pixelShader->Release();
        pixelShader = nullptr;
    }

    if (depthStencilView)
    {
        depthStencilView->Release();
        depthStencilView = nullptr;
    }

    if (depthStencilBuffer)
    {
        depthStencilBuffer->Release();
        depthStencilBuffer = nullptr;
    }

    if (backBuffer)
    {
        backBuffer->Release();
        backBuffer = nullptr;
    }

    // release all loaded meshes
    meshManager.reset();

    // 3. Release the device context
    if (devcon)
    {
        devcon->Release();
        devcon = nullptr;
    }

    // 4. Release the swap chain
    if (swapchain)
    {
        swapchain->Release();
        swapchain = nullptr;
    }

    // 5. Release the device last
    if (dev)
    {
        dev->Release();
        dev = nullptr;
    }
}

//
//  FUNKTION: MyRegisterClass()
//
//  ZWECK: Registriert die Fensterklasse.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;  // window class extended window (full Windows11-stuff)

    wcex.cbSize = sizeof(WNDCLASSEX);  // size [bytes] in memory for an instance of this class

    wcex.style          = CS_HREDRAW | CS_VREDRAW;  // vertically and horizontally redrawn (else: not updated)
    wcex.lpfnWndProc    = WndProc;  // long pointer windows processing -> our own callback function for messages
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;  // instance
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NGIN));  // icon
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);  // cursor
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);  // color
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_NGIN);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

#define IDC_POSITION_X 1001
#define IDC_POSITION_Y 1002
#define IDC_POSITION_Z 1003
#define IDC_ROTATION_X 1004
#define IDC_ROTATION_Y 1005
#define IDC_ROTATION_Z 1006

#define IDC_FOV        1010
#define IDC_NEAR_PLANE 1011
#define IDC_FAR_PLANE  1012

//
//   FUNKTION: InitInstance(HINSTANCE, int)
//
//   ZWECK: Speichert das Instanzenhandle und erstellt das Hauptfenster.
//
//   KOMMENTARE:
//
//        In dieser Funktion wird das Instanzenhandle in einer globalen Variablen gespeichert, und das
//        Hauptprogrammfenster wird erstellt und angezeigt.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int x = (screenWidth - WINDOW_WIDTH) / 2;
    int y = (screenHeight - WINDOW_HEIGHT) / 2;

    // main window for the whole process/program
    hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x,
        y,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
        return FALSE;

    RECT clientRect{};
    GetClientRect(hWnd, &clientRect);

    int width = clientRect.right;
    int height = clientRect.bottom;

    int viewportWidth = width - INSPECTOR_WIDTH;

    HFONT inspectorFont = CreateFontW(
        15,                 // height in pixels
        0, 0, 0,
        FW_NORMAL,          // or FW_BOLD
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
    );

    // LEFT: DirectX viewport
    viewportWnd = CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0,
        0,
        viewportWidth,
        height,
        hWnd,
        nullptr,
        hInstance,
        nullptr
    );

    // RIGHT: Inspector
    int inspectorX = viewportWidth + 15;

    CreateWindowW(L"STATIC", L"CAMERA", WS_CHILD | WS_VISIBLE, inspectorX, 15, 200, 25, hWnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Transform", WS_CHILD | WS_VISIBLE, inspectorX + 10, 55, 200, 20, hWnd, nullptr, hInstance, nullptr);

    // X POSITION
    HWND positionXLabel = 
        CreateWindowW(L"STATIC", L"Position X", WS_CHILD | WS_VISIBLE, inspectorX + 20, 90, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(positionXLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editPosX = 
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 87, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_POSITION_X), hInstance, nullptr);

    // Y POSITION
    HWND positionYLabel = 
        CreateWindowW(L"STATIC", L"Position Y", WS_CHILD | WS_VISIBLE, inspectorX + 20, 115, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(positionYLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editPosY = 
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 112, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_POSITION_Y), hInstance, nullptr);
    
    // Z POSITION
    HWND positionZLabel = 
        CreateWindowW(L"STATIC", L"Position Z", WS_CHILD | WS_VISIBLE, inspectorX + 20, 140, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(positionZLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editPosZ = 
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 137, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_POSITION_Z), hInstance, nullptr);


    // X ROTATION
    HWND rotationXLabel =
        CreateWindowW(L"STATIC", L"Rotation X", WS_CHILD | WS_VISIBLE, inspectorX + 20, 170, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(rotationXLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editRotX =
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 167, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_ROTATION_X), hInstance, nullptr);

    // Y ROTATION
    HWND rotationYLabel =
        CreateWindowW(L"STATIC", L"Rotation Y", WS_CHILD | WS_VISIBLE, inspectorX + 20, 195, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(rotationYLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editRotY =
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 192, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_ROTATION_Y), hInstance, nullptr);

    // Z ROTATION
    HWND rotationZLabel =
        CreateWindowW(L"STATIC", L"Rotation Z", WS_CHILD | WS_VISIBLE, inspectorX + 20, 220, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(rotationZLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editRotZ =
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 217, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_ROTATION_Z), hInstance, nullptr);


    CreateWindowW(L"STATIC", L"Camera", WS_CHILD | WS_VISIBLE, inspectorX + 10, 250, 200, 20, hWnd, nullptr, hInstance, nullptr);

    // FOV
    HWND camFovLabel = 
        CreateWindowW(L"STATIC", L"FOV", WS_CHILD | WS_VISIBLE, inspectorX + 20, 275, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(camFovLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editFov = 
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 272, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_FOV), hInstance, nullptr);

    // Near Plane
    HWND camNearLabel = 
        CreateWindowW(L"STATIC", L"Near Plane", WS_CHILD | WS_VISIBLE, inspectorX + 20, 300, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(camNearLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editNear = 
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 297, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_NEAR_PLANE), hInstance, nullptr);

    // Far Plane
    HWND camFarLabel = 
        CreateWindowW(L"STATIC", L"Far Plane", WS_CHILD | WS_VISIBLE, inspectorX + 20, 325, 70, 20, hWnd, nullptr, hInstance, nullptr);
    SendMessageW(camFarLabel, WM_SETFONT, reinterpret_cast<WPARAM>(inspectorFont), TRUE);  // change font

    editFar = 
        CreateWindowW(L"EDIT", L"0.0", WS_CHILD | WS_VISIBLE | WS_BORDER, inspectorX + 100, 322, 140, 24, hWnd, reinterpret_cast<HMENU>(IDC_FAR_PLANE), hInstance, nullptr);
	
    SetWindowSubclass(editPosX, EditSubclassProc, 0, 0);
    SetWindowSubclass(editPosY, EditSubclassProc, 0, 0);
    SetWindowSubclass(editPosZ, EditSubclassProc, 0, 0);

    SetWindowSubclass(editRotX, EditSubclassProc, 0, 0);
    SetWindowSubclass(editRotY, EditSubclassProc, 0, 0);
    SetWindowSubclass(editRotZ, EditSubclassProc, 0, 0);

    SetWindowSubclass(editFov, EditSubclassProc, 0, 0);
    SetWindowSubclass(editNear, EditSubclassProc, 0, 0);
    SetWindowSubclass(editFar, EditSubclassProc, 0, 0);

    if (!InitD3D(viewportWnd))
    {
        MessageBoxA(
            hWnd,
            "DirectX initialization failed.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return FALSE;
    }
	
	// MainCamera exists now
    UpdateCameraInspector();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

XMFLOAT3 QuaternionToEulerDegrees(const XMFLOAT4& rotation)
{
    XMVECTOR qVector = XMQuaternionNormalize(
        XMLoadFloat4(&rotation)
    );

    XMFLOAT4 q;
    XMStoreFloat4(&q, qVector);

    // X = pitch
    float sinPitch = 2.0f * (q.w * q.x - q.y * q.z);
    sinPitch = std::clamp(sinPitch, -1.0f, 1.0f);

    float pitch = std::asin(sinPitch);

    // Y = yaw
    float yaw = std::atan2(
        2.0f * (q.w * q.y + q.x * q.z),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y)
    );

    // Z = roll
    float roll = std::atan2(
        2.0f * (q.w * q.z + q.x * q.y),
        1.0f - 2.0f * (q.x * q.x + q.z * q.z)
    );

    return {
        XMConvertToDegrees(pitch),
        XMConvertToDegrees(yaw),
        XMConvertToDegrees(roll)
    };
}

XMVECTOR EulerDegreesToQuaternion(const XMFLOAT3& eulerRotation)
{
    XMVECTOR quaternion =
        XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(eulerRotation.x), // pitch, X
            XMConvertToRadians(eulerRotation.y), // yaw,   Y
            XMConvertToRadians(eulerRotation.z)  // roll,  Z
        );

    return quaternion;
}

void UpdateCameraInspector()
{
    if (!MainCameraInitialized)
        return;

    Transform* transform = entityManager.GetComponent<Transform>(MainCamera);

    Camera* camera = entityManager.GetComponent<Camera>(MainCamera);

    if (!transform || !camera)
        return;

    updatingInspector = true;

    wchar_t buffer[64];

    // Position X
    swprintf_s(buffer, L"%.2f", transform->position.x);
    SetWindowTextW(editPosX, buffer);

    // Position Y
    swprintf_s(buffer, L"%.2f", transform->position.y);
    SetWindowTextW(editPosY, buffer);

    // Position Z
    swprintf_s(buffer, L"%.2f", transform->position.z);
    SetWindowTextW(editPosZ, buffer);


    // Quaternion -> Euler degrees
    XMFLOAT3 rotation = QuaternionToEulerDegrees(transform->rotation);

    // Rotation X
    swprintf_s(buffer, L"%.2f", rotation.x);
    SetWindowTextW(editRotX, buffer);

    // Rotation Y
    swprintf_s(buffer, L"%.2f", rotation.y);
    SetWindowTextW(editRotY, buffer);

    // Rotation Z
    swprintf_s(buffer, L"%.2f", rotation.z);
    SetWindowTextW(editRotZ, buffer);


    // FOV: radians -> degrees for inspector
    swprintf_s(buffer,L"%.2f", XMConvertToDegrees(camera->fov));
    SetWindowTextW(editFov, buffer);

    // Near plane
    swprintf_s(buffer, L"%.2f", camera->nearPlane);
    SetWindowTextW(editNear, buffer);

    // Far plane
    swprintf_s(buffer, L"%.2f", camera->farPlane);
    SetWindowTextW(editFar, buffer);

    updatingInspector = false;
}

//
//  FUNKTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ZWECK: Verarbeitet Meldungen für das Hauptfenster.
//
//  WM_COMMAND  - Verarbeiten des Anwendungsmenüs
//  WM_PAINT    - Darstellen des Hauptfensters
//  WM_DESTROY  - Ausgeben einer Beendenmeldung und zurückkehren
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	    case WM_COMMAND:
	    {
	        int controlId = LOWORD(wParam);
	        int notification = HIWORD(wParam); 

	        // --------------------------------------------------------
	        // MENU COMMANDS
	        // --------------------------------------------------------

	        switch (controlId)
	        {
		        case IDM_ABOUT:
		            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
		            return 0;

		        case IDM_EXIT:
		            DestroyWindow(hWnd);
		            return 0;
	        }

	        // --------------------------------------------------------
	        // INSPECTOR INPUT
	        // --------------------------------------------------------

            if (notification == EN_CHANGE && MainCameraInitialized && !updatingInspector)
	        {
	            HWND control = reinterpret_cast<HWND>(lParam);

	            Transform* transform =
	                entityManager.GetComponent<Transform>(MainCamera);

	            Camera* camera =
	                entityManager.GetComponent<Camera>(MainCamera);

	            if (!transform || !camera)
	                return 0;

	            wchar_t text[64]{};
	            GetWindowTextW(control, text, 64);

	            wchar_t* end = nullptr;
	            float value = std::wcstof(text, &end);

	            // Nothing valid could be parsed
	            if (end == text)
	                return 0;

	            // ----------------------------------------------------
	            // TRANSFORM
	            // ----------------------------------------------------

	            if (control == editPosX)
	            {
	                transform->position.x = value;
	            }
	            else if (control == editPosY)
	            {
	                transform->position.y = value;
	            }
	            else if (control == editPosZ)
	            {
	                transform->position.z = value;
	            }
                else if (control == editRotX || control == editRotY || control == editRotZ)
                {
                    wchar_t textX[64]{};
                    wchar_t textY[64]{};
                    wchar_t textZ[64]{};

                    // read in all three rotation input fields
                    GetWindowTextW(editRotX, textX, 64);
                    GetWindowTextW(editRotY, textY, 64);
                    GetWindowTextW(editRotZ, textZ, 64);

                    wchar_t* endX = nullptr;
                    wchar_t* endY = nullptr;
                    wchar_t* endZ = nullptr;

                    float rotX = std::wcstof(textX, &endX);  // wchar_t* -> float [conversion]
                    float rotY = std::wcstof(textY, &endY);
                    float rotZ = std::wcstof(textZ, &endZ);

                    if (endX == textX || endY == textY || endZ == textZ)
                        return 0;

                    XMVECTOR quaternion =
                        XMQuaternionRotationRollPitchYaw(
                            XMConvertToRadians(rotX), // pitch, X
                            XMConvertToRadians(rotY), // yaw,   Y
                            XMConvertToRadians(rotZ)  // roll,  Z
                        );

                    XMStoreFloat4(&transform->rotation, quaternion);
                }

	            // ----------------------------------------------------
	            // CAMERA
	            // ----------------------------------------------------

	            else if (control == editFov)
	            {
	                // Inspector uses degrees,
	                // Camera component stores radians
	                camera->fov = XMConvertToRadians(value);
	            }
	            else if (control == editNear)
	            {
	                // Near plane must be positive
	                if (value > MIN_NEAR_PLANE && value < camera->farPlane - MIN_PLANE_GAP)
	                    camera->nearPlane = value;
	            }
	            else if (control == editFar)
	            {
	                // Far plane must be beyond near plane
	                if (value > camera->nearPlane + MIN_PLANE_GAP)
	                    camera->farPlane = value;
	            }
	        }

	        return 0;
	    }

	    case WM_PAINT:
	    {
	        PAINTSTRUCT ps{};
	        HDC hdc = BeginPaint(hWnd, &ps);

	        // Native Win32 child controls paint themselves.
	        // DirectX renders separately into viewportWnd.

	        EndPaint(hWnd, &ps);

	        return 0;
	    }

	    case WM_DESTROY:
	    {
	        PostQuitMessage(0);
	        return 0;
	    }

	    default:
	        return DefWindowProc(
	            hWnd,
	            message,
	            wParam,
	            lParam
	        );
    }
}

/**
 * Callback Function for Mouse-Events on HWND-objects.
 *
 * @param hwnd
 * @param message
 * @param wParam
 * @param lParam
 * @param subclassId
 * @param refData
 * @return
 */
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData)
{
    if (!MainCameraInitialized)
        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam
        );

    switch (message)
    {
        case WM_MOUSEMOVE:
        {
	    	// hovered-HWND not set yet (mouse entry on this HWND)
            if (hoveredEdit != hwnd)
            {
                hoveredEdit = hwnd;

                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(TRACKMOUSEEVENT);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;

                TrackMouseEvent(&track);
            }

            // Mouse position in SCREEN coordinates
            POINT mouse{};
            GetCursorPos(&mouse);

            // Click -> drag transition
            if (editMouseDown && draggedEdit == hwnd && !editDragging)
            {
                int distance =
                    std::abs(mouse.x - dragStartMouseX);

                if (distance >= DRAG_THRESHOLD)
                {
                    editDragging = true;

                    // Start from the threshold position.
                    previousMouseX = mouse.x;
                }
            }

            // Actual numeric dragging
            if (editDragging &&draggedEdit == hwnd)
            {
                int deltaX = mouse.x - previousMouseX;

                if (deltaX != 0)
                {
                    Transform* transform = entityManager.GetComponent<Transform>(MainCamera);

                    Camera* camera = entityManager.GetComponent<Camera>(MainCamera);

                    if (!transform || !camera)
                        break;

                    float sensitivity = 0.1f;  // TODO: GLOBAL VARIABLE
                    float change = static_cast<float>(deltaX) * sensitivity;

                    if (hwnd == editPosX)
                        transform->position.x += change;
                    else if (hwnd == editPosY)
                        transform->position.y += change;
                    else if (hwnd == editPosZ)
                        transform->position.z += change;
                    else if (hwnd == editRotX || hwnd == editRotY || hwnd == editRotZ) {
                        // read in all three rotation input fields
                        wchar_t textX[64]{}, textY[64]{}, textZ[64]{};
                        GetWindowTextW(editRotX, textX, 64);
                        GetWindowTextW(editRotY, textY, 64);
                        GetWindowTextW(editRotZ, textZ, 64);

                        wchar_t* endX = nullptr;
                        wchar_t* endY = nullptr;
                        wchar_t* endZ = nullptr;

                        float rotX = std::wcstof(textX, &endX);  // wchar_t* -> float [conversion]
                        float rotY = std::wcstof(textY, &endY);
                        float rotZ = std::wcstof(textZ, &endZ);

                        if (endX == textX || endY == textY || endZ == textZ)
                            return 0;

                        XMVECTOR quaternion =
                            XMQuaternionRotationRollPitchYaw(
                                XMConvertToRadians(rotX + (hwnd == editRotX ? change : 0)), // pitch, X
                                XMConvertToRadians(rotY + (hwnd == editRotY ? change : 0)), // yaw,   Y
                                XMConvertToRadians(rotZ + (hwnd == editRotZ ? change : 0))  // roll,  Z
                            );

                        XMStoreFloat4(&transform->rotation, quaternion);
                    }
                    else if (hwnd == editFov)
                        camera->fov += XMConvertToRadians(change);
                    else if (hwnd == editNear)
                    {
                        float newNearPlane = camera->nearPlane + change;

                        // > MIN_NEAR_PLANE && < FAR-PLANE
                        newNearPlane = std::clamp(
                            newNearPlane,
                            MIN_NEAR_PLANE,
                            camera->farPlane - MIN_PLANE_GAP
                        );

                        camera->nearPlane = newNearPlane;
                    }
                    else if (hwnd == editFar)
                    {
                        float newFarPlane = camera->farPlane + change;

                        // >= Near-Plane
                        newFarPlane = std::max<float>(
                            newFarPlane,
                            camera->nearPlane + MIN_PLANE_GAP
                        );

                        camera->farPlane = newFarPlane;
                    }

                    UpdateCameraInspector();

                    previousMouseX = mouse.x;
                }

                // Don't let EDIT select text while numeric dragging.
                return 0;
            }

            // Not dragging -> normal text selection behavior.
            break;
        }

	    case WM_MOUSELEAVE:
        {
            if (hoveredEdit == hwnd)
                hoveredEdit = nullptr;

            break;
        }

        case WM_LBUTTONDOWN:
        {
            editMouseDown = true;
            editDragging = false;
            draggedEdit = hwnd;

            POINT mouse{};
            GetCursorPos(&mouse);

            dragStartMouseX = mouse.x;
            previousMouseX = mouse.x;

            // WE own the mouse gesture until we know
            // whether this is a click or drag.
            SetCapture(hwnd);

            // Do NOT call DefSubclassProc here.
            return 0;
        }

        case WM_LBUTTONUP:
        {
            if (draggedEdit != hwnd)
                break;

            bool wasDragging = editDragging;

            editMouseDown = false;
            editDragging = false;
            draggedEdit = nullptr;

            if (GetCapture() == hwnd)
                ReleaseCapture();

            if (!wasDragging)
            {
                // It was a CLICK, not a drag.
                // Give keyboard focus to the edit box.
                SetFocus(hwnd);

                // Select the complete numeric value.
                SendMessageW(
                    hwnd,
                    EM_SETSEL,
                    0,
                    -1
                );
            }

            return 0;
        }

        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        {
            editMouseDown = false;
            editDragging = false;
            draggedEdit = nullptr;

            break;
        }
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

// Meldungshandler für Infofeld.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

XMMATRIX GetWorldMatrix(const Transform& transform)
{
    using namespace DirectX;

    XMMATRIX scale = XMMatrixScaling(
        transform.scale.x,
        transform.scale.y,
        transform.scale.z
    );

    XMMATRIX rotation = XMMatrixRotationQuaternion(
        XMLoadFloat4(&transform.rotation)
    );

    XMMATRIX translation = XMMatrixTranslation(
        transform.position.x,
        transform.position.y,
        transform.position.z
    );

    return scale * rotation * translation;
}

XMMATRIX GetProjectionMatrix(const Camera& camera, float aspectRatio)
{
    if (camera.projectionType == ProjectionType::Perspective)
    {
        return XMMatrixPerspectiveFovLH(
            camera.fov,
            aspectRatio,
            camera.nearPlane,
            camera.farPlane
        );
    }

    float height = camera.orthographicSize;
    float width = height * aspectRatio;

    return XMMatrixOrthographicLH(
        width,
        height,
        camera.nearPlane,
        camera.farPlane
    );
}

/*
#include <windows.h>

RECT GetMonitorResolutionForWindow(HWND hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfo(monitor, &mi))
    {
        return mi.rcMonitor; // full monitor rectangle in virtual-screen coordinates
    }

    return {};
}

RECT r = GetMonitorResolutionForWindow(hwnd);

int width  = r.right - r.left;
int height = r.bottom - r.top;

If you want the usable desktop area excluding taskbar, use:
int workWidth  = mi.rcWork.right - mi.rcWork.left;
int workHeight = mi.rcWork.bottom - mi.rcWork.top;
 **/

std::vector<Vertex> GetCubeVertices()
{
    return {
        // ========================================================
        // FRONT (-Z)
        // ========================================================
        {
            {-1.0f, -1.0f, -1.0f},   // position
            { 0.0f,  0.0f, -1.0f},   // normal
            { 0.0f,  1.0f}            // UV
        },
        {
            {-1.0f,  1.0f, -1.0f},
            { 0.0f,  0.0f, -1.0f},
            { 0.0f,  0.0f}
        },
        {
            { 1.0f,  1.0f, -1.0f},
            { 0.0f,  0.0f, -1.0f},
            { 1.0f,  0.0f}
        },

        {
            {-1.0f, -1.0f, -1.0f},
            { 0.0f,  0.0f, -1.0f},
            { 0.0f,  1.0f}
        },
        {
            { 1.0f,  1.0f, -1.0f},
            { 0.0f,  0.0f, -1.0f},
            { 1.0f,  0.0f}
        },
        {
            { 1.0f, -1.0f, -1.0f},
            { 0.0f,  0.0f, -1.0f},
            { 1.0f,  1.0f}
        },


        // ========================================================
        // BACK (+Z)
        // ========================================================
        {
            {-1.0f, -1.0f,  1.0f},
            { 0.0f,  0.0f,  1.0f},
            { 1.0f,  1.0f}
        },
        {
            { 1.0f,  1.0f,  1.0f},
            { 0.0f,  0.0f,  1.0f},
            { 0.0f,  0.0f}
        },
        {
            {-1.0f,  1.0f,  1.0f},
            { 0.0f,  0.0f,  1.0f},
            { 1.0f,  0.0f}
        },

        {
            {-1.0f, -1.0f,  1.0f},
            { 0.0f,  0.0f,  1.0f},
            { 1.0f,  1.0f}
        },
        {
            { 1.0f, -1.0f,  1.0f},
            { 0.0f,  0.0f,  1.0f},
            { 0.0f,  1.0f}
        },
        {
            { 1.0f,  1.0f,  1.0f},
            { 0.0f,  0.0f,  1.0f},
            { 0.0f,  0.0f}
        },


        // ========================================================
        // LEFT (-X)
        // ========================================================
        {
            {-1.0f, -1.0f,  1.0f},
            {-1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            {-1.0f,  1.0f,  1.0f},
            {-1.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f}
        },
        {
            {-1.0f,  1.0f, -1.0f},
            {-1.0f,  0.0f,  0.0f},
            { 1.0f,  0.0f}
        },

        {
            {-1.0f, -1.0f,  1.0f},
            {-1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            {-1.0f,  1.0f, -1.0f},
            {-1.0f,  0.0f,  0.0f},
            { 1.0f,  0.0f}
        },
        {
            {-1.0f, -1.0f, -1.0f},
            {-1.0f,  0.0f,  0.0f},
            { 1.0f,  1.0f}
        },


        // ========================================================
        // RIGHT (+X)
        // ========================================================
        {
            { 1.0f, -1.0f, -1.0f},
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            { 1.0f,  1.0f, -1.0f},
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f}
        },
        {
            { 1.0f,  1.0f,  1.0f},
            { 1.0f,  0.0f,  0.0f},
            { 1.0f,  0.0f}
        },

        {
            { 1.0f, -1.0f, -1.0f},
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            { 1.0f,  1.0f,  1.0f},
            { 1.0f,  0.0f,  0.0f},
            { 1.0f,  0.0f}
        },
        {
            { 1.0f, -1.0f,  1.0f},
            { 1.0f,  0.0f,  0.0f},
            { 1.0f,  1.0f}
        },


        // ========================================================
        // TOP (+Y)
        // ========================================================
        {
            {-1.0f,  1.0f, -1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            {-1.0f,  1.0f,  1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f,  0.0f}
        },
        {
            { 1.0f,  1.0f,  1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 1.0f,  0.0f}
        },

        {
            {-1.0f,  1.0f, -1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            { 1.0f,  1.0f,  1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 1.0f,  0.0f}
        },
        {
            { 1.0f,  1.0f, -1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 1.0f,  1.0f}
        },


        // ========================================================
        // BOTTOM (-Y)
        // ========================================================
        {
            {-1.0f, -1.0f,  1.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            {-1.0f, -1.0f, -1.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  0.0f}
        },
        {
            { 1.0f, -1.0f, -1.0f},
            { 0.0f, -1.0f,  0.0f},
            { 1.0f,  0.0f}
        },

        {
            {-1.0f, -1.0f,  1.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  1.0f}
        },
        {
            { 1.0f, -1.0f, -1.0f},
            { 0.0f, -1.0f,  0.0f},
            { 1.0f,  0.0f}
        },
        {
            { 1.0f, -1.0f,  1.0f},
            { 0.0f, -1.0f,  0.0f},
            { 1.0f,  1.0f}
        }
    };
}




