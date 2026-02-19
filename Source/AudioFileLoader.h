#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class AudioFileLoader {
public:
    static void browseForFile(std::function<void(const juce::File&)> onFileChosen) {
        auto chooser = std::make_unique<juce::FileChooser>(
            "Select an audio file...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.mp3;*.flac;*.ogg"
        );

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [onFileChosen, chooser = std::move(chooser)](const juce::FileChooser& fc) mutable {
                auto result = fc.getResult();
                if (result.existsAsFile()) {
                    onFileChosen(result);
                }
            });
    }

    static bool isSupportedFile(const juce::String& fileName) {
        return fileName.endsWith(".wav") || fileName.endsWith(".mp3") || 
               fileName.endsWith(".flac") || fileName.endsWith(".ogg");
    }
};
