/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   arrowright_png;
    const int            arrowright_pngSize = 1007;

    extern const char*   arrowleft_png;
    const int            arrowleft_pngSize = 1010;

    extern const char*   settings_png;
    const int            settings_pngSize = 1024;

    extern const char*   folderuploadfill_png;
    const int            folderuploadfill_pngSize = 511;

    extern const char*   pausefill_png;
    const int            pausefill_pngSize = 320;

    extern const char*   playfill_png;
    const int            playfill_pngSize = 420;

    extern const char*   rewindfill_png;
    const int            rewindfill_pngSize = 623;

    extern const char*   speedfill_png;
    const int            speedfill_pngSize = 600;

    extern const char*   stopfill_png;
    const int            stopfill_pngSize = 324;

    extern const char*   volumedownfill_png;
    const int            volumedownfill_pngSize = 793;

    extern const char*   volumemutefill_png;
    const int            volumemutefill_pngSize = 768;

    extern const char*   volumeupfill_png;
    const int            volumeupfill_pngSize = 1060;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 12;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
