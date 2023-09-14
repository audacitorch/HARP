/**
 * @file
 * @brief This is the interface for any kind of deep learning model. this will
 * be the base class for wave 2 wave, wave 2 label, text 2 wave, midi 2 wave,
 * wave 2 midi, etc.
 * @author hugo flores garcia, aldo aguilar
 */

#pragma once

#include <any>
#include <map>
#include <string>
#include <unordered_map>

#include "juce_events/juce_events.h"
#include "juce_audio_basics/juce_audio_basics.h"

using std::any;
using std::map;
using std::string;

namespace modelparams {
  inline bool contains(const map<string, any> &params, const string &key) {
    return params.find(key) != params.end();
  }
}

struct ModelCard {
  int sampleRate;
  std::string name;
  std::string description;
  std::string author;
  std::vector<std::string> tags;
};


/**
 * @class Model
 * @brief Abstract class for different types of deep learning processors.
 */
class Model : public juce::ChangeBroadcaster {
public:
  /**
   * @brief Load the model parameters.
   * @param params A map of parameters for the model.
   * @return A boolean indicating whether the model was loaded successfully.
   */
  virtual bool load(const map<string, any> &params) = 0;

  /**
   * @brief Checks if the model is ready.
   * @return A boolean indicating whether the model is ready.
   */
  virtual bool ready() const = 0;

public:
  // //! provides access to the model card (metadata)
  ModelCard &card();

  void addListener(juce::ChangeListener *listener){
    addChangeListener(listener);
  }

protected: 
  ModelCard m_card;

private:
  std::function<void(std::string)> editorsWidgetCreationCallback;

};
