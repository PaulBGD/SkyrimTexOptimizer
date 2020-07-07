/* Copyright (C) 2019 G'k
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <stdexcept>
#include <iostream>
#include "textures.hpp"

TexturesOptimizer::TexturesOptimizer() {
    if (!createDevice(0, _pDevice.GetAddressOf())) {
        std::cerr << "failed to create device for adapter 0" << std::endl;
    }

    // Initialize COM (needed for WIC)
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (FAILED(hr))
        throw std::runtime_error("Failed to initialize COM. Textures processing won't work.");
}

bool TexturesOptimizer::getDXGIFactory(IDXGIFactory1 **pFactory) {
    if (!pFactory)
        return false;

    *pFactory = nullptr;

    typedef HRESULT(WINAPI *pfn_CreateDXGIFactory1)(REFIID riid, _Out_ void **ppFactory);

    static pfn_CreateDXGIFactory1 sCreateDXGIFactory1 = nullptr;

    if (!sCreateDXGIFactory1) {
        const HMODULE hModDXGI = LoadLibraryW(L"dxgi.dll");
        if (!hModDXGI)
            return false;

        sCreateDXGIFactory1 = reinterpret_cast<pfn_CreateDXGIFactory1>(
                reinterpret_cast<void *>(GetProcAddress(hModDXGI, "CreateDXGIFactory1")));
        if (!sCreateDXGIFactory1)
            return false;
    }

    return SUCCEEDED(sCreateDXGIFactory1(IID_PPV_ARGS(pFactory)));
}

bool TexturesOptimizer::createDevice(const uint32_t adapter, ID3D11Device **pDevice) const {
    if (!pDevice)
        return false;

    *pDevice = nullptr;

    static PFN_D3D11_CREATE_DEVICE s_DynamicD3D11CreateDevice = nullptr;

    if (!s_DynamicD3D11CreateDevice) {
        const HMODULE hModD3D11 = LoadLibraryW(L"d3d11.dll");
        if (!hModD3D11)
            return false;

        s_DynamicD3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
                reinterpret_cast<void *>(GetProcAddress(hModD3D11, "D3D11CreateDevice")));
        if (!s_DynamicD3D11CreateDevice)
            return false;
    }

    D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
    };

    const UINT createDeviceFlags = 0;

    Microsoft::WRL::ComPtr<IDXGIAdapter> pAdapter;
    if (adapter >= 0) {
        Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
        if (getDXGIFactory(dxgiFactory.GetAddressOf())) {
            if (FAILED(dxgiFactory->EnumAdapters(adapter, pAdapter.GetAddressOf()))) {
                return false;
            }
        }
    }

    D3D_FEATURE_LEVEL fl;
    HRESULT hr = s_DynamicD3D11CreateDevice(pAdapter.Get(),
                                            (pAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                            nullptr,
                                            createDeviceFlags,
                                            featureLevels,
                                            _countof(featureLevels),
                                            D3D11_SDK_VERSION,
                                            pDevice,
                                            &fl,
                                            nullptr);
    if (SUCCEEDED(hr)) {
        if (fl < D3D_FEATURE_LEVEL_11_0) {
            D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
            hr = (*pDevice)->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
            if (FAILED(hr))
                memset(&hwopts, 0, sizeof(hwopts));

            if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x) {
                if (*pDevice) {
                    (*pDevice)->Release();
                    *pDevice = nullptr;
                }
                hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }
    }

    if (SUCCEEDED(hr)) {
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        hr = (*pDevice)->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));

        return SUCCEEDED(hr);
    }
    std::cerr << "Failed to create GPU device: " << std::system_category().message(hr) << std::endl;
    return false;
}

TexturesOptimizer::TexOptOptionsResult TexturesOptimizer::processArguments(const std::optional<size_t> &tWidth,
                                                                           const std::optional<size_t> &tHeight) {
    TexOptOptionsResult result{};
    //Calculating target width and height

    result.tWidth = _info.width;
    result.tHeight = _info.height;
    if (tWidth.has_value()) {
        while (result.tWidth > tWidth.value()) {
            result.tWidth /= 2;
            result.tHeight /= 2;
        }
    }

    result.bNeedsResize = !isPowerOfTwo() || (result.tHeight != _info.height || result.tWidth != _info.width);

    result.bNeedsCompress = canBeCompressed();

    result.bNeedsMipmaps = _info.mipLevels != calculateOptimalMipMapsNumber() && canHaveMipMaps();

    return result;
}

bool TexturesOptimizer::doCPUWork(const std::optional<size_t> &tWidth,
                                  const std::optional<size_t> &tHeight) {
    //Getting operations to perform. This will be repeated several times, since the texture will change after each operation

    auto options = processArguments(tWidth, tHeight);

    //Decompressing
    if (isCompressed()) {
        if (!decompress())
            return false;
    }

    options = processArguments(tWidth, tHeight);

    //Fitting to a power of two or resizing
    if (tWidth != _info.width || tHeight != _info.height) {
        if (options.bNeedsResize)
            if (!resize(options.tWidth, options.tHeight))
                return false;
    }

    if (options.bNeedsMipmaps)
        return generateMipMaps();

    return true;
}

bool TexturesOptimizer::doGPUWork(uint32_t adapter) {
    DXGI_FORMAT targetFormat = _info.format;
    if (canBeCompressed()) {
        targetFormat = DXGI_FORMAT_BC7_UNORM;
    }
    return convert(adapter, targetFormat);
}

bool TexturesOptimizer::canBeCompressed() const {
//    return DirectX::IsCompressed(_info.format) || _info.width < 4 || _info.height < 4;
    return true;
}

std::wstring ConvertStringToWstring(const std::string &str) {
    if (str.empty()) {
        return std::wstring();
    }
    int num_chars = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), str.length(), NULL, 0);
    std::wstring wstrTo;
    if (num_chars) {
        wstrTo.resize(num_chars);
        if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), str.length(), &wstrTo[0], num_chars)) {
            return wstrTo;
        }
    }
    return std::wstring();
}

bool TexturesOptimizer::open(const std::string &filePath, const TextureType &type) {
    _image.reset(new(std::nothrow) DirectX::ScratchImage);
    if (!_image)
        return false;

    modifiedCurrentTexture = false;

    HRESULT hr = S_FALSE;
    switch (type) {
        case DDS:
            const DWORD ddsFlags = DirectX::DDS_FLAGS_NONE;
            const auto wide = ConvertStringToWstring(filePath);
            hr = LoadFromDDSFile(wide.c_str(), ddsFlags, &_info, *_image);
            if (FAILED(hr))
                return false;

            if (DirectX::IsTypeless(_info.format)) {
                _info.format = DirectX::MakeTypelessUNORM(_info.format);

                if (DirectX::IsTypeless(_info.format))
                    return false;

                _image->OverrideFormat(_info.format);
            }
    }
    if (SUCCEEDED(hr)) {
        _type = type;
        _name = filePath;
        return true;
    }
    return false;
}

bool TexturesOptimizer::read(const std::string& filePath, const char *data, const size_t length, const TextureType &type) {
    _image.reset(new(std::nothrow) DirectX::ScratchImage);
    if (!_image)
        return false;

    modifiedCurrentTexture = false;

    HRESULT hr = S_FALSE;
    switch (type) {
        case DDS:
            const DWORD ddsFlags = DirectX::DDS_FLAGS_NONE;
            hr = LoadFromDDSMemory(data, length, ddsFlags, &_info, *_image);
            if (FAILED(hr))
                return false;

            if (DirectX::IsTypeless(_info.format)) {
                _info.format = DirectX::MakeTypelessUNORM(_info.format);

                if (DirectX::IsTypeless(_info.format))
                    return false;

                _image->OverrideFormat(_info.format);
            }
    }
    if (SUCCEEDED(hr)) {
        _type = type;
        _name = filePath;
        return true;
    }
    return false;
}

bool TexturesOptimizer::decompress() {
    if (!DirectX::IsCompressed(_info.format))
        return false;

    const auto img = _image->GetImage(0, 0, 0);
    assert(img);
    const size_t nimg = _image->GetImageCount();

    std::unique_ptr<DirectX::ScratchImage> timage(new(std::nothrow) DirectX::ScratchImage);
    if (!timage) {
        return false;
    }

    const HRESULT hr = Decompress(img, nimg, _info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage);
    if (FAILED(hr)) {
        return false;
    }

    const auto &tinfo = timage->GetMetadata();
    _info.format = tinfo.format;

    if (!compareInfo(_info, tinfo))
        return false;

    _image.swap(timage);
    modifiedCurrentTexture = true;
    return true;
}

bool TexturesOptimizer::resize(size_t targetWidth, size_t targetHeight) {
    if (_info.width <= targetWidth && _info.height <= targetHeight)
        return true;

    fitPowerOfTwo(targetWidth, targetHeight);

    std::unique_ptr<DirectX::ScratchImage> timage(new(std::nothrow) DirectX::ScratchImage);
    if (!timage) {
        return false;
    }

    const auto imgs = _image->GetImages();
    if (!imgs)
        return false;

    const DWORD filter = DirectX::TEX_FILTER_FANT | DirectX::TEX_FILTER_SEPARATE_ALPHA;
    const HRESULT hr = Resize(imgs, _image->GetImageCount(), _info, targetWidth, targetHeight, filter, *timage);
    if (FAILED(hr)) {
        return false;
    }

    auto &tinfo = timage->GetMetadata();

    assert(tinfo.width == targetWidth && tinfo.height == targetHeight && tinfo.mipLevels == 1);
    _info.width = tinfo.width;
    _info.height = tinfo.height;
    _info.mipLevels = 1;

    if (!compareInfo(_info, tinfo))
        return false;

    _image.swap(timage);
    modifiedCurrentTexture = true;
    return true;
}

bool TexturesOptimizer::canHaveMipMaps() {
    return DirectX::IsCompressed(_info.format) || _info.width < 4 || _info.height < 4;
}

bool TexturesOptimizer::generateMipMaps() {
    const size_t tMips = calculateOptimalMipMapsNumber();

    if (_info.mipLevels != 1 && _info.mipLevels != tMips) {
        // Mips generation only works on a single base image, so strip off existing mip levels
        // Also required for preserve alpha coverage so that existing mips are regenerated
        std::unique_ptr<DirectX::ScratchImage> timage(new(std::nothrow) DirectX::ScratchImage);
        if (!timage) {
            return false;
        }

        DirectX::TexMetadata mdata = _info;
        mdata.mipLevels = 1;
        HRESULT hr = timage->Initialize(mdata);
        if (FAILED(hr)) {
            return false;
        }

        for (size_t i = 0; i < _info.arraySize; ++i) {
            const DWORD filter = DirectX::TEX_FILTER_FANT | DirectX::TEX_FILTER_SEPARATE_ALPHA;
            hr = CopyRectangle(*_image->GetImage(0, i, 0),
                               DirectX::Rect(0, 0, _info.width, _info.height),
                               *timage->GetImage(0, i, 0),
                               filter,
                               0,
                               0);
            if (FAILED(hr)) {
                return false;
            }
        }

        modifiedCurrentTexture = true;
        _image.swap(timage);
        _info.mipLevels = _image->GetMetadata().mipLevels;
    }

    if ((_info.width > 1 || _info.height > 1 || _info.depth > 1)) {
        std::unique_ptr<DirectX::ScratchImage> timage(new(std::nothrow) DirectX::ScratchImage);
        if (!timage) {
            return false;
        }

        //Forcing non wic since WIC won't work on my computer, and thus probably on other computers
        const DWORD filter = DirectX::TEX_FILTER_FANT | DirectX::TEX_FILTER_SEPARATE_ALPHA;
        const HRESULT hr = GenerateMipMaps(_image->GetImages(),
                                           _image->GetImageCount(),
                                           _image->GetMetadata(),
                                           filter,
                                           tMips,
                                           *timage);
        if (FAILED(hr)) {
            return false;
        }

        const auto &tinfo = timage->GetMetadata();
        _info.mipLevels = tinfo.mipLevels;

        if (!compareInfo(_info, tinfo))
            return false;

        _image.swap(timage);
        modifiedCurrentTexture = true;
    }
    return true;
}

size_t TexturesOptimizer::calculateOptimalMipMapsNumber() const {
    size_t height = _info.height;
    size_t width = _info.width;
    size_t tMips = 1;
    //Calculating mips levels
    while (height > 1 || width > 1) {
        if (height > 1)
            height >>= 1;

        if (width > 1)
            width >>= 1;

        ++tMips;
    }
    return tMips;
}

bool TexturesOptimizer::isCompressed() const {
    return DirectX::IsCompressed(_info.format);
}

DirectX::TexMetadata TexturesOptimizer::getInfo() const {
    return _info;
}

bool TexturesOptimizer::convert(uint32_t adapter, const DXGI_FORMAT &format) {
    if (DirectX::IsCompressed(format)) {
        return convertWithCompression(adapter, format);
    }
    return convertWithoutCompression(format);
}

bool TexturesOptimizer::convertWithoutCompression(const DXGI_FORMAT &format) {
    if (_info.format != format) {
        std::unique_ptr<DirectX::ScratchImage> timage(new(std::nothrow) DirectX::ScratchImage);
        if (!timage) {
            return false;
        }

        const HRESULT hr = Convert(_image->GetImages(),
                                   _image->GetImageCount(),
                                   _image->GetMetadata(),
                                   format,
                                   0,
                                   DirectX::TEX_THRESHOLD_DEFAULT,
                                   *timage);
        if (FAILED(hr)) {
            return false;
        }

        const auto &tinfo = timage->GetMetadata();
        if (tinfo.format != format)
            return false;
        _info.format = tinfo.format;

        if (!compareInfo(_info, tinfo))
            return false;

        _image.swap(timage);
        modifiedCurrentTexture = true;
    }
    return true;
}

bool TexturesOptimizer::convertWithCompression(uint32_t adapter, const DXGI_FORMAT &format) {
    if (isCompressed() || _image->GetMetadata().format == format)
        return true;

    const auto img = _image->GetImage(0, 0, 0);
    if (!img)
        return false;

    const size_t nimg = _image->GetImageCount();

    std::unique_ptr<DirectX::ScratchImage> timage(new(std::nothrow) DirectX::ScratchImage);
    if (!timage) {
        return false;
    }

    bool bc6hbc7 = false;
    if (format == DXGI_FORMAT_BC6H_TYPELESS || format == DXGI_FORMAT_BC6H_UF16 || format == DXGI_FORMAT_BC6H_SF16
        || format == DXGI_FORMAT_BC7_TYPELESS || format == DXGI_FORMAT_BC7_UNORM
        || format == DXGI_FORMAT_BC7_UNORM_SRGB) {
        bc6hbc7 = true;
    }

    HRESULT hr;
    if (bc6hbc7 && _pDevice) {
        hr = Compress(_pDevice.Get(),
                      img,
                      nimg,
                      _info,
                      format,
                      DirectX::TEX_COMPRESS_FLAGS::TEX_COMPRESS_BC7_USE_3SUBSETS,
                      1.f,
                      *timage);
    } else {
        std::cerr << "uh oh, compressing with cpu! has pDevice: " << (_pDevice != nullptr) << std::endl;
        hr = Compress(img,
                      nimg,
                      _info,
                      format,
                      DirectX::TEX_COMPRESS_DEFAULT | DirectX::TEX_FILTER_SEPARATE_ALPHA,
                      DirectX::TEX_THRESHOLD_DEFAULT,
                      *timage);
    }

    if (FAILED(hr)) {
        return false;
    }

    const auto &tinfo = timage->GetMetadata();
    _info.format = tinfo.format;

    if (!compareInfo(_info, tinfo))
        return false;

    _image.swap(timage);
    modifiedCurrentTexture = true;
    return true;
}

bool TexturesOptimizer::saveToFile(const std::string &filePath) const {
    const auto img = _image->GetImage(0, 0, 0);
    if (!img)
        return false;
    const size_t nimg = _image->GetImageCount();

    const auto wide = ConvertStringToWstring(filePath);
    const HRESULT hr = SaveToDDSFile(img, nimg, _info, DirectX::DDS_FLAGS_NONE, wide.c_str());
    return SUCCEEDED(hr);
}

void TexturesOptimizer::fitPowerOfTwo(size_t &resultX, size_t &resultY) {
    //Finding nearest power of two
    size_t x = 1;
    while (x < resultX)
        x *= 2;
    resultX = x;

    size_t y = 1;
    while (y < resultY)
        y *= 2;
    resultY = y;
}

bool TexturesOptimizer::isPowerOfTwo() const {
    return ((_info.width != 0) && !(_info.width & (_info.width - 1)))
           && ((_info.height != 0) && !(_info.height & (_info.height - 1)));
}

bool TexturesOptimizer::compareInfo(const DirectX::TexMetadata &info1, const DirectX::TexMetadata &info2) {
    const bool isSame = info1.width == info2.width || info1.height == info2.height || info1.depth == info2.depth
                        || info1.arraySize == info2.arraySize || info1.miscFlags == info2.miscFlags
                        || info1.format == info2.format || info1.dimension == info2.dimension;

    return isSame;
}
                                                                                                                                                                                                                                                                                                                                                                                                         