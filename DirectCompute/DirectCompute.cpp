// DirectCompute.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include "pch.h"
#include <iostream>
#include <random>
#include <cmath>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iomanip>
#include "app/renderdoc_app.h"
#include <stdint.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

struct OutputType {
	float directExp;
	float indirectExp;
};

union FloatCmp {
	FloatCmp(float num = 0.0f) : f(num) {}
	
	int32_t i;
	float f;
};

//Forward declarations
HRESULT CreateBuffer(ID3D11Device* device, UINT element_size, UINT element_count, void* init_data, ID3D11Buffer** buffer);
HRESULT CreateShaderResourceView(ID3D11Device* device, ID3D11Buffer* buffer, ID3D11ShaderResourceView** srv);
HRESULT CreateUnorderedAccessView(ID3D11Device* device, ID3D11Buffer* buffer, ID3D11UnorderedAccessView** uav);
HRESULT CreateStagingBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Buffer* bufferFrom, ID3D11Buffer** bufferTo);
int ULPDiff(float a, float b);

int main(int argc, char* argv[])
{
	ID3D11Device*               device = nullptr;
	ID3D11DeviceContext*        deviceContext = nullptr;
	ID3D11ComputeShader*        shader = nullptr;

	ID3D11Buffer*               inBuffer = nullptr;
	ID3D11Buffer*               outBuffer = nullptr;
	ID3D11Buffer*				readBuffer = nullptr;

	ID3D11ShaderResourceView*   inSRV = nullptr;
	ID3D11UnorderedAccessView*  outUAV = nullptr;

	static const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	HRESULT hr;
	float min, max;

	if (argc < 3) {
		min = 0.0;
		max = 1.0;
	}
	else {
		min = std::atof(argv[1]);
		max = std::atof(argv[2]);
	}
	std::cout << "min and max are set to: " << min << " " << max << "\n";

	std::cout << "Trying to create device...\n";
	
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 1, D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
	if (FAILED(hr)) {
		std::cout << "We were unable to create a hardware device! Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}

    std::cout << "Creating compute shader...\n";

	static const D3D_SHADER_MACRO defines[] = { "USE_STRUCTURED_BUFFERS" };
	DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	//DWORD shaderFlags = 0;
	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* codeBlob = nullptr;
	
	hr = D3DCompileFromFile(L"ExpCompute.hlsl", defines, nullptr, "main", "cs_5_0", shaderFlags, 0, &codeBlob, &errorBlob);
	if (FAILED(hr)) {
		std::cout << "A shader error has occurred! Error code: " << std::hex << hr << "\n";
		if (errorBlob) {
			std::cout << "A shader compile error has occurred!\n\t" << errorBlob->GetBufferPointer() << "\n";
		}
		return EXIT_FAILURE;
	}

	HMODULE rdoc_handle = GetModuleHandleA("renderdoc.dll");
	RENDERDOC_API_1_4_0* rdoc_api = nullptr;

	if (rdoc_handle) {
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(rdoc_handle, "RENDERDOC_GetAPI");

		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_0, (void**)&rdoc_api);

		if (ret != 1) rdoc_api = nullptr;
	}

	if (rdoc_api) rdoc_api->StartFrameCapture(nullptr, nullptr);

	hr = device->CreateComputeShader(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), nullptr, &shader);
	if (FAILED(hr)) {
		std::cout << "An error has occurred while creating the shader! Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}

	std::cout << "Creating buffers and filling them with input and expected output data...\n";

	float data[100], exp_data[100];

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(min, max);
	for (int i = 0; i < 100; i++) {
		data[i] = dis(gen);
		exp_data[i] = std::exp(data[i]);
	}

	std::cout << "Creating buffers for the input and output values...\n";

	hr = CreateBuffer(device, sizeof(float), 100, data, &inBuffer);
	if (FAILED(hr)) {
		std::cout << "Failed to create input buffer. Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}
	hr = CreateBuffer(device, sizeof(OutputType), 100, nullptr, &outBuffer);
	if (FAILED(hr)) {
		std::cout << "Failed to create output buffer. Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}
	
	std::cout << "Creating buffer views...\n";
	hr = CreateShaderResourceView(device, inBuffer, &inSRV);
	if (FAILED(hr)) {
		std::cout << "Failed to create shader resource view. Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}
	hr = CreateUnorderedAccessView(device, outBuffer, &outUAV);
	if (FAILED(hr)) {
		std::cout << "Failed to create unordered resource view. Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}
	
	std::cout << "Run Compute Shader...\n";


	deviceContext->CSSetShader(shader, nullptr, 0);
	deviceContext->CSSetShaderResources(0, 1, &inSRV);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &outUAV, nullptr);
	deviceContext->Dispatch(100, 1, 1);

	if (rdoc_api) rdoc_api->EndFrameCapture(nullptr, nullptr);

	std::cout << "Reading back results...\n";

	hr = CreateStagingBuffer(device, deviceContext, outBuffer, &readBuffer);
	if (FAILED(hr)) {
		std::cout << "Failed to create staging buffer. Error code was: " << std::hex << hr  << "\n";
		return EXIT_FAILURE;
	}
	
	D3D11_MAPPED_SUBRESOURCE mapped_resource;
	hr = deviceContext->Map(readBuffer, 0, D3D11_MAP_READ, 0, &mapped_resource);
	if (FAILED(hr)) {
		std::cout << "Couldn't map resource to output buffer. Error: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}
	
	OutputType* result = static_cast<OutputType*>(mapped_resource.pData);
	if (!result) {
		std::cout << "Couldn't cast mapped resource data to output type\n";
		return EXIT_FAILURE;
	}
	
	int sumUlpDirect = 0;
	int sumUlpIndirect = 0;

	for (int i = 0; i < 100; i++) {
		int ulpDirect = ULPDiff(exp_data[i], result[i].directExp);
		int ulpIndirect = ULPDiff(exp_data[i], result[i].indirectExp);
		sumUlpDirect += ulpDirect;
		sumUlpIndirect += ulpIndirect;
	}
	
	std::cout << "The sum of ULPs direct is: " << sumUlpDirect << "\n";
	std::cout << "The sum of ULPs indirect is: " << sumUlpIndirect << "\n";

	return EXIT_SUCCESS;
}

HRESULT CreateBuffer(ID3D11Device* device, UINT element_size, UINT element_count, void* init_data, ID3D11Buffer** buffer) {
	
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.ByteWidth = element_size * element_count;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = element_size;

	if (init_data) 
	{
		D3D11_SUBRESOURCE_DATA subresource_data{init_data};
		
		return device->CreateBuffer(&desc, &subresource_data, buffer);
	}
	else {
		return device->CreateBuffer(&desc, nullptr, buffer);
	}
}

HRESULT CreateStagingBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Buffer* bufferFrom, ID3D11Buffer** bufferTo) {

	HRESULT hr;
	ID3D11Buffer* debugbuf = nullptr;

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	bufferFrom->GetDesc(&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;

	hr = device->CreateBuffer(&desc, nullptr, bufferTo);
	if (FAILED(hr)) return hr;

	context->CopyResource(*bufferTo, bufferFrom);

	return S_OK;
}

HRESULT CreateShaderResourceView(ID3D11Device* device, ID3D11Buffer* buffer, ID3D11ShaderResourceView** srv) {
	D3D11_BUFFER_DESC desc_buf;
	ZeroMemory(&desc_buf, sizeof(desc_buf));
	buffer->GetDesc(&desc_buf);

	D3D11_SHADER_RESOURCE_VIEW_DESC desc_srv;
	ZeroMemory(&desc_srv, sizeof(desc_srv));
	desc_srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc_srv.Buffer.FirstElement = 0;
	desc_srv.Format = DXGI_FORMAT_UNKNOWN;
	desc_srv.Buffer.NumElements = desc_buf.ByteWidth / desc_buf.StructureByteStride;

	return device->CreateShaderResourceView(buffer, &desc_srv, srv);
}

HRESULT CreateUnorderedAccessView(ID3D11Device* device, ID3D11Buffer* buffer, ID3D11UnorderedAccessView** uav) {
	D3D11_BUFFER_DESC desc_buf;
	ZeroMemory(&desc_buf, sizeof(desc_buf));
	buffer->GetDesc(&desc_buf);

	D3D11_UNORDERED_ACCESS_VIEW_DESC desc_uav;
	ZeroMemory(&desc_uav, sizeof(desc_uav));
	desc_uav.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	desc_uav.Buffer.FirstElement = 0;
	desc_uav.Format = DXGI_FORMAT_UNKNOWN;
	desc_uav.Buffer.NumElements = desc_buf.ByteWidth / desc_buf.StructureByteStride;

	return device->CreateUnorderedAccessView(buffer, &desc_uav, uav);
}

int ULPDiff(float a, float b) {
	FloatCmp ia(a), ib(b);
	return std::abs(ia.i - ib.i);
}