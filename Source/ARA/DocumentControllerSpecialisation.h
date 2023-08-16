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
 * @brief Implementation of the ARA document controller.
 * This controller is needed for any ARA plugin and handles functions for
 * editing and playback of audio, analysis of content from the host, and it
 * maintains the ARA model graph. More information can be found on the offical
 * JUCE documentation:
 * https://docs.juce.com/master/classARADocumentControllerSpecialisation.html#details
 * @author JUCE, aldo aguilar, hugo flores garcia
 */

#pragma once

#include "EditorRenderer.h"
#include "PlaybackRenderer.h"
#include "../DeepLearning/TorchModel.h"

#include "../Util/PreviewState.h"

/**
 * @class TensorJuceDocumentControllerSpecialisation
 * @brief Specialises ARA's document controller, with added functionality for
 * audio modifications, playback rendering, and editor rendering.
 */
class TensorJuceDocumentControllerSpecialisation
    : public ARADocumentControllerSpecialisation,
      private ProcessingLockInterface {
public:
  /**
   * @brief Constructor.
   * Uses ARA's document controller specialisation's constructor.
   */
  using ARADocumentControllerSpecialisation::
      ARADocumentControllerSpecialisation;

  PreviewState previewState; ///< Preview state.

  std::shared_ptr<TorchWave2Wave> getModel() { return mModel; }
protected:
  void willBeginEditing(
      ARADocument *) override; ///< Called when beginning to edit a document.
  void didEndEditing(
      ARADocument *) override; ///< Called when editing a document ends.

  /**
   * @brief Creates an audio modification.
   * @return A new AudioModification instance.
   */
  ARAAudioModification *doCreateAudioModification(
      ARAAudioSource *audioSource, ARA::ARAAudioModificationHostRef hostRef,
      const ARAAudioModification *optionalModificationToClone) noexcept
      override;

  /**
   * @brief Creates a playback renderer.
   * @return A new PlaybackRenderer instance.
   */
  ARAPlaybackRenderer *doCreatePlaybackRenderer() noexcept override;

  /**
   * @brief Creates an editor renderer.
   * @return A new EditorRenderer instance.
   */
  EditorRenderer *doCreateEditorRenderer() noexcept override;

  bool
  doRestoreObjectsFromStream(ARAInputStream &input,
                             const ARARestoreObjectsFilter *filter) noexcept
      override; ///< Restores objects from a stream.
  bool doStoreObjectsToStream(ARAOutputStream &output,
                              const ARAStoreObjectsFilter *filter) noexcept
      override; ///< Stores objects to a stream.

private:
  ScopedTryReadLock getProcessingLock() override; ///< Gets the processing lock.
  std::shared_ptr<TorchWave2Wave> mModel {new TorchWave2Wave()}; ///< Model for audio processing.

  ReadWriteLock processBlockLock; ///< Lock for processing blocks.
};