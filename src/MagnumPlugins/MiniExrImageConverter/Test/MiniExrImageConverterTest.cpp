/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024
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

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Half.h>
#include <Magnum/Trade/AbstractImageConverter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct MiniExrImageConverterTest: TestSuite::Tester {
    explicit MiniExrImageConverterTest();

    void wrongFormat();

    void rgb();
    void rgba();

    void unsupportedMetadata();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    ImageConverterFlags converterFlags;
    ImageFlags2D imageFlags;
    const char* message;
} UnsupportedMetadataData[]{
    {"1D array", {}, ImageFlag2D::Array,
        "1D array images are unrepresentable in OpenEXR, saving as a regular 2D image"},
    {"1D array, quiet", ImageConverterFlag::Quiet, ImageFlag2D::Array,
        nullptr},
};

MiniExrImageConverterTest::MiniExrImageConverterTest() {
    addTests({&MiniExrImageConverterTest::wrongFormat,

              &MiniExrImageConverterTest::rgb,
              &MiniExrImageConverterTest::rgba});

    addInstancedTests({&MiniExrImageConverterTest::unsupportedMetadata},
        Containers::arraySize(UnsupportedMetadataData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef MINIEXRIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(MINIEXRIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void MiniExrImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("MiniExrImageConverter");

    const char data[4]{};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::R16F, {1, 1}, data}));
    CORRADE_COMPARE(out, "Trade::MiniExrImageConverter::convertToData(): unsupported format PixelFormat::R16F\n");
}

using namespace Math::Literals;

const Half RgbData[]{
    /* Skip */
    {}, {}, {}, {},

    0.00_h, 0.25_h, 0.50_h, {},
    0.75_h, 1.00_h, 1.25_h, {},
    1.50_h, 1.75_h, 2.00_h, {}
};

const ImageView2D Rgb{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGB16F, {1, 3}, RgbData};

const Half RgbaData[]{
    0.00_h, 0.25_h, 0.50_h, 9.0_h,
    0.75_h, 1.00_h, 1.25_h, 9.0_h,
    1.50_h, 1.75_h, 2.00_h, 9.0_h
};

const ImageView2D Rgba{PixelFormat::RGBA16F, {1, 3}, RgbaData};

void MiniExrImageConverterTest::rgb() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("MiniExrImageConverter");
    CORRADE_COMPARE(converter->extension(), "exr");
    CORRADE_COMPARE(converter->mimeType(), "image/x-exr");

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Rgb);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(MINIEXRIMAGECONVERTER_TEST_DIR, "image.exr"),
        TestSuite::Compare::StringToFile);
}

void MiniExrImageConverterTest::rgba() {
    Containers::Optional<Containers::Array<char>> data = _manager.instantiate("MiniExrImageConverter")->convertToData(Rgba);
    CORRADE_VERIFY(data);
    /* Alpha is ignored, so it is the same file */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(MINIEXRIMAGECONVERTER_TEST_DIR, "image.exr"),
        TestSuite::Compare::StringToFile);
}

void MiniExrImageConverterTest::unsupportedMetadata() {
    auto&& data = UnsupportedMetadataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("MiniExrImageConverter");
    converter->addFlags(data.converterFlags);

    const char imageData[8]{};
    ImageView2D image{PixelFormat::RGBA16F, {1, 1}, imageData, data.imageFlags};

    Containers::String out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData(image));
    if(!data.message)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, Utility::format("Trade::MiniExrImageConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::MiniExrImageConverterTest)
