#include <windows.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <deque>
#include <time.h>

//Declarations
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct Particle
{
	float x,y, vx, vy; //Координаты и скорости
};

struct D3DParticle
{
	D3DXVECTOR3 pos;
	D3DXVECTOR3 velocity;
};
//////////////////////////////////////////////////////////////////////////

//Globals
HWND hMainWnd = NULL;

ID3D10Device *device = nullptr;
IDXGISwapChain *swapChain = nullptr;

ID3D10RenderTargetView *renderTargetView = nullptr;

ID3D10Effect *effect = nullptr;
ID3D10EffectTechnique *renderTech = nullptr;
ID3D10EffectTechnique *streamOutTech = nullptr;

ID3D10Buffer *vertexBuffer = nullptr;
ID3D10Buffer *streamOut = nullptr;
ID3D10InputLayout *vertexLayout = nullptr;

ID3D10ShaderResourceView *particleTexture = nullptr;

ID3D10RasterizerState *rasterizeState = nullptr;

//Constant

const float defG = 16.6738480f; // Гравитационная постоянная умноженная на массу "курсора"
const float defResistance = 0.9975f; // Сопротивление среде
const float defSize = 3;

//Variables
int Height = 0;
int Width = 0;

float G = defG;
float Resistance = defResistance;
float Size = defSize;
int updateDelay = 8;


#ifdef _DEBUG
int particlesCount = 5000;
#else
int particlesCount = 1500000;
#endif

