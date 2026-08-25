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
using namespace DirectX;


#include "Camera.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "D3DX11.lib")
#pragma comment (lib, "D3DX10.lib")

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define BASE_COLOR D3DXCOLOR(0.0f, 0.0f, 0.0f, 1.0f)
#define SHAPE_SCALE 0.1
#define PI 3.14159265358979323846f

struct Position;
struct Rotation;
struct Vertex;
struct Triangle;
struct Matrix3x3; 

float cubeRotation = 0.0f;

// direct x pipeline (Interface Direct-X Global Interface Swap Chain)
IDXGISwapChain* swapchain;
ID3D11Device* dev;
ID3D11DeviceContext* devcon;
ID3D11RenderTargetView* backBuffer;
HWND hWnd;  // handler for window

// 3D Stuff
ID3D11Texture2D* depthStencilBuffer = nullptr;
ID3D11DepthStencilView* depthStencilView = nullptr;
// Camera
Camera camera;
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
ID3D11Buffer* vertexBuffer;
ID3D11Buffer* matrixBuffer = nullptr;

UINT vertexBufferByteSize;
UINT numberOfBufferVertices;

std::vector<Triangle> triangles;
std::vector<Vertex> testVertices;
bool graphicsUpdateRequired = false;

bool InitD3D(HWND Hwnd);
void Render(void);
void FixedUpdate(float fixedDeltaTime);
void CleanD3D(void);
bool InitGraphics(void);
bool UpdateGraphics(void);
bool InitPipeline(void);  // setup shaders, const. buffers, ...

XMMATRIX GetWorldMatrix(const Transform& transform);
XMMATRIX GetProjectionMatrix(const Camera& camera, float aspectRatio);

void UpdateTriangles(void);
float DegreesToRadians(float degrees);
Matrix3x3 GetRotationMatrix(float eulerX, float eulerY, float eulerZ);
Triangle GetRandomTriangle(float xPosition, float yPosition, Position vel);
std::vector<Vertex> TrianglesToLinearVertices();

#define MAX_LOADSTRING 100

// Globale Variablen:
HINSTANCE hInst;                                // Aktuelle Instanz
WCHAR szTitle[MAX_LOADSTRING];                  // Titelleistentext
WCHAR szWindowClass[MAX_LOADSTRING];            // Der Klassenname des Hauptfensters.

// Vorwärtsdeklarationen der in diesem Codemodul enthaltenen Funktionen:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

inline float Length3(float x, float y, float z) noexcept
{
    return std::sqrt(x * x + y * y + z * z);
}

struct Position {
    FLOAT X, Y, Z;

    float Length() const
    {
        return Length3(X, Y, Z);
    }

    Position Normalize() const
    {
        Position p = *this;
        float d = Length();

        if (d > 0.0f)  // do not divide by 0
        {
            p.X /= d;
            p.Y /= d;
            p.Z /= d;
        }

        return p;
    }

    Position& operator*=(const float& f) noexcept
    {
        X *= f;
        Y *= f;
        Z *= f;
        return *this;
    }

    Position operator*(const float& f) noexcept
    {
        Position p = *this;
        p.X *= f;
        p.Y *= f;
        p.Z *= f;
        return p;
    }

    Position& operator+=(const Position& p) noexcept
    {
        X += p.X;
        Y += p.Y;
        Z += p.Z;
        return *this;
    }

    Position& operator+=(const float& f) noexcept
    {
        X += f;
        Y += f;
        Z += f;
        return *this;
    }
};
struct Rotation {
    FLOAT X, Y, Z;

    Rotation& operator*=(const float& f) noexcept
    {
        X *= f;
        Y *= f;
        Z *= f;
        return *this;
    }
};
struct Vertex
{
    Position POS;
    D3DXCOLOR Color;
    Vertex& operator+=(const Position& p) noexcept
    {
        POS.X += p.X;
        POS.Y += p.Y;
        POS.Z += p.Z;
        return *this;
    }

    Vertex operator+(const Position& p) const noexcept
    {
        Vertex result = *this;
        result += p;
        return result;
    }

    Vertex operator*(const float& f) const noexcept
    {
        Vertex result = *this;
        result.POS *= f;
        return result;
    }
};
struct Matrix3x3 {
    std::array<std::array<float, 3>, 3> data = {{
        {{0.0f, 0.0f, 0.0f}},  // first row
        {{0.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}}
    }};

