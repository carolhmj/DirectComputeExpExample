// DirectCompute.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include "pch.h"
#include <iostream>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")


int main()
{
	ID3D11Device*               device = nullptr;
	ID3D11DeviceContext*        deviceContext = nullptr;
	ID3D11ComputeShader*        shader = nullptr;

	ID3D11Buffer*               inBuffer = nullptr;
	ID3D11Buffer*               outBuffer = nullptr;

	ID3D11ShaderResourceView*   inSRV = nullptr;
	ID3D11UnorderedAccessView*  outUAV = nullptr;

	static const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	HRESULT hr;

	std::cout << "Trying to create device...\n";
	
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 1, D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
	if (FAILED(hr)) {
		std::cout << "We were unable to create a hardware device! Error code was: " << std::hex << hr << "\n";
		return EXIT_FAILURE;
	}

    std::cout << "Creating compute shader...\n";

	static const D3D_SHADER_MACRO defines[] = { "USE_STRUCTURED_BUFFERS" };
	DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* codeBlob = nullptr;
	
	hr = D3DCompileFromFile(L"ExpCompute.hlsl", defines, nullptr, "main", "cs_5_0", shaderFlags, 0, &codeBlob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			std::cout << "A shader compile error has occurred!\n\t" << errorBlob->GetBufferPointer() << "\n";
		}
	}

	hr = device->CreateComputeShader(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), nullptr, &shader);
	if (FAILED(hr)) {
		std::cout << "An error has occurred while creating the shader! Error code was: " << std::hex << hr << "\n";
	}

	return EXIT_SUCCESS;
}
