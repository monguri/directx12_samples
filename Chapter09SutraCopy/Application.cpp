#include "Application.h"

#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ComPtr<ID3D12Device> _dev = nullptr;
ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
ComPtr<IDXGISwapChain4> _swapchain = nullptr;

std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
{
	// フォルダ区切りが/でも\でも対応できるようにする。
	// rfindは見つからなかったらepos(-1、0xffffffff)を返す。
	int pathIndex1 = (int)modelPath.rfind('/');
	int pathIndex2 = (int)modelPath.rfind('\\');
	int pathIndex = max(pathIndex1, pathIndex2);
	const std::string& folderPath = modelPath.substr(0, pathIndex + 1);
	return folderPath + texPath;
}

std::string GetExtension(const std::string& path)
{
	size_t idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

std::pair<std::string, std::string> SplitFileName(const std::string& path, const char splitter = '*')
{
	size_t idx = path.find(splitter);
	std::pair<std::string, std::string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(idx + 1, path.length() - idx - 1);
	return ret;
}

std::wstring GetWideStringFromString(const std::string& str)
{
	// MultiByteToWideCharで使うには先にwchar_t配列を必要なサイズの確保が必要。
	// 固定長配列を返してもいいがstd::wstringの方が扱いやすいので
	// 先に長さを取得してresizeしておく

	// 引数の文字列の長さ取得
	int num1 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		nullptr,
		0
	);
	std::wstring wstr;
	wstr.resize(num1);

	int num2 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		&wstr[0],
		num1
	);

	assert(num1 == num2);
	return wstr;
}

ComPtr<ID3D12Resource> CreateGrayGradientTexture()
{
	// テクスチャバッファ作成
	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		256
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	HRESULT result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	// 上が白くて下が黒いグレースケールグラデーションテクスチャの作成
	// 4byte 4x256のテクスチャ
	std::vector<unsigned int> data(4 * 256);
	// グレースケール値
	unsigned int grayscale = 0xff;
	for (auto it = data.begin(); it != data.end(); it += 4) // インクリメントは行単位
	{
		// グレースケール値をRGBA4チャンネルに適用したもの
		unsigned int grayscaleRGBA = (grayscale << 24) | (grayscale << 16) | (grayscale << 8) | grayscale;
		// 行の4ピクセル同時に塗る
		std::fill(it, it + 4, grayscaleRGBA);
		// グレースケール値を下げる
		--grayscale;
	}

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		(UINT)(4 * sizeof(unsigned int)),
		(UINT)(sizeof(unsigned int) * data.size())
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	return texbuff;
}

ComPtr<ID3D12Resource> CreateWhiteTexture()
{
	// テクスチャバッファ作成
	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		4
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	HRESULT result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	// 4byte 4x4のテクスチャ
	std::vector<unsigned char> data(4 * 4 * 4);
	// 0xffで埋めるためRGBAは(255, 255, 255, 255)になる
	std::fill(data.begin(), data.end(), 0xff);

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		(UINT)data.size()
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	return texbuff;
}