    std::array<float, 3>& operator[](int row) noexcept
    {
        return data[row];
    }

    const std::array<float, 3>& operator[](int row) const noexcept
    {
        return data[row];
    }

    Matrix3x3& operator*=(const Matrix3x3& m) noexcept
    {
        *this = *this * m;
        return *this;
    }

    Matrix3x3 operator*(const Matrix3x3& m) const noexcept
    {
        Matrix3x3 result;  // filled with 0s

        for (int row = 0; row < 3; row++)
        {
            for (int col = 0; col < 3; col++)
            {
                for (int i = 0; i < 3; i++)
                {
                    result[row][col] += data[row][i] * m[i][col];
                }
            }
        }

        return result;
    }

    Position operator*(const Position& p) const noexcept
    {
        Position result;

        result.X = data[0][0] * p.X + data[0][1] * p.Y + data[0][2] * p.Z;
        result.Y = data[1][0] * p.X + data[1][1] * p.Y + data[1][2] * p.Z;
        result.Z = data[2][0] * p.X + data[2][1] * p.Y + data[2][2] * p.Z;

        return result;
    }

    Vertex operator*(const Vertex& v) const noexcept
    {
        Vertex result = v;
        result.POS = *this * v.POS;
        return result;
    }
};
struct Triangle {
    Vertex A, B, C;
    Position POS;
    Rotation ROT;
    float SCALE;
    float rotationSpeed;
    Position velocity;
    float speed;

    std::vector<Vertex> GetLinearList() const
    {
        Matrix3x3 rotationMatrix =
            GetRotationMatrix(
                DegreesToRadians(ROT.X),
                DegreesToRadians(ROT.Y),
                DegreesToRadians(ROT.Z)
            );

        Vertex a = rotationMatrix * (A * SCALE);
        Vertex b = rotationMatrix * (B * SCALE);
        Vertex c = rotationMatrix * (C * SCALE);

        return { a + POS, b + POS, c + POS };
    }
};

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

bool CheckHR(HRESULT hr, const char* message)
{
    if (FAILED(hr))
    {
        std::ostringstream oss;
        oss << message << "\nHRESULT: 0x" << std::hex << hr;

        MessageBoxA(nullptr, oss.str().c_str(), "DirectX Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
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
    scd.BufferDesc.Width = SCREEN_WIDTH;
    scd.BufferDesc.Height = SCREEN_HEIGHT;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL, D3D11_SDK_VERSION, &scd, &swapchain, &dev, NULL, &devcon);
    if (!CheckHR(hr, "Failed to create D3D11 device and swapchain"))
        return false;

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
    depthDesc.Width = SCREEN_WIDTH;
    depthDesc.Height = SCREEN_HEIGHT;
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
    viewport.Width = SCREEN_WIDTH;
    viewport.Height = SCREEN_HEIGHT;
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
    if(graphicsUpdateRequired)
        UpdateGraphics();

    // do not draw on old frame:
    devcon->ClearRenderTargetView(backBuffer, BASE_COLOR);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Camera matrices
    XMVECTOR eye = XMLoadFloat3(&camera.position);  // camera position as eye
    XMVECTOR target = XMLoadFloat3(&camera.target);  // target position of the eye
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(  // View-Matrix
        eye,
        target,
        up
    );

    float aspect =
        static_cast<float>(SCREEN_WIDTH) /
        static_cast<float>(SCREEN_HEIGHT);

    projection = GetProjectionMatrix(camera, aspect);

    // Object transform
    world = XMMatrixRotationX(XMConvertToRadians(cubeRotation * 0.5f)) *
        XMMatrixRotationY(XMConvertToRadians(cubeRotation));
	// TODO: change to transform-component of object

    // Send W/V/P Matrices to HLSL
    D3D11_MAPPED_SUBRESOURCE mappedResource{};

    HRESULT hr = devcon->Map(
        matrixBuffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mappedResource
    );

    if (SUCCEEDED(hr))
    {
        MatrixBufferData* matrixData = static_cast<MatrixBufferData*>(mappedResource.pData);

        matrixData->world = XMMatrixTranspose(world);  // shader multiplies P * M

        matrixData->view = XMMatrixTranspose(view);

        matrixData->projection = XMMatrixTranspose(projection);

        devcon->Unmap(matrixBuffer, 0);
    }

    // write into constant buffer
    devcon->VSSetConstantBuffers(0,1, &matrixBuffer);

    UINT Stride = sizeof(Vertex);
    UINT offset = 0;

    devcon->IASetVertexBuffers(0, 1, &vertexBuffer, &Stride, &offset);
    devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    devcon->Draw(numberOfBufferVertices, 0);

    swapchain->Present(0, 0);  // just show it (back buffer is swapped to front)
}

void SetColorOfVertices(Triangle& tri) {
    tri.A.Color = D3DXCOLOR(RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), 1.0f);
    tri.B.Color = D3DXCOLOR(RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), 1.0f);
    tri.C.Color = D3DXCOLOR(RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), 1.0f);
}

