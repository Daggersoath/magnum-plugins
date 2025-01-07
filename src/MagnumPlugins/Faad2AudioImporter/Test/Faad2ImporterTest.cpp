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

#include <string> /** @todo remove once AbstractImporter is <string>-free */
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h> /** @todo remove once AbstractImporter is <string>-free */
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Audio/AbstractImporter.h>
#include <Magnum/DebugTools/CompareImage.h>

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test { namespace {

struct Faad2ImporterTest: TestSuite::Tester {
    explicit Faad2ImporterTest();

    void empty();

    void error();
    void mono();
    void stereo();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

Faad2ImporterTest::Faad2ImporterTest() {
    addTests({&Faad2ImporterTest::empty,

              &Faad2ImporterTest::error,
              &Faad2ImporterTest::mono,
              &Faad2ImporterTest::stereo,

              &Faad2ImporterTest::openTwice,
              &Faad2ImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef FAAD2AUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(FAAD2AUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void Faad2ImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("Faad2AudioImporter");

    Containers::String out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out, "Audio::Faad2Importer::openData(): can't read file header\n");
}

/* AAC files with zero samples have 0 bytes, so it's the same as above */

void Faad2ImporterTest::error() {
    Containers::String out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("Faad2AudioImporter");
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(FAAD2AUDIOIMPORTER_TEST_DIR, "error.aac")));
    CORRADE_COMPARE(out, "Audio::Faad2Importer::openData(): decoding error\n");
}

void Faad2ImporterTest::mono() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("Faad2AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(FAAD2AUDIOIMPORTER_TEST_DIR, "mono.aac")));

    {
        CORRADE_EXPECT_FAIL("Even though `file` reports mono.aac as mono, FAAD2 decodes it as stereo.");
        CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    }
    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 96000);

    Containers::Array<char> data = importer->data();
    /* Two channels, 16 bits, 1024 samples each */
    CORRADE_COMPARE(data.size(), 1024*2*2);

    /* Testing via CompareImage because there's off-by-one difference in some
       older versions. */
    constexpr UnsignedShort expected[]{
        2663, 2663, 2668, 2668, 1663, 1663, 514, 514, 0, 0, 188, 188, 541, 541,
        552, 552, 225, 225, 65483, 65483, 2, 2, 267, 267, 400, 400, 241, 241,
        65506, 65506, 65404, 65404
    };
    CORRADE_COMPARE_WITH(
        (ImageView2D{PixelFormat::R16UI, {8, 4}, data.prefix(32*2)}),
        (ImageView2D{PixelFormat::R16UI, {8, 4}, expected}),
        (DebugTools::CompareImage{1.0f, 0.5625f}));
}

void Faad2ImporterTest::stereo() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("Faad2AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(FAAD2AUDIOIMPORTER_TEST_DIR, "stereo.aac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 44100);

    Containers::Array<char> data = importer->data();
    /* Two channels, 16 bits, 1024 samples each */
    CORRADE_COMPARE(data.size(), 1024*2*2);

    /* Testing via CompareImage because there's off-by-one difference in some
       older versions. */
    constexpr UnsignedShort expected[]{
        16518, 16518, 3364, 3364, 59935, 59935, 421, 421, 63882, 63882, 64205,
        64205, 2501, 2501, 65266, 65266, 186, 186, 1051, 1051, 64651, 64651,
        401, 401, 182, 182, 64756, 64756, 61, 61, 65122, 65122
    };
    CORRADE_COMPARE_WITH(
        (ImageView2D{PixelFormat::R16UI, {8, 4}, data.prefix(32*2)}),
        (ImageView2D{PixelFormat::R16UI, {8, 4}, expected}),
        (DebugTools::CompareImage{1.0f, 0.625f}));
}

void Faad2ImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("Faad2AudioImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(FAAD2AUDIOIMPORTER_TEST_DIR, "mono.aac")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(FAAD2AUDIOIMPORTER_TEST_DIR, "mono.aac")));

    /* Shouldn't crash, leak or anything */
}

void Faad2ImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("Faad2AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(FAAD2AUDIOIMPORTER_TEST_DIR, "mono.aac")));

    {
        CORRADE_EXPECT_FAIL("Even though `file` reports mono.aac as mono, FAAD2 decodes it as stereo.");
        CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    }
    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 96000);

    /* Testing via CompareImage because there's off-by-one difference in some
       older versions. */
    constexpr UnsignedShort expected[]{
        2663, 2663, 2668, 2668, 1663, 1663, 514, 514, 0, 0, 188, 188, 541, 541,
        552, 552, 225, 225, 65483, 65483, 2, 2, 267, 267, 400, 400, 241, 241,
        65506, 65506, 65404, 65404
    };

    /* Verify that everything is working the same way on second use */
    {
        Containers::Array<char> data = importer->data();
        CORRADE_COMPARE(data.size(), 1024*2*2);
        CORRADE_COMPARE_WITH(
            (ImageView2D{PixelFormat::R16UI, {8, 4}, data.prefix(32*2)}),
            (ImageView2D{PixelFormat::R16UI, {8, 4}, expected}),
            (DebugTools::CompareImage{1.0f, 0.5625f}));
    } {
        Containers::Array<char> data = importer->data();
        CORRADE_COMPARE(data.size(), 1024*2*2);
        CORRADE_COMPARE_WITH(
            (ImageView2D{PixelFormat::R16UI, {8, 4}, data.prefix(32*2)}),
            (ImageView2D{PixelFormat::R16UI, {8, 4}, expected}),
            (DebugTools::CompareImage{1.0f, 0.5625f}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::Faad2ImporterTest)
