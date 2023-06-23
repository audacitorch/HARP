/**
  * @file
  * @brief This file is part of the JUCE examples.
  * 
  * Copyright (c) 2022 - Raw Material Software Limited
  * The code included in this file is provided under the terms of the ISC license
  * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
  * To use, copy, modify, and/or distribute this software for any purpose with or
  * without fee is hereby granted provided that the above copyright notice and
  * this permission notice appear in all copies.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
  * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
  * PURPOSE, ARE DISCLAIMED.
  * 
  * @brief Base class for any wave 2 wave models. Wave 2 wave models take samples
  * from an audio source and will output a sample buffer to be read from.
  * @author hugo flores garcia, aldo aguilar
  */
#pragma once

#include <string>
#include <map>
#include <any>

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

using std::map;
using std::string;
using std::any;

/**
 * @class Wave2Wave
 * @brief Class that represents Wave2Wave Model. 
 */
class Wave2Wave {
protected:
    bool save_buffer_to_file(
        const juce::AudioBuffer<float> &buffer, 
        const juce::File& outputFile, 
        int sampleRate
    ) const;

    bool load_buffer_from_file(
        const juce::File& inputFile,
        juce::AudioBuffer<float> &buffer,
        int targetSampleRate
    ) const;

public:
    /**
     * @brief Processes a buffer of audio data with the model.
     * @param bufferToProcess Buffer to be processed by the model.
     * @param sampleRate The sample rate of the audio data.
     * @param kwargs A map of parameters for the model.
     */
    virtual void process(
        juce::AudioBuffer<float> *bufferToProcess, 
        int sampleRate,
        const map<string, any> &kwargs
    ) const = 0;
};