ComPtr<ID3D12Resource> CreateBlackTexture()
{
	// テクスチャバッファ作成
	CD3DX12_HEAP_PROPERTIES texHeapProp(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		4
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	HRESULT result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	// 4byte 4x4のテクスチャ
	std::vector<unsigned char> data(4 * 4 * 4);
	// 0x00で埋めるためRGBAは(0, 0, 0, 0)になる
	std::fill(data.begin(), data.end(), 0x00);

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		(UINT)data.size()
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	return texbuff;
}

// ファイル拡張子ごとにロード関数を使い分けるためのテーブル
using LoadLambda_t = std::function<HRESULT(const std::wstring& path, TexMetadata* meta, ScratchImage& img)>;
std::map<std::string, LoadLambda_t> loadLambdaTable;

// ファイルパスごとにリソースをキャッシュして使いまわすためのテーブル
std::map<std::string, ComPtr<ID3D12Resource>> _resourceTable;

ComPtr<ID3D12Resource> LoadTextureFromFile(const std::string& texPath)
{
	// キャッシュ済みならそれを返す
	// イテレータの型は複雑なのでautoを使う
	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end())
	{
		return _resourceTable[texPath];
	}

	// WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	const std::wstring& wtexpath = GetWideStringFromString(texPath);
	const std::string& ext = GetExtension(texPath);

	HRESULT result = loadLambdaTable[ext](
		wtexpath,
		&metadata,
		scratchImg
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	const Image* img = scratchImg.GetImage(0, 0, 0);

	// テクスチャバッファ作成
	CD3DX12_HEAP_PROPERTIES texHeapProp(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		(UINT)metadata.width,
		(UINT)metadata.height,
		(UINT16)metadata.arraySize,
		(UINT16)metadata.mipLevels
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		(UINT)img->rowPitch,
		(UINT)img->slicePitch
	);
	if (FAILED(result))
	{
		return nullptr;
	}

	// テーブルにキャッシュ
	_resourceTable[texPath] = texbuff;
	return texbuff;
}

void EnableDebugLayer()
{
	ComPtr<ID3D12Debug> debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.ReleaseAndGetAddressOf()))))
	{
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

struct SceneData
{
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX proj;
	XMFLOAT3 eye;
};

Application::Application()
{
}

Application::~Application()
{
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}

bool Application::Init()
{
	HRESULT result = CoInitializeEx(0, COINIT_MULTITHREADED);
	DebugOutputFormatString("Show window test.");

	CreateGameWindow();

#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG

	result = CreateDXGIDevice();
	result = CreateCommand();

	// スワップチェインの生成。一関数しか呼ばないので関数化しない
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue.Get(),
		_hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf()
	);

	result = CreateFinalRenderTarget(swapchainDesc);


	loadLambdaTable["sph"] = loadLambdaTable["spa"] = loadLambdaTable["bmp"] = loadLambdaTable["png"] =
	[](const std::wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	loadLambdaTable["tga"] =
	[](const std::wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	loadLambdaTable["dds"] =
	[](const std::wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromDDSFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	// 深度バッファ作成
	D3D12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		window_width,
		window_height,
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	CD3DX12_HEAP_PROPERTIES depthHeapProp(D3D12_HEAP_TYPE_DEFAULT);

	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
	
	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf())
	);
	
	// デプスステンシルビュー作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_dev->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());


	// フェンスの生成
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));

	// PMDヘッダ格納データ
	struct PMDHeader
	{
		float version;
		char model_name[20];
		char comment[256];
	};

	char signature[3];
	PMDHeader pmdheader = {};
	FILE* fp = nullptr;
	std::string strModelPath = "model/初音ミク.pmd";
	//std::string strModelPath = "model/初音ミクmetal.pmd";
	//std::string strModelPath = "model/巡音ルカ.pmd";
	errno_t error = fopen_s(&fp, strModelPath.c_str(), "rb");
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;
	fread(&vertNum, sizeof(vertNum), 1, fp);

#pragma pack(1)
	// PMDマテリアルデータ読み出し用
	// PMDはフォンシェーディングの模様
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// 本来ここでこの構造体は2バイトのパディングが発生する
		unsigned int indicesNum;
		char texFilePath[20];
	}; // pack(1)がなければ70バイトのはずが72バイトになる
