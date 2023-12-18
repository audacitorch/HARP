/**
 * @file
 * @brief Base class for any MIDI 2 MIDI models. MIDI 2 MIDI models take MIDI
 * messages from a MIDI source and will output a MIDI buffer to be read from.
 * @author Frank Cwitkowitz
 */
#pragma once

#include <any>
#include <map>
#include <string>

using std::any;
using std::map;
using std::string;

/**
 * @class Midi2Midi
 * @brief Class that represents Midi2Midi Model.
 */
class Midi2Midi {
protected:
  bool save_buffer_to_file(const juce::MidiBuffer &buffer,
                           const juce::File &outputFile) const {
    // TODO
  }

  bool load_buffer_from_file(const juce::File &inputFile,
                             juce::MidiBuffer &buffer) const {
    // TODO
    return true;
  }

public:
  /**
   * @brief Processes a buffer of MIDI data with the model.
   * @param bufferToProcess Buffer to be processed by the model.
   */
  virtual void process(juce::MidiBuffer *bufferToProcess) const = 0;
};
