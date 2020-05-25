/* Copyright (C) 2019 G'k
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once

//#include "FilesystemOperations.h"
//#include "PluginsOperations.h"
//#include "Profiles.h"
//#include "pch.h"

#include "libs/DirectXTex/DirectXTex.h"
#include <optional>
#include <wrl/client.h>

class TexturesOptimizer final
{

public:
    TexturesOptimizer();

    enum TextureType
    {
        DDS,
    };

    bool open(const std::string &filePath, const TextureType &type);

    bool saveToFile(const std::string &filePath) const;
    /*!
   * \brief Decompress the current texture. It is required to use several functions.
   * \return False if an error happens
   */
    bool decompress();
    /*!
   * \brief Convenience function, that will use appropriately convertWithCompression or convertWithoutCompression
   * \param format The format to use
   * \return False in case of error
   */
    bool convert(uint32_t adapter, const DXGI_FORMAT &format);
    /*!
   * \brief Compress the file using the provided compression format,
   * \param format The format to use
   */
    bool convertWithCompression(uint32_t adapter, const DXGI_FORMAT &format);
    bool convertWithoutCompression(const DXGI_FORMAT &format);
    /*!
   * \brief Check if a texture is compressed
   * \return True if the file is compressed
   */
    bool isCompressed() const;
    [[nodiscard]] bool canBeCompressed() const;
    /*!
   * \brief Perform various optimizations on the current texture
   * \return False if an error happens
   */
    bool doCPUWork(const std::optional<size_t> &tWidth,
                  const std::optional<size_t> &tHeight);

    bool doGPUWork(uint32_t adapter);

    bool resize(size_t targetWidth, size_t targetHeight);

    static void fitPowerOfTwo(size_t &resultX, size_t &resultY);
    bool isPowerOfTwo() const;

    bool generateMipMaps();
    bool canHaveMipMaps();
    size_t calculateOptimalMipMapsNumber() const;

    DirectX::TexMetadata getInfo() const;

    static bool compareInfo(const DirectX::TexMetadata &info1, const DirectX::TexMetadata &info2);

    struct TexOptOptionsResult
    {
        bool bNeedsResize;
        bool bNeedsCompress;
        bool bNeedsMipmaps;
        size_t tWidth;
        size_t tHeight;
    };

    TexOptOptionsResult processArguments(const std::optional<size_t> &tWidth,
                                         const std::optional<size_t> &tHeight);

    bool modifiedCurrentTexture = false;

private:
    std::unique_ptr<DirectX::ScratchImage> _image{};
    DirectX::TexMetadata _info{};
    std::string _name;
    TextureType _type;

    Microsoft::WRL::ComPtr<ID3D11Device> _pDevice;

    bool createDevice(uint32_t adapter, ID3D11Device **pDevice) const;
    static bool getDXGIFactory(IDXGIFactory1 **pFactory) ;
};