static bool CanBeCulled2D(const Triangle& tri)
{
	return tri.POS.X > 1.0f || tri.POS.X < -1.0f || tri.POS.Y > 1.0f || tri.POS.Y < -1.0f;
}

void FixedUpdate(float fixedDeltaTime) {
    // Physics goes here
    // Example:
    // position += velocity * fixedDeltaTime;

    cubeRotation += fixedDeltaTime * 5.0f;
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

    D3D11_INPUT_ELEMENT_DESC ied[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            12,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
		}
    };

    hr = dev->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &layout);
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

float DegreesToRadians(float degrees)
{
    return degrees * PI / 180.0f;
}

Matrix3x3 GetRotationMatrix(const float eulerX, const float eulerY, const float eulerZ)
{
    float cx = std::cos(eulerX);
    float sx = std::sin(eulerX);

    float cy = std::cos(eulerY);
    float sy = std::sin(eulerY);

    float cz = std::cos(eulerZ);
    float sz = std::sin(eulerZ);

    Matrix3x3 xRot = {{{
        {{ 1.0f, 0.0f, 0.0f }},
        {{ 0.0f, cx,  -sx  }},
        {{ 0.0f, sx,   cx  }}
    }}};

    Matrix3x3 yRot = {{{
        {{  cy,  0.0f, sy   }},
        {{ 0.0f, 1.0f, 0.0f }},
        {{ -sy,  0.0f, cy   }}
    }}};

    Matrix3x3 zRot = {{{
        {{ cz,  -sz,  0.0f }},
        {{ sz,   cz,  0.0f }},
        {{ 0.0f, 0.0f, 1.0f }}
    }}};

    return xRot * yRot * zRot;
}

std::vector<Vertex> TrianglesToLinearVertices() {
    std::vector<Vertex> result;

    auto AddTriangleVertices = [&](const Triangle& tri)
    {
        std::vector<Vertex> triVertices = tri.GetLinearList();
        result.insert(result.end(), triVertices.begin(), triVertices.end());
    };

    for(int i = 0; i < triangles.size(); i++) {
        AddTriangleVertices(triangles[i]);
    }
    return result;
}

Triangle GetRandomTriangle(float xPosition, float yPosition, Position vel)
{
    bool xPosOK = xPosition >= -1.0f && xPosition <= 1.0f;
    bool yPosOK = yPosition >= -1.0f && yPosition <= 1.0f;
    float xPos = xPosOK ? xPosition : RandomFloat(-1.0f, 1.0f);
    float yPos = yPosOK ? yPosition : RandomFloat(-1.0f, 1.0f);

    float zRot = RandomFloat(0.0f, 90.0f);

    float scale = RandomFloat(0.05f, 0.15f);

    float rotationSpeed = RandomFloat(50.0f, 150.0f);

    float speed = RandomFloat(0.1f, 0.12f);

    Position velocity = vel.Length() > 0 ? vel : Position{ xPos, yPos, 0 };

    return {
        { 0.0f,  1.0f, 0.0f, D3DXCOLOR(1, 0, 0, 1) },
        { 1.0f, -1.0f, 0.0f, D3DXCOLOR(0, 1, 0, 1) },
        {-1.0f, -1.0f, 0.0f, D3DXCOLOR(0, 0, 1, 1) },
        {xPos, yPos, 0},
        {0, 0, zRot},
        scale,
        rotationSpeed,
        velocity.Normalize(),
        speed
    };
}