#pragma pack()

	constexpr unsigned int pmdvertex_size = 38;
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertBuff.ReleaseAndGetAddressOf())
	);

	// 頂点バッファへのデータ書き込み
	unsigned char* vertMap = nullptr;
	result = _vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(vertices.begin(), vertices.end(), vertMap);
	_vertBuff->Unmap(0, nullptr);

	// 頂点バッファービューの用意
	_vbView.BufferLocation = _vertBuff->GetGPUVirtualAddress();
	_vbView.SizeInBytes = (UINT)vertices.size();
	_vbView.StrideInBytes = pmdvertex_size;

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	// マテリアル情報の読み出し
	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);

	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	ComPtr<ID3D12Resource> whiteTex = CreateWhiteTexture();
	ComPtr<ID3D12Resource> blackTex = CreateBlackTexture();
	ComPtr<ID3D12Resource> gradTex = CreateGrayGradientTexture();
	assert(whiteTex != nullptr);
	assert(blackTex != nullptr);
	assert(gradTex != nullptr);

	{
		std::vector<PMDMaterial> pmdMaterials(materialNum);
		fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

		for (int i = 0; i < pmdMaterials.size(); ++i)
		{
			_materials[i].indicesNum = pmdMaterials[i].indicesNum;
			_materials[i].material.diffuse = pmdMaterials[i].diffuse;
			_materials[i].material.alpha = pmdMaterials[i].alpha;
			_materials[i].material.specular = pmdMaterials[i].specular;
			_materials[i].material.specularity = pmdMaterials[i].specularity;
			_materials[i].material.ambient = pmdMaterials[i].ambient;
			_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;

			_textureResources[i] = nullptr;
			_sphResources[i] = nullptr;
			_spaResources[i] = nullptr;
			_toonResources[i] = nullptr;

			// トゥーンシェーディング用のCLUTテクスチャリソースのロード
			std::string toonFilePath = "toon/";
			char toonFileName[16];
			sprintf_s(toonFileName, 16, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1); // この足し算だと255+1は256で扱われるが、現状toon00.bmpはないためこのままにしておく
			toonFilePath += toonFileName;
			_toonResources[i] = LoadTextureFromFile(toonFilePath);

			if (strlen(pmdMaterials[i].texFilePath) > 0)
			{
				// 通常テクスチャ、sph、spaのリソースのロード
				std::string texFileName = pmdMaterials[i].texFilePath;
				std::string sphFileName = "";
				std::string spaFileName = "";

				if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
				{
					const std::pair<std::string, std::string>& namepair = SplitFileName(texFileName);
					if (GetExtension(namepair.first) == "sph")
					{
						sphFileName = namepair.first;
						texFileName = namepair.second;
					}
					else if (GetExtension(namepair.first) == "spa")
					{
						spaFileName = namepair.first;
						texFileName = namepair.second;
					}
					else
					{
						texFileName = namepair.first;
						if (GetExtension(namepair.second) == "sph")
						{
							sphFileName = namepair.second;
						}
						else if (GetExtension(namepair.second) == "spa")
						{
							spaFileName = namepair.second;
						}
					}
				}
				else
				{
					if (GetExtension(texFileName) == "sph")
					{
						sphFileName = texFileName;
						texFileName = "";
					}
					else if (GetExtension(texFileName) == "spa")
					{
						spaFileName = texFileName;
						texFileName = "";
					}
				}

				if (texFileName.length() > 0)
				{
					const std::string& texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
					_textureResources[i] = LoadTextureFromFile(texFilePath);
				}

				if (sphFileName.length() > 0)
				{
					const std::string& sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
					_sphResources[i] = LoadTextureFromFile(sphFilePath);
				}

				if (spaFileName.length() > 0)
				{
					const std::string& spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
					_spaResources[i] = LoadTextureFromFile(spaFilePath);
				}
			}

			if (_toonResources[i] == nullptr)
			{
				_toonResources[i] = gradTex;
			}

			if (_textureResources[i] == nullptr)
			{
				_textureResources[i] = whiteTex;
			}

			if (_sphResources[i] == nullptr)
			{
				_sphResources[i] = whiteTex;
			}

			if (_spaResources[i] == nullptr)
			{
				_spaResources[i] = blackTex;
			}
		}
	}

	fclose(fp);

	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_idxBuff.ReleaseAndGetAddressOf())
	);

	// インデックスバッファへのデータ書き込み
	unsigned short* idxMap = nullptr;
	result = _idxBuff->Map(0, nullptr, (void**)&idxMap);
	std::copy(indices.begin(), indices.end(), idxMap);
	_idxBuff->Unmap(0, nullptr);

	// インデックスバッファービューの用意
	_ibView.BufferLocation = _idxBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = (UINT)(indices.size() * sizeof(indices[0]));

	// マテリアルバッファを作成
	// sizeof(MaterialForHlsl)の44バイトを256でアラインメントしているので256。
	// かなりもったいない
	// TODO:定数バッファをマテリアル数だけ作っているから、一個にまとめられないか？
	size_t materialBuffSize = (sizeof(MaterialForHlsl) + 0xff) & ~0xff;
	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);

	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	for (const Material& m : _materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}
	_materialBuff->Unmap(0, nullptr);

	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = materialNum * 5; // MaterialForHlslのCBVと通常テクスチャとsphとspaとCLUTのSRVの5つずつ
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialDescHeap.ReleaseAndGetAddressOf()));

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = (UINT)materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	// Formatはテクスチャによる

	D3D12_CPU_DESCRIPTOR_HANDLE matDescHeapH = _materialDescHeap->GetCPUDescriptorHandleForHeapStart();
	UINT incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (unsigned int i = 0; i < materialNum; ++i)
	{
		_dev->CreateConstantBufferView(
			&matCBVDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		srvDesc.Format = _textureResources[i]->GetDesc().Format;
		_dev->CreateShaderResourceView(
			_textureResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _sphResources[i]->GetDesc().Format;
		_dev->CreateShaderResourceView(
			_sphResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _spaResources[i]->GetDesc().Format;
		_dev->CreateShaderResourceView(
			_spaResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _toonResources[i]->GetDesc().Format;
		_dev->CreateShaderResourceView(
			_toonResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;
	}

	// シェーダの準備
	// TODO:これはグラフィックスパイプラインステートが完成したら解放されてもよい？
	ComPtr<ID3DBlob> _vsBlob = nullptr;
	ComPtr<ID3DBlob> _psBlob = nullptr;

	ComPtr<ID3DBlob> errorBlob = nullptr;
	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_vsBlob,
		&errorBlob
	);
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("ファイルが見当たりません。");
		}
		else
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			OutputDebugStringA(errstr.c_str());
		}

		exit(1);
	}

	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_psBlob,
		&errorBlob
	);
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("ファイルが見当たりません。");
		}
		else
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			OutputDebugStringA(errstr.c_str());
		}

		exit(1);
	}

	// 頂点レイアウトの設定
	D3D12_INPUT_ELEMENT_DESC posInputLayout;
	posInputLayout.SemanticName = "POSITION";
	posInputLayout.SemanticIndex = 0;
	posInputLayout.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	posInputLayout.InputSlot = 0;
	posInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	posInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	posInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC normalInputLayout;
	normalInputLayout.SemanticName = "NORMAL";
	normalInputLayout.SemanticIndex = 0;
	normalInputLayout.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	normalInputLayout.InputSlot = 0;
	normalInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	normalInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	normalInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC texcoordInputLayout;
	texcoordInputLayout.SemanticName = "TEXCOORD";
	texcoordInputLayout.SemanticIndex = 0;
	texcoordInputLayout.Format = DXGI_FORMAT_R32G32_FLOAT;
	texcoordInputLayout.InputSlot = 0;
	texcoordInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	texcoordInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	texcoordInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC bonenoInputLayout;
	bonenoInputLayout.SemanticName = "BONE_NO";
	bonenoInputLayout.SemanticIndex = 0;
	bonenoInputLayout.Format = DXGI_FORMAT_R16G16_UINT;
	bonenoInputLayout.InputSlot = 0;
	bonenoInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	bonenoInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	bonenoInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC weightInputLayout;
	weightInputLayout.SemanticName = "WEIGHT";
	weightInputLayout.SemanticIndex = 0;
	weightInputLayout.Format = DXGI_FORMAT_R8_UINT;
	weightInputLayout.InputSlot = 0;
	weightInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	weightInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	weightInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC edgeflgInputLayout;
	edgeflgInputLayout.SemanticName = "EDGE_FLG";
	edgeflgInputLayout.SemanticIndex = 0;
	edgeflgInputLayout.Format = DXGI_FORMAT_R8_UINT;
	edgeflgInputLayout.InputSlot = 0;
	edgeflgInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	edgeflgInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	edgeflgInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC inputLayouts[] = {
		posInputLayout,
		normalInputLayout,
		texcoordInputLayout,
		bonenoInputLayout,
		weightInputLayout,
		edgeflgInputLayout,
	};

	CD3DX12_DESCRIPTOR_RANGE descTblRange[3] = {}; // VS用のCBVとPS用のCBVとテクスチャ用のSRV
	// VS用の行列
	descTblRange[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		0
	);
	// PS用のMaterialData
	descTblRange[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		1
	);
	// PS用の通常テクスチャとsphとspatとCLUT
	descTblRange[2].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		4, // 通常テクスチャとsphとspatとCLUT
		0
	);

	CD3DX12_ROOT_PARAMETER rootParams[2] = {};
	rootParams[0].InitAsDescriptorTable(1, &descTblRange[0]);
	rootParams[1].InitAsDescriptorTable(2, &descTblRange[1]);

	// サンプラ用のルートシグネチャ設定
	CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	samplerDescs[0].Init(0);
	samplerDescs[1].Init(
		1,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	// ルートシグネチャ作成
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(
		2,
		rootParams,
		2,
		samplerDescs,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// TODO:これはグラフィックスパイプラインステートが完成したら解放されてもよい？
	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);

	result = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(_rootsignature.ReleaseAndGetAddressOf())
	);

	// グラフィックスパイプラインステート作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _rootsignature.Get();
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(_vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(_psBlob.Get());
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.StencilEnable = false;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpipeline.InputLayout.pInputElementDescs = inputLayouts;
	gpipeline.InputLayout.NumElements = _countof(inputLayouts);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipelinestate.ReleaseAndGetAddressOf()));

	// 定数バッファ用データ
	// 定数バッファ作成
	_worldMat = XMMatrixIdentity();
	XMFLOAT3 eye(0, 15, -15);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);
	XMMATRIX viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV4,
		(float)window_width / (float)window_height,
		1.0f,
		100.0f
	);

	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_constBuff.ReleaseAndGetAddressOf())
	);

	result = _constBuff->Map(0, nullptr, (void**)&_mapScene);
	_mapScene->world = _worldMat;
	_mapScene->view = viewMat;
	_mapScene->proj = projMat;
	_mapScene->eye = eye;

	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC basicHeapDesc = {};
	basicHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	basicHeapDesc.NodeMask = 0;
	basicHeapDesc.NumDescriptors = 1;
	basicHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&basicHeapDesc, IID_PPV_ARGS(_basicDescHeap.ReleaseAndGetAddressOf()));
	CD3DX12_CPU_DESCRIPTOR_HANDLE basicHeapHandle(_basicDescHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_constBuff->GetDesc().Width;

	_dev->CreateConstantBufferView(
		&cbvDesc,
		basicHeapHandle
	);
	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);

	CD3DX12_VIEWPORT viewport(_backBuffers[0].Get());
	CD3DX12_RECT scissorrect(0, 0, window_width, window_height);

	MSG msg = {};
	float angle = 0.0f;

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		angle += 0.005f;
		_worldMat = XMMatrixRotationY(angle);
		_mapScene->world = _worldMat;

		UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

		// Present状態からレンダーターゲット状態にする
		_cmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
		);

		_cmdList->SetPipelineState(_pipelinestate.Get());

		// レンダーターゲットを指定する
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvH(_rtvHeaps->GetCPUDescriptorHandleForHeapStart());
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvH(_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		// レンダーターゲットをクリアする
		float clearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// 三角形を描画する
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &_vbView);
		_cmdList->IASetIndexBuffer(&_ibView);

		_cmdList->SetGraphicsRootSignature(_rootsignature.Get());

		ID3D12DescriptorHeap* bdh[] = {_basicDescHeap.Get()};
		_cmdList->SetDescriptorHeaps(1, bdh);
		_cmdList->SetGraphicsRootDescriptorTable(0, _basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		ID3D12DescriptorHeap* mdh[] = {_materialDescHeap.Get()};
		_cmdList->SetDescriptorHeaps(1, mdh);

		// マテリアルセクションごとにマテリアルを切り替えて描画
		CD3DX12_GPU_DESCRIPTOR_HANDLE materialH(_materialDescHeap->GetGPUDescriptorHandleForHeapStart());
		unsigned int idxOffset = 0;
		UINT cbvsrvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5; // CBVと通常テクスチャとsphとspaとCLUTのSRV

		for (const Material& m : _materials)
		{
			_cmdList->SetGraphicsRootDescriptorTable(1, materialH);
			_cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}

		// レンダーターゲット状態からPresent状態にする
		_cmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
		);

		// コマンドリストのクローズ
		_cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { _cmdList.Get() };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);
		_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

		if (_fence->GetCompletedValue() != _fenceVal)
		{
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		// コマンドリストのクリア
		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator.Get(), nullptr);

		// スワップ
		_swapchain->Present(1, 0);
	}
}