std::deque<Particle> particles;
//////////////////////////////////////////////////////////////////////////
/*
void animate()
{
POINT pos;
GetCursorPos(&pos);
RECT rc;
GetClientRect(hMainWnd, &rc); 
ScreenToClient(hMainWnd, &pos);

const int mx = pos.x;
const int my = pos.y;
const auto size = particles.size();

float force;
float distSquare;

D3DParticle *pVertexBuffer;
vertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, ((void **)&pVertexBuffer));

int i;
#pragma omp parallel \
shared(pVertexBuffer, particles, mx, my, size) \
private(i, force, distSquare)
{
#pragma omp for
for( i = 0; i < size; ++i )
{
auto &x = particles[i].x;
auto &y = particles[i].y;

distSquare = pow( x - mx, 2 ) + pow( y - my, 2 );
if( distSquare < 400 ) // A magic number represent min process distance
{
force = 0;
}
else
{
force = G / distSquare;
}

const float xForce = (mx - x) * force;
const float yForce = (my - y) * force;

particles[i].vx *= Resistance;
particles[i].vy *= Resistance;

particles[i].vx += xForce;
particles[i].vy += yForce;

x+= particles[i].vx;
y+= particles[i].vy;

if( x > Width )
x -= Width;
else if( x < 0 )
x += Width;

if( y > Height )
y -= Height;
else if( y < 0 )
y += Height;

pVertexBuffer[i].pos.x = particles[i].x;
pVertexBuffer[i].pos.y = particles[i].y;
pVertexBuffer[i].velocity.x = fabs(particles[i].vx)/7;
pVertexBuffer[i].velocity.y = fabs(particles[i].vy)/7;
}
}
vertexBuffer->Unmap();
}
*/
//////////////////////////////////////////////////////////////////////////
bool initD3D()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof(sd) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = Width;
	sd.BufferDesc.Height = Height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hMainWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	if( FAILED( D3D10CreateDeviceAndSwapChain( NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL,
		0, D3D10_SDK_VERSION, &sd, &swapChain, &device ) ) )
	{
		return FALSE;
	}

	ID3D10Texture2D *pBackBuffer;
	if( FAILED( swapChain->GetBuffer( 0, __uuidof( ID3D10Texture2D ), (LPVOID*)&pBackBuffer ) ) )
		return FALSE;

	auto hr = device->CreateRenderTargetView( pBackBuffer, NULL, &renderTargetView );
	pBackBuffer->Release();
	if( FAILED( hr ) )
		return FALSE;

	device->OMSetRenderTargets( 1, &renderTargetView, NULL );

	D3D10_VIEWPORT vp;
	vp.Width = Width;
	vp.Height = Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	device->RSSetViewports( 1, &vp );

	//Setup render states
	D3D10_RASTERIZER_DESC rsDesc;
	ZeroMemory(&rsDesc, sizeof(D3D10_RASTERIZER_DESC));
	rsDesc.AntialiasedLineEnable = TRUE;
	rsDesc.MultisampleEnable = TRUE;
	rsDesc.FillMode = D3D10_FILL_SOLID;
	rsDesc.CullMode = D3D10_CULL_NONE;
	rsDesc.FrontCounterClockwise = false;

	device->CreateRasterizerState(&rsDesc, &rasterizeState);	

	device->RSSetState(rasterizeState);

	//Setup alpha blending
	D3D10_BLEND_DESC blendDesc = {0};
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.BlendEnable[0] = true;
	blendDesc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
	blendDesc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
	blendDesc.BlendOp = D3D10_BLEND_OP_ADD;
	blendDesc.SrcBlendAlpha = D3D10_BLEND_ONE;
	blendDesc.DestBlendAlpha = D3D10_BLEND_ZERO;
	blendDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	blendDesc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

	ID3D10BlendState* mTransparentBS;
	device->CreateBlendState(&blendDesc, &mTransparentBS);
	device->OMSetBlendState(mTransparentBS, NULL, 0xffffffff);


	return TRUE;
}
bool initEffect()
{
	DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif
	ID3D10Blob *errorBuffer;
	auto hr = D3DX10CreateEffectFromFile( L"effect.fx", NULL, NULL, "fx_4_0", dwShaderFlags, 0,
		device, NULL, NULL, &effect, &errorBuffer, NULL );
	if( FAILED( hr ) )
	{
		MessageBoxA(hMainWnd, (char *)errorBuffer->GetBufferPointer(), nullptr, MB_ICONHAND );
		return FALSE;
	}

	// Obtain the technique
	renderTech = effect->GetTechniqueByName("Render");
	streamOutTech = effect->GetTechniqueByName("StreamOut");


	// Setup matrices
	ID3D10EffectMatrixVariable* g_pViewVariable = NULL;
	ID3D10EffectMatrixVariable* g_pProjectionVariable = NULL;

	D3DXMATRIX matrixView;
	D3DXMATRIX matrixProjection;

	D3DXMatrixLookAtLH(
		&matrixView,
		&D3DXVECTOR3(0,0,0),
		&D3DXVECTOR3(0,0,1),
		&D3DXVECTOR3(0,1,0));

	D3DXMatrixOrthoOffCenterLH(&matrixProjection, 0, Width, Height, 0, 0, 255);

	g_pViewVariable = effect->GetVariableByName( "View" )->AsMatrix();
	g_pProjectionVariable = effect->GetVariableByName( "Projection" )->AsMatrix();

	g_pViewVariable->SetMatrix( (float*)&matrixView );
	g_pProjectionVariable->SetMatrix( (float*)&matrixProjection );

	// Setup variables
	effect->GetVariableByName( "Size" )->AsScalar()->SetFloat(Size);
	effect->GetVariableByName( "G" )->AsScalar()->SetFloat(G);
	effect->GetVariableByName( "Resistance" )->AsScalar()->SetFloat(Resistance);
	effect->GetVariableByName( "Width" )->AsScalar()->SetInt(Width);
	effect->GetVariableByName( "Height" )->AsScalar()->SetInt(Height);

	// Setup Textures

	D3DX10CreateShaderResourceViewFromFile(device, L"particle.png", NULL, NULL, &particleTexture, NULL);
	effect->GetVariableByName( "Base" )->AsShaderResource()->SetResource(particleTexture);

	return true;
}