bool InitGraphics()
{
    testVertices =
    {
        // FRONT
        { {-1, -1, -1}, D3DXCOLOR(1, 0, 0, 1) },
        { {-1,  1, -1}, D3DXCOLOR(1, 0, 0, 1) },
        { { 1,  1, -1}, D3DXCOLOR(1, 0, 0, 1) },

        { {-1, -1, -1}, D3DXCOLOR(1, 0, 0, 1) },
        { { 1,  1, -1}, D3DXCOLOR(1, 0, 0, 1) },
        { { 1, -1, -1}, D3DXCOLOR(1, 0, 0, 1) },

        // BACK
        { {-1, -1,  1}, D3DXCOLOR(0, 1, 0, 1) },
        { { 1,  1,  1}, D3DXCOLOR(0, 1, 0, 1) },
        { {-1,  1,  1}, D3DXCOLOR(0, 1, 0, 1) },

        { {-1, -1,  1}, D3DXCOLOR(0, 1, 0, 1) },
        { { 1, -1,  1}, D3DXCOLOR(0, 1, 0, 1) },
        { { 1,  1,  1}, D3DXCOLOR(0, 1, 0, 1) },

        // LEFT
        { {-1, -1,  1}, D3DXCOLOR(0, 0, 1, 1) },
        { {-1,  1,  1}, D3DXCOLOR(0, 0, 1, 1) },
        { {-1,  1, -1}, D3DXCOLOR(0, 0, 1, 1) },

        { {-1, -1,  1}, D3DXCOLOR(0, 0, 1, 1) },
        { {-1,  1, -1}, D3DXCOLOR(0, 0, 1, 1) },
        { {-1, -1, -1}, D3DXCOLOR(0, 0, 1, 1) },

        // RIGHT
        { {1, -1, -1}, D3DXCOLOR(1, 1, 0, 1) },
        { {1,  1, -1}, D3DXCOLOR(1, 1, 0, 1) },
        { {1,  1,  1}, D3DXCOLOR(1, 1, 0, 1) },

        { {1, -1, -1}, D3DXCOLOR(1, 1, 0, 1) },
        { {1,  1,  1}, D3DXCOLOR(1, 1, 0, 1) },
        { {1, -1,  1}, D3DXCOLOR(1, 1, 0, 1) },

        // TOP
        { {-1, 1, -1}, D3DXCOLOR(1, 0, 1, 1) },
        { {-1, 1,  1}, D3DXCOLOR(1, 0, 1, 1) },
        { { 1, 1,  1}, D3DXCOLOR(1, 0, 1, 1) },

        { {-1, 1, -1}, D3DXCOLOR(1, 0, 1, 1) },
        { { 1, 1,  1}, D3DXCOLOR(1, 0, 1, 1) },
        { { 1, 1, -1}, D3DXCOLOR(1, 0, 1, 1) },

        // BOTTOM
        { {-1, -1,  1}, D3DXCOLOR(0, 1, 1, 1) },
        { {-1, -1, -1}, D3DXCOLOR(0, 1, 1, 1) },
        { { 1, -1, -1}, D3DXCOLOR(0, 1, 1, 1) },

        { {-1, -1,  1}, D3DXCOLOR(0, 1, 1, 1) },
        { { 1, -1, -1}, D3DXCOLOR(0, 1, 1, 1) },
        { { 1, -1,  1}, D3DXCOLOR(0, 1, 1, 1) }
    };

    return UpdateGraphics();
}

