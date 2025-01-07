#ifndef Magnum_Trade_StbDxtImageConverter_h
#define Magnum_Trade_StbDxtImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class @ref Magnum::Trade::StbDxtImageConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/StbDxtImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBDXTIMAGECONVERTER_BUILD_STATIC
    #ifdef StbDxtImageConverter_EXPORTS
        #define MAGNUM_STBDXTIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBDXTIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBDXTIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBDXTIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STBDXTIMAGECONVERTER_EXPORT
#define MAGNUM_STBDXTIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief BC1/BC3 compressor using stb_dxt
@m_since_latest_{plugins}

Converts uncompressed 2D, 2D array or cube and 3D RGB and RGBA images to
block-compressed BC1/BC3 images using the [stb_dxt](https://github.com/nothings/stb)
library.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [stb_dxt](https://github.com/nothings/stb) library by Fabian Giesen and
    Sean Barrett, released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/nothings/stb/blob/3a1174060a7dd4eb652d4e6854bc4cd98c159200/stb_dxt.h#L701-L717),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/nothings/stb/blob/3a1174060a7dd4eb652d4e6854bc4cd98c159200/stb_dxt.h#L683-L699),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-StbDxtImageConverter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    via the base @ref AbstractImageConverter interface. See its documentation
    for introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_STBDXTIMAGECONVERTER` is enabled when building Magnum Plugins. To
use as a dynamic plugin, load @cpp "StbDxtImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_STBDXTIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StbDxtImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StbDxtImageConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::StbDxtImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StbDxtImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StbDxtImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins and
@ref file-formats for more information.

@section Trade-StbDxtImageConverter-behavior Behavior and limitations

@m_class{m-note m-warning}

@par Security note
    As the [stb repository README](https://github.com/nothings/stb/blob/master/README.md)
    itself says, the `stb_*` libraries are developed with a goal of simplicity
    and ease of use. As such, they may crash or pose a security risk on broken
    or malicious input, and they may likely be slower or have less features
    than canonical implementations of given format. Consider using alternatives
    listed in @ref file-formats if your project is dealing with untrusted data.

An @ref PixelFormat::RGBA8Unorm / @relativeref{PixelFormat,RGBA8Srgb} input
will produce a compressed @ref ImageData2D with
@ref CompressedPixelFormat::Bc3RGBAUnorm /
@relativeref{CompressedPixelFormat,Bc3RGBASrgb}. If the input is
@ref PixelFormat::RGB8Unorm / @relativeref{PixelFormat,RGBA8Srgb}, an image
with @ref CompressedPixelFormat::Bc1RGBUnorm /
@relativeref{CompressedPixelFormat,Bc1RGBSrgb} is returned instead. You can
override alpha channel presence in the output by explicitly enabling or
disabling the @cb{.ini} alpha @ce @ref Trade-StbDxtImageConverter-configuration "configuration option".

Image flags are passed through unchanged. 3D images are compressed
slice-by-slice, independently of whether @ref ImageFlag3D::Array and/or
@ref ImageFlag3D::CubeMap or neither is set. On the other hand, if a 2D image
with @ref ImageFlag2D::Array is passed, the conversion will fail as it's not
possible to represent 1D array images without a significant loss in quality
and layer cross-talk.

The input image size is expected to be divisible by four in the X and Y
dimension. If your image doesn't fit this requirement, you have to pad/crop or
resample it first. Since 3D images are compressed slice-by-slice, there's no
restriction on the Z dimension.

Unlike image converters dealing with uncompressed pixel formats, the image
* *isn't* Y-flipped on export due to the nontrivial amount of work involved
with Y-flipping block-compressed data. This is in line with importers of
compressed pixel formats such as @ref AstcImporter, @ref DdsImporter or
@ref KtxImporter, which don't Y-flip compressed formats on import either.

@section Trade-StbDxtImageConverter-configuration Plugin-specific configuration

Various compressor options can be set through @ref configuration(). See below
for all options and their default values:

@snippet MagnumPlugins/StbDxtImageConverter/StbDxtImageConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_STBDXTIMAGECONVERTER_EXPORT StbDxtImageConverter: public AbstractImageConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit StbDxtImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_STBDXTIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_STBDXTIMAGECONVERTER_LOCAL Containers::Optional<ImageData2D> doConvert(const ImageView2D& image) override;
        MAGNUM_STBDXTIMAGECONVERTER_LOCAL Containers::Optional<ImageData3D> doConvert(const ImageView3D& image) override;
};

}}

#endif
