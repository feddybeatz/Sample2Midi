/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   cnn_contour_model_json;
    const int            cnn_contour_model_jsonSize = 335797;

    extern const char*   cnn_note_model_json;
    const int            cnn_note_model_jsonSize = 111888;

    extern const char*   cnn_onset_1_model_json;
    const int            cnn_onset_1_model_jsonSize = 247581;

    extern const char*   cnn_onset_2_model_json;
    const int            cnn_onset_2_model_jsonSize = 21667;

    extern const char*   features_model_onnx;
    const int            features_model_onnxSize = 111305;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 5;

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