bool UpdateGraphics() {
    graphicsUpdateRequired = FALSE;

    //std::vector<Vertex> vertices = TrianglesToLinearVertices();
    const std::vector<Vertex>& vertices = testVertices;

    D3D11_BUFFER_DESC vertexBufferDesc; // buffer description
    ZeroMemory(&vertexBufferDesc, sizeof(D3D11_BUFFER_DESC));

    vertexBufferByteSize = sizeof(Vertex) * vertices.size();
    numberOfBufferVertices = vertices.size();

    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;  // could be anything
    vertexBufferDesc.ByteWidth = vertexBufferByteSize;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;  // this is a vertex buffer
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // full access to the buffer

    HRESULT hr = dev->CreateBuffer(&vertexBufferDesc, NULL, &vertexBuffer);
    if (!CheckHR(hr, "Failed to create vertex buffer"))
        return false;

    // copy vertices into buffer and give to pipeline
    D3D11_MAPPED_SUBRESOURCE ms;
    ZeroMemory(&ms, sizeof(D3D11_MAPPED_SUBRESOURCE));

    hr = devcon->Map(vertexBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    if (!CheckHR(hr, "Failed to map vertex buffer"))
        return false;

    memcpy(ms.pData, vertices.data(), sizeof(Vertex) * vertices.size());
    devcon->Unmap(vertexBuffer, NULL);  // close copy process

    return true;
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

    if (vertexBuffer)
    {
	    vertexBuffer->Release();
    	vertexBuffer = nullptr;
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
   hInst = hInstance; // Instanzenhandle in der globalen Variablen speichern

    // create the handler for the window
   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      300, 300, SCREEN_WIDTH, SCREEN_HEIGHT, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

    if (!InitD3D(hWnd))
    {
        MessageBoxA(hWnd, "DirectX initialization failed.", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

   ShowWindow(hWnd, nCmdShow);  // pop up the window
   UpdateWindow(hWnd);  // update the window

   return TRUE;
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
	    case WM_COMMAND:  // windows message command
	        {
	            int wmId = LOWORD(wParam);  // id of message
	            // Menüauswahl analysieren:
	            switch (wmId)
	            {
		            case IDM_ABOUT:
		                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
		                break;
		            case IDM_EXIT:
		                DestroyWindow(hWnd);
		                break;
		            default:
		                return DefWindowProc(hWnd, message, wParam, lParam);
	            }
	        }
	        break;
	    case WM_PAINT:
	        {
	            PAINTSTRUCT ps;
	            HDC hdc = BeginPaint(hWnd, &ps);
	            // TODO: Zeichencode, der hdc verwendet, hier einfügen...
	            EndPaint(hWnd, &ps);
	        }
	        break;
	    case WM_DESTROY:
	        PostQuitMessage(0);
	        break;
	    default:
	        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
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




/*

	#pragma once

	#include <vector>
	#include <string>
	#include <cstdint>
	#include <stdexcept>

	#include <d3d11.h>
	#include <wrl/client.h>
	#include <DirectXMath.h>
	
	#include "ufbx.h"

	using MeshHandle = uint32_t;

	inline constexpr MeshHandle INVALID_MESH = 0;

	struct Vertex
	{
	    DirectX::XMFLOAT3 position;
	    DirectX::XMFLOAT3 normal;
	    DirectX::XMFLOAT2 texCoord;
	};

	struct CpuMeshData
	{
	    std::vector<Vertex> vertices;
	    std::vector<uint32_t> indices;
	};

	struct Mesh
	{
	    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	    uint32_t vertexCount = 0;
	    uint32_t indexCount = 0;
	    uint32_t stride = sizeof(Vertex);
	};

	class MeshManager
	{
		private:
		    ID3D11Device* device = nullptr;

		    // index 0 is reserved for INVALID_MESH
		    std::vector<Mesh> meshes;

		public:
		    explicit MeshManager(ID3D11Device* device)
		        : device(device)
		    {
		        meshes.emplace_back(); // invalid mesh at index 0
		    }

		    MeshHandle LoadMesh(const std::string& path);

		    const Mesh& GetMesh(MeshHandle handle) const;

		private:
		    MeshHandle CreateMeshFromCpuData(const CpuMeshData& data);
	};



	------ .HPP ------

	MeshHandle MeshManager::CreateMeshFromCpuData(const CpuMeshData& data)
	{
	    if (!device)  // TODO: dev is my name I think
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

	CpuMeshData LoadMeshCpuWithUfbx(const std::string& path)
	{
	    ufbx_load_opts opts = {};
	    ufbx_error error = {};

	    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

	    if (!scene)
	        throw std::runtime_error(error.description.data);

	    CpuMeshData result;

	    for (size_t meshIndex = 0; meshIndex < scene->meshes.count; ++meshIndex)
	    {
	        ufbx_mesh* mesh = scene->meshes.data[meshIndex];

	        std::vector<uint32_t> triIndices(mesh->max_face_triangles * 3);

	        for (size_t faceIndex = 0; faceIndex < mesh->faces.count; ++faceIndex)
	        {
	            ufbx_face face = mesh->faces.data[faceIndex];

	            uint32_t numTriangles = ufbx_triangulate_face(
	                triIndices.data(),
	                triIndices.size(),
	                mesh,
	                face
	            );

	            for (uint32_t i = 0; i < numTriangles * 3; ++i)
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

	    ufbx_free_scene(scene);
	    return result;
	}

	const Mesh& MeshManager::GetMesh(MeshHandle handle) const
	{
	    if (handle == INVALID_MESH || handle >= meshes.size())
	        throw std::runtime_error("Invalid mesh handle.");

	    return meshes[handle];
	}


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