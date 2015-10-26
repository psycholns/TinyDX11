#define WIDTH 1280
#define HEIGHT 800                   


// some tricks avoid the otherwise generated stubs - d3d11.h can not compile with the modified WINAPI definition
#define D3D11CreateDeviceAndSwapChain xxx
#include <d3d11.h>
#undef D3D11CreateDeviceAndSwapChain
#undef WINAPI
#define WINAPI __declspec(dllimport)  __stdcall 

#include <d3dx11.h>
#include <stdio.h>

extern "C" {
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

	// only need the direct pointer for this, for the uuid offset fetch
	HRESULT __stdcall  _imp__D3DX11CompileFromMemory(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude,
		LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);
};


// just a hlsl adaption of "No Chrome Spheres Permitted" - could be optimized further, but that's not the point
static char shader[] =
"int t;"  // implicit constant buffer
"RWTexture2D<float4> y;"
"[numthreads(16,16,1)]"
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
"if(sin(p.z*6)*sin(p.x*6)>0)x+=2/length(p);"
"}"
"y[i.xy]=(abs(v.y-v.x)>.1&&abs(v.y+v.x)>.1)?x:float4(1,0,0,0);"
"}";

static int SwapChainDesc[] = {
	WIDTH, HEIGHT, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0,  // w,h,refreshrate,format, scanline, scaling
	1, 0, // sampledesc (count, quality)
	DXGI_USAGE_UNORDERED_ACCESS, // usage
	1, // buffercount
	0, // hwnd
	0, 0, 0 }; // fullscreen, swap_discard, flags

static D3D11_BUFFER_DESC ConstBufferDesc = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 }; // 16,0,4,0,0,0

#ifndef _DEBUG
void WinMainCRTStartup()
{
	//note: to remove cursor under newer windows you need both the decoration bits and showcursor, like this:
	//HWND  hWnd = CreateWindowExA(0,(LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0); //"edit"
	//ShowCursor(0);

	HWND  hWnd = CreateWindowExA(0, (LPCSTR)0xC019, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); //"static"
#else
int WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)
{
	HWND hWnd = CreateWindowA("edit", 0, WS_POPUP | WS_VISIBLE, 0, 0, WIDTH, HEIGHT, 0, 0, 0, 0);
	SwapChainDesc[12] = 1; // windowed
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
	D3D11CreateDeviceAndSwapChain( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, 0,0,D3D11_SDK_VERSION, (DXGI_SWAP_CHAIN_DESC*)&SwapChainDesc[0], &pSwapChain, &pd3dDevice, NULL, &pImmediateContext );
#endif


	// unordered access view on back buffer
	IID* iid = (IID*)(*(char**)_imp__D3DX11CompileFromMemory - 36181);   // superdirty way to get that otherwise big guid - d3dx11_43 will remain constant, so it's ok...
	pSwapChain->GetBuffer(0, /*__uuidof( ID3D11Texture2D )*/*iid, (LPVOID*)&SwapChainDesc[0]);	// reuse swapchain struct for temp storage of texture pointer
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
	hr = _imp__D3DX11CompileFromMemory(shader, sizeof(shader), 0, 0, 0, "cs_5_0", "cs_5_0", D3D10_SHADER_DEBUG, 0, 0, &pBlob, &pErrorBlob, 0);
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
	//_imp__D3DX11CompileFromMemory(shader, 1024, 0, 0, 0, "cs_5_0", "cs_5_0", 0, 0, 0, &pBlob, 0, 0);
	D3DX11CompileFromMemory(shader, 1024, 0, 0, 0, "cs_5_0", "cs_5_0", 0, 0, 0, &pBlob, 0, 0); 
	pd3dDevice->CreateComputeShader((void*)(((int*)pBlob)[3]), ((int*)pBlob)[2], NULL, &pCS);
#endif
	pImmediateContext->CSSetShader(pCS, NULL, 0);


	do
	{
		SwapChainDesc[0] = GetTickCount();
		pImmediateContext->UpdateSubresource(pConstants, 0, 0, &SwapChainDesc[0], 4, 4);
		pImmediateContext->Dispatch(WIDTH / 16, HEIGHT / 16, 1);
		pSwapChain->Present(0, 0);
	} while (!GetAsyncKeyState(VK_ESCAPE));

	ExitProcess(0);
}