void Application::Terminate()
{
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

void Application::CreateGameWindow()
{
	// ウィンドウの生成
	_windowClass.cbSize = sizeof(WNDCLASSEX);
	_windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	_windowClass.lpszClassName = _T("DX12Sample");
	_windowClass.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&_windowClass);

	RECT wrc = {0, 0, window_width, window_height};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	_hwnd = CreateWindow(
		_windowClass.lpszClassName,
		_T("DX12サンプル"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		_windowClass.hInstance,
		nullptr
	);
}

HRESULT Application::CreateDXGIDevice()
{
	// DXGIFactoryの生成
	HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// NVIDIAアダプタの選択
	std::vector<ComPtr<IDXGIAdapter>> adapters;
	ComPtr<IDXGIAdapter> tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	ComPtr<IDXGIAdapter> nvidiaAdapter = nullptr;
	for (ComPtr<IDXGIAdapter> adapter : adapters)
	{
		DXGI_ADAPTER_DESC desc = {};
		adapter->GetDesc(&desc);
		std::wstring strDesc = desc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			nvidiaAdapter = adapter;
			break;
		}
	}

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	result = S_FALSE;

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (D3D_FEATURE_LEVEL level : levels)
	{
		if (D3D12CreateDevice(nvidiaAdapter.Get(), level, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf())) == S_OK)
		{
			featureLevel = level;
			result = S_OK;
			break;
		}
	}

	return result;
}

HRESULT Application::CreateCommand()
{
	// コマンドアロケータとコマンドリストの生成
	HRESULT result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// コマンドキューの生成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT Application::CreateFinalRenderTarget(const DXGI_SWAP_CHAIN_DESC1& swapchainDesc)
{
	// ディスクリプタヒープの生成と、2枚のバックバッファ用のレンダーターゲットビューの生成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(_rtvHeaps->GetCPUDescriptorHandleForHeapStart());

	// SRGBテクスチャ対応
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	_backBuffers.resize(swapchainDesc.BufferCount);
	for (UINT i = 0; i < swapchainDesc.BufferCount; ++i)
	{
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(_backBuffers[i].ReleaseAndGetAddressOf()));
		if (FAILED(result))
		{
			assert(false);
			return result;
		}

		_dev->CreateRenderTargetView(_backBuffers[i].Get(), &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return result;
}