void initParticles()
{
	srand(clock());

	Particle tmp;
	for( int i = 0; i<particlesCount; ++i )
	{
		tmp.x  = rand()%Width;
		tmp.y  = rand()%Height;
		tmp.vx = rand()%2;
		tmp.vy = rand()%2;

		particles.push_back( tmp );
	}
}
bool initVertexes()
{
	D3D10_INPUT_ELEMENT_DESC layout[] =
	{
		{ "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = sizeof( layout ) / sizeof( layout[0] );

	// Создаем input layout
	D3D10_PASS_DESC PassDesc;
	renderTech->GetPassByIndex( 0 )->GetDesc( &PassDesc );
	auto hr = device->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
		PassDesc.IAInputSignatureSize, &vertexLayout );
	if( FAILED( hr ) )
		return FALSE;

	size_t count = particles.size();
	D3DParticle *vertexData = new D3DParticle[count];

	for(size_t i=0; i<count; ++i)
	{
		vertexData[i].pos.x = particles[i].x;
		vertexData[i].pos.y = particles[i].y;
		vertexData[i].pos.z = 0.5f; // We don't use z coordinate

		vertexData[i].velocity.x = particles[i].vx;
		vertexData[i].velocity.y = particles[i].vy;
		vertexData[i].velocity.z = 1; // We don't use z velocity
	}

	D3D10_BUFFER_DESC bd;
	bd.Usage = D3D10_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( D3DParticle ) * count;
	bd.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
	bd.CPUAccessFlags = NULL;
	bd.MiscFlags = 0;
	D3D10_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = vertexData;
	if( FAILED( device->CreateBuffer( &bd, &InitData, &vertexBuffer ) ) )
		return FALSE;	


	// Init Stream out buffer

	device->CreateBuffer(&bd, &InitData, &streamOut);

	delete[] vertexData;

	return true;
}

void init()
{
	if(!initD3D())
	{
		MessageBoxA(hMainWnd, "Unable to initialize graphics system.", nullptr, MB_ICONHAND);
		terminate();
	}
	if(!initEffect())
	{
		MessageBoxA(hMainWnd, "Unable to initialize effects.", nullptr, MB_ICONHAND);
		terminate();
	}
	initParticles();
	if(!initVertexes())
	{
		MessageBoxA(hMainWnd, "Unable to initialize vertex data.", nullptr, MB_ICONHAND);
		terminate();
	}
}

void cleanUp()
{
	if(vertexLayout) vertexLayout->Release();
	if(vertexBuffer) vertexBuffer->Release();
	if(renderTargetView) renderTargetView->Release();
	if(particleTexture) particleTexture->Release();
	if(rasterizeState) rasterizeState->Release();
	if(effect) effect->Release();
	if(swapChain) swapChain->Release();
	if(device) device->Release();
}

void DrawParticles() 
{
	// устанавливаем vertex buffer
	UINT stride = sizeof( D3DParticle );
	UINT offset = 0;
	device->IASetVertexBuffers( 0, 1, &vertexBuffer, &stride, &offset );
	device->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

	// Устанавливаем input layout
	device->IASetInputLayout( vertexLayout );

	device->Draw(particles.size(), 0);
}

void UpdateParticles()
{
	// Getting data

	POINT pos;
	GetCursorPos(&pos);
	RECT rc;
	GetClientRect(hMainWnd, &rc); 
	ScreenToClient(hMainWnd, &pos);

	const int mx = pos.x;
	const int my = pos.y;

	// Setup variables
	effect->GetVariableByName( "X" )->AsScalar()->SetInt(mx);
	effect->GetVariableByName( "Y" )->AsScalar()->SetInt(my);

	// Stream outing
	UINT offset[1] = {0};
	device->SOSetTargets(1, &streamOut, offset);

	D3D10_TECHNIQUE_DESC techDesc;
	streamOutTech->GetDesc( &techDesc );
	for( UINT p = 0; p < techDesc.Passes; ++p )
	{
		streamOutTech->GetPassByIndex( p )->Apply( 0 );
		DrawParticles();
	}

	ID3D10Buffer* bufferArray[1] = {0};
	device->SOSetTargets(1, bufferArray, offset);
	std::swap(streamOut, vertexBuffer);
}

void Render() 
{
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA
	device->ClearRenderTargetView(renderTargetView, ClearColor);

	D3D10_TECHNIQUE_DESC techDesc;
	renderTech->GetDesc( &techDesc );
	for( UINT p = 0; p < techDesc.Passes; ++p )
	{
		renderTech->GetPassByIndex( p )->Apply( 0 );
		DrawParticles();
	}

	swapChain->Present(NULL, NULL);
}

void initVariables() 
{
	int args = 0;
	auto data = CommandLineToArgvW(GetCommandLineW(), &args);

	for(int i=1; i<args; ++i)
	{
		if(data[i][0] == '-') //We recive a parameter
		{
			if(i < args-1) //We have a data after parameter
			{
				if(data[i+1][0] != '-') //We have a value
				{
					if(wcscmp(data[i]+1, L"pointSize") == 0)
					{
						Size = _wtof(data[i+1]);
					}
					else if(wcscmp(data[i]+1, L"updateDelay") == 0)
					{
						updateDelay = _wtoi(data[i+1]);
					}
					else if(wcscmp(data[i]+1, L"particles") == 0)
					{
						particlesCount = _wtoi(data[i+1]);
					}
				}
			}
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
	initVariables();

	WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_VREDRAW|CS_HREDRAW|CS_OWNDC, 
		WndProc, 0, 0, hInstance, NULL, NULL, (HBRUSH)(COLOR_WINDOW+1), 
		NULL, L"ParticleSystemClass", NULL}; 

	RegisterClassEx(&wc);

	Width = GetSystemMetrics(SM_CXSCREEN);
	Height = GetSystemMetrics(SM_CYSCREEN);

	hMainWnd = CreateWindow(L"ParticleSystemClass", 
		L"Particles", 
		WS_POPUP, 0, 0, Width, Height, 

		NULL, NULL, hInstance, NULL);

	SetTimer(hMainWnd, 0, updateDelay, 0);

	init();

	ShowWindow(hMainWnd, nShow);
	UpdateWindow(hMainWnd);


	MSG msg = {0};
	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	cleanUp();

	return(0);
} 

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch(msg)
	{
	case WM_SETCURSOR:
		SetCursor( LoadCursor(NULL, IDC_CROSS) );
		return TRUE;

	case WM_TIMER:
		UpdateParticles();
		Render();
		return TRUE;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		G=-G;
		effect->GetVariableByName( "G" )->AsScalar()->SetFloat(G);
		return 0;

	case WM_MOUSEWHEEL:
		{
			const auto keyState = GET_KEYSTATE_WPARAM(wParam);
			int wheelDelta;

			if(keyState & MK_SHIFT)
			{
				wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam)/6;
			}
			else
			{
				wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam)/120;
			}

			G+= wheelDelta*0.05f;
			effect->GetVariableByName( "G" )->AsScalar()->SetFloat(G);
			return 0;
		}

	case WM_RBUTTONUP:
		{
			G=0;
			effect->GetVariableByName( "G" )->AsScalar()->SetFloat(G);
			return 0;
		}

		return 0;

	case WM_KEYUP:
		{
			switch (wParam)
			{
			case VK_ESCAPE:
				ShowWindow(hMainWnd, SW_HIDE);
				PostQuitMessage(0);
				break;
			case VK_SPACE:
				Resistance = 1;
				effect->GetVariableByName( "Resistance" )->AsScalar()->SetFloat(Resistance);
				break;
			case VK_TAB:
				Resistance = defResistance;
				effect->GetVariableByName( "Resistance" )->AsScalar()->SetFloat(Resistance);
				break;
			case 'Q':
				Resistance+= 0.001f;

				if(Resistance > 1.f)
					Resistance = 1.f;
				effect->GetVariableByName( "Resistance" )->AsScalar()->SetFloat(Resistance);
				break;
			case 'W':
				Resistance-= 0.001f;

				if(Resistance < 0)
					Resistance = 0;
				effect->GetVariableByName( "Resistance" )->AsScalar()->SetFloat(Resistance);
				break;
			case 'S':
				Resistance = 0;
				G = 0;
				effect->GetVariableByName( "Resistance" )->AsScalar()->SetFloat(Resistance);
				effect->GetVariableByName( "G" )->AsScalar()->SetFloat(G);
				break;
			case 'G':
				G = defG;
				effect->GetVariableByName( "G" )->AsScalar()->SetFloat(G);
				break;
			}
			return 0;
		}

	default:
		return(DefWindowProc(hwnd, msg, wParam, lParam));
	}

	return 0;
}