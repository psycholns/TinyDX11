#define WIDTH 1280
#define HEIGHT 800                  

// some tricks to avoid the otherwise generated stubs - d3d11.h/d3dcompiler.h can not compile with the modified WINAPI definition
#define D3D11CreateDeviceAndSwapChain xxx
#define D3DCompile yyy

#include <d3d11.h>
#include <d3dcompiler.h>

#undef D3D11CreateDeviceAndSwapChain
#undef D3DCompile
#undef WINAPI
#define WINAPI __declspec(dllimport)  __stdcall 

extern "C" 
{
	// now with non-stub WINAPI
	HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
		__in_opt IDXGIAdapter* pAdapter,
		D3D_DRIVER_TYPE DriverType,
		HMODULE Software,
		UINT Flags,
		__in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
		UINT FeatureLevels,
		UINT SDKVersion,
		__in_opt CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
		__out_opt IDXGISwapChain** ppSwapChain,
		__out_opt ID3D11Device** ppDevice,
		__out_opt D3D_FEATURE_LEVEL* pFeatureLevel,
		__out_opt ID3D11DeviceContext** ppImmediateContext);

	HRESULT WINAPI
		D3DCompile(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
			_In_ SIZE_T SrcDataSize,
			_In_opt_ LPCSTR pSourceName,
			_In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
			_In_opt_ ID3DInclude* pInclude,
			_In_opt_ LPCSTR pEntrypoint,
			_In_ LPCSTR pTarget,
			_In_ UINT Flags1,
			_In_ UINT Flags2,
			_Out_ ID3DBlob** ppCode,
			_Always_(_Outptr_opt_result_maybenull_) ID3DBlob** ppErrorMsgs);
};


// a fullscreen window with stretched content is more compatible than oldskool fullscreen mode these days
static int SwapChainDesc[] = {
	WIDTH, HEIGHT, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0,  // w,h,refreshrate,format, scanline, (fullscreen)scaling
	1, 0, // sampledesc (count, quality)
	DXGI_USAGE_UNORDERED_ACCESS, // usage
	1, // buffercount
	0, // hwnd
	1, 0, 0 }; // windowed, swap_discard, flags

static D3D11_BUFFER_DESC ConstBufferDesc = { 256, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 }; // 256,0,4,0,0,0

// just a hlsl adaption of "No Chrome Spheres Permitted" to have something
static char shader[] =
"int t;"  // implicit constant buffer
"RWTexture2D<float3> y;"
"[numthreads(8,8,1)]"
"void cs_5_0(uint3 i:sv_dispatchthreadid)"
"{"
"float3 v=i/float3(640,400,1)-1,"
"w=normalize(float3(v.x,-v.y*.8-1,2)),"
"p=float3(sin(t*.0044),sin(t*.0024)+2,sin(t*.0034)-5);"
"float b=dot(w,p),d=b*b-dot(p,p)+1,x=0;"
"if(d>0){"
"p-=w*(b+sqrt(d));"
"x=pow(d,8);"
"w=reflect(w,p);"
"}"
"if(w.y<0){"
"p-=w*(p.y+1)/w.y;"
"x+=(sin(p.z*6)*sin(p.x*6)>0)*2/length(p);"
"}"
"y[i.xy]=abs(v.y-v.x)>.1&&abs(v.y+v.x)>.1?x:float3(1,0,0);"
"}";


#ifndef _DEBUG
void WinMainCRTStartup()
{
	//note: to remove cursor under newer windows you need both the decoration bits and showcursor
	ShowCursor(0);
	HWND  hWnd = CreateWindowExA(0,(LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0); //"edit"
#else
#include <stdio.h>
int WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)
{
	HWND hWnd = CreateWindowA("edit", 0, WS_POPUP | WS_VISIBLE, 0, 0, WIDTH, HEIGHT, 0, 0, 0, 0);
#endif
	SwapChainDesc[11] = (int)hWnd;

	// setting up device
	ID3D11Device*               pd3dDevice;
	ID3D11DeviceContext*        pImmediateContext;
	IDXGISwapChain*             pSwapChain;
#ifdef _DEBUG	
	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, 0, 0, D3D11_SDK_VERSION, (DXGI_SWAP_CHAIN_DESC*)&SwapChainDesc[0], &pSwapChain, &pd3dDevice, NULL, &pImmediateContext);
	if (FAILED(hr)) return hr;
#else
	D3D11CreateDeviceAndSwapChain( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, 0,0, D3D11_SDK_VERSION, (DXGI_SWAP_CHAIN_DESC*)&SwapChainDesc[0], &pSwapChain, &pd3dDevice, NULL, &pImmediateContext );
#endif

	// unordered access view on back buffer
	pSwapChain->GetBuffer(0, __uuidof( ID3D11Texture2D ), (LPVOID*)&SwapChainDesc[0]);	// reuse swapchain struct for temp storage of texture pointer
	pd3dDevice->CreateUnorderedAccessView((ID3D11Texture2D*)SwapChainDesc[0], NULL, (ID3D11UnorderedAccessView**)&SwapChainDesc[0]);
	pImmediateContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&SwapChainDesc[0], 0);

	// constant buffer
	ID3D11Buffer *pConstants;
	pd3dDevice->CreateBuffer(&ConstBufferDesc, NULL, &pConstants);
	pImmediateContext->CSSetConstantBuffers(0, 1, &pConstants);

	// compute shader
	ID3D11ComputeShader *pCS;
#ifdef _DEBUG
	ID3D10Blob* pErrorBlob = NULL;
	ID3D10Blob* pBlob = NULL;
	hr = D3DCompile(shader, sizeof(shader), 0, 0, 0, "cs_5_0", "cs_5_0", D3D10_SHADER_DEBUG, 0, &pBlob, &pErrorBlob);

	if (FAILED(hr))
	{
		char message[4096];
		sprintf(message, "unknown compiler error");
		if (pErrorBlob != NULL)
			sprintf(message, "%s", pErrorBlob->GetBufferPointer());
		MessageBox(hWnd, message, "Error", MB_OK);
		return 10;
	}
	else
		hr = pd3dDevice->CreateComputeShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pCS);
	void *pp = pBlob->GetBufferPointer(); int ps = pBlob->GetBufferSize();
	//int* bla = (int*)pBlob; void* pp2 = (void*)bla[3]; int ps2 = bla[2]; // yep, they are the same, check crinkler version below
	if (pErrorBlob != 0) pErrorBlob->Release();
	if (pBlob != 0) pBlob->Release();
#else
	ID3D10Blob* pBlob;
	D3DCompile(shader, 1024, 0, 0, 0, "cs_5_0", "cs_5_0", 0, 0, &pBlob, 0); // We can use some abitary large value for size, null terminated anyway...

	pd3dDevice->CreateComputeShader((void*)(((int*)pBlob)[3]), ((int*)pBlob)[2], NULL, &pCS);
#endif
	pImmediateContext->CSSetShader(pCS, NULL, 0);

	do
	{
		PeekMessageA(0, 0, 0, 0, PM_REMOVE); // unanswered input may kill you - at least in windowed mode
		
		SwapChainDesc[0] = GetTickCount();   // please use something better (from the music), this is too coarse..
		pImmediateContext->UpdateSubresource(pConstants, 0, 0, &SwapChainDesc[0], 0, 0);
		pImmediateContext->Dispatch(WIDTH / 8, HEIGHT / 8, 1);
		pSwapChain->Present(0, 0);
	} 
	while (!GetAsyncKeyState(VK_ESCAPE));

	ExitProcess(0);  // actually not needed in this setup, but may be smaller..
}
