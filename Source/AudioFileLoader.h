#pragma once
#include <functional>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * AudioFileLoader
 *
 * Loads an audio file on a background juce::Thread so the message thread is
 * never blocked.  When loading is complete the supplied callback is invoked on
 * the message thread via juce::MessageManager::callAsync.
 *
 * Usage:
 *   loader = std::make_unique<AudioFileLoader>();
 *   loader->loadAsync(file, formatManager,
 *       [](juce::AudioBuffer<float> buf, double sr) { ... });
 */
class AudioFileLoader : public juce::Thread {
public:
  using LoadCallback =
      std::function<void(juce::AudioBuffer<float>, double sampleRate)>;

  AudioFileLoader() : juce::Thread("AudioFileLoader") {}

  ~AudioFileLoader() override { stopThread(4000); }

  /** Start loading the file in the background.
   *  @param file           The audio file to load.
   *  @param formatManager  A registered AudioFormatManager (must outlive this
   * call).
   *  @param onComplete     Called on the message thread when loading finishes.
   *                        Receives an empty buffer on failure.
   */
  void loadAsync(const juce::File &file,
                 juce::AudioFormatManager &formatManager,
                 LoadCallback onComplete) {
    // Stop any previous load
    stopThread(4000);

    pendingFile = file;
    pendingManager = &formatManager;
    completionCallback = std::move(onComplete);

    startThread();
  }

  // -------------------------------------------------------------------------
  // Static helpers (unchanged from original)
  // -------------------------------------------------------------------------

  static void
  browseForFile(std::function<void(const juce::File &)> onFileChosen) {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select an audio file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.mp3;*.flac;*.ogg");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [onFileChosen, chooser](const juce::FileChooser &fc) {
                           auto result = fc.getResult();
                           if (result.existsAsFile())
                             onFileChosen(result);
                         });
  }

  static bool isSupportedFile(const juce::String &fileName) {
    return fileName.endsWith(".wav") || fileName.endsWith(".mp3") ||
           fileName.endsWith(".flac") || fileName.endsWith(".ogg");
  }

private:
  void run() override {
    juce::AudioBuffer<float> resultBuffer;
    double resultSampleRate = 0.0;

    if (pendingManager != nullptr) {
      std::unique_ptr<juce::AudioFormatReader> reader(
          pendingManager->createReaderFor(pendingFile));

      if (reader != nullptr) {
        resultSampleRate = reader->sampleRate;
        const int numChannels = (int)reader->numChannels;
        const int numSamples = (int)reader->lengthInSamples;

        resultBuffer.setSize(numChannels, numSamples);
        reader->read(&resultBuffer, 0, numSamples, 0, true, true);
      }
    }

    // Marshal result back to the message thread
    auto callback = completionCallback;
    juce::MessageManager::callAsync([callback,
                                     resultBuffer = std::move(resultBuffer),
                                     resultSampleRate]() mutable {
      if (callback)
        callback(std::move(resultBuffer), resultSampleRate);
    });
  }

  juce::File pendingFile;
  juce::AudioFormatManager *pendingManager = nullptr;
  LoadCallback completionCallback;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileLoader)
};
