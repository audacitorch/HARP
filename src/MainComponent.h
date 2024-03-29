/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2022 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             MainComponent
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Plays an audio file.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2022, linux_make, androidstudio, xcode_iphone

 type:             Component
 mainClass:        MainComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
// #include <juce_events/timers/juce_Timer.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "WebModel.h"
#include "CtrlComponent.h"
#include "TitledTextBox.h"
#include "ThreadPoolJob.h"

using namespace juce;


// this only calls the callback ONCE
class TimedCallback : public Timer
{
public:
    TimedCallback(std::function<void()> callback, int interval) : mCallback(callback), mInterval(interval) {
        startTimer(mInterval);
    }

    ~TimedCallback() {
        stopTimer();
    }

    void timerCallback() override {
        mCallback();
        stopTimer();
    }
private:
    std::function<void()> mCallback;
    int mInterval;
};

inline Colour getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour uiColour, Colour fallback = Colour (0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*> (&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour (uiColour);

    return fallback;
}


inline std::unique_ptr<InputSource> makeInputSource (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return std::make_unique<AndroidDocumentInputSource> (doc);

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return std::make_unique<FileInputSource> (url.getLocalFile());
   #endif

    return std::make_unique<URLInputSource> (url);
}

inline std::unique_ptr<OutputStream> makeOutputStream (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return doc.createOutputStream();

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return url.getLocalFile().createOutputStream();
   #endif

    return url.createOutputStream();
}



class DemoThumbnailComp  : public Component,
                           public ChangeListener,
                           public FileDragAndDropTarget,
                           public ChangeBroadcaster,
                           private ScrollBar::Listener,
                           private Timer
{
public:
    DemoThumbnailComp (AudioFormatManager& formatManager,
                       AudioTransportSource& source,
                       Slider& slider)
        : transportSource (source),
          zoomSlider (slider),
          thumbnail (512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener (this);

        addAndMakeVisible (scrollbar);
        scrollbar.setRangeLimits (visibleRange);
        scrollbar.setAutoHide (false);
        scrollbar.addListener (this);

        currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
        addAndMakeVisible (currentPositionMarker);
    }

    ~DemoThumbnailComp() override
    {
        scrollbar.removeListener (this);
        thumbnail.removeChangeListener (this);
    }

    void setURL (const URL& url)
    {
        if (auto inputSource = makeInputSource (url))
        {
            thumbnail.setSource (inputSource.release());

            Range<double> newRange (0.0, thumbnail.getTotalLength());
            scrollbar.setRangeLimits (newRange);
            setRange (newRange);

            startTimerHz (40);
        }
    }

    URL getLastDroppedFile() const noexcept { return lastFileDropped; }

    void setZoomFactor (double amount)
    {
        if (thumbnail.getTotalLength() > 0)
        {
            auto newScale = jmax (0.001, thumbnail.getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
            auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);

            setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
        }
    }

    void setRange (Range<double> newRange)
    {
        visibleRange = newRange;
        scrollbar.setCurrentRange (visibleRange);
        updateCursorPosition();
        repaint();
    }

    void setFollowsTransport (bool shouldFollow)
    {
        isFollowingTransport = shouldFollow;
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colours::darkgrey);
        g.setColour (Colours::lightblue);

        if (thumbnail.getTotalLength() > 0.0)
        {
            auto thumbArea = getLocalBounds();

            thumbArea.removeFromBottom (scrollbar.getHeight() + 4);
            thumbnail.drawChannels (g, thumbArea.reduced (2),
                                    visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
        else
        {
            g.setFont (14.0f);
            g.drawFittedText ("(No audio file selected)", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized() override
    {
        scrollbar.setBounds (getLocalBounds().removeFromBottom (14).reduced (2));
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }

    bool isInterestedInFileDrag (const StringArray& /*files*/) override
    {
        return true;
    }

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        lastFileDropped = URL (File (files[0]));
        sendChangeMessage();
    }

    void mouseDown (const MouseEvent& e) override
    {
        mouseDrag (e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (canMoveTransport())
            transportSource.setPosition (jmax (0.0, xToTime ((float) e.x)));
    }

    void mouseUp (const MouseEvent&) override
    {
        transportSource.start();
    }

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        if (thumbnail.getTotalLength() > 0.0)
        {
            auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
            newStart = jlimit (0.0, jmax (0.0, thumbnail.getTotalLength() - (visibleRange.getLength())), newStart);

            if (canMoveTransport())
                setRange ({ newStart, newStart + visibleRange.getLength() });

            if (wheel.deltaY != 0.0f)
                zoomSlider.setValue (zoomSlider.getValue() - wheel.deltaY);

            repaint();
        }
    }

private:
    AudioTransportSource& transportSource;
    Slider& zoomSlider;
    ScrollBar scrollbar  { false };

    AudioThumbnailCache thumbnailCache  { 5 };
    AudioThumbnail thumbnail;
    Range<double> visibleRange;
    bool isFollowingTransport = false;
    URL lastFileDropped;

    DrawableRectangle currentPositionMarker;

    float timeToX (const double time) const
    {
        if (visibleRange.getLength() <= 0)
            return 0;

        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    bool canMoveTransport() const noexcept
    {
        return ! (isFollowingTransport && transportSource.isPlaying());
    }

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (! (isFollowingTransport && transportSource.isPlaying()))
                setRange (visibleRange.movedToStartAt (newRangeStart));
    }

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());

        currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - scrollbar.getHeight())));
    }
};

//==============================================================================
class MainComponent  : public Component,
                          #if (JUCE_ANDROID || JUCE_IOS)
                           private Button::Listener,
                          #endif
                           private ChangeListener
{
public:
    explicit MainComponent(const URL& initialFileURL = URL()): jobsFinished(0), totalJobs(0),
        jobProcessorThread(customJobs, jobsFinished, totalJobs, processBroadcaster)
    {
        addAndMakeVisible (zoomLabel);
        zoomLabel.setFont (Font (15.00f, Font::plain));
        zoomLabel.setJustificationType (Justification::centredRight);
        zoomLabel.setEditable (false, false, false);
        zoomLabel.setColour (TextEditor::textColourId, Colours::black);
        zoomLabel.setColour (TextEditor::backgroundColourId, Colour (0x00000000));

        addAndMakeVisible (followTransportButton);
        followTransportButton.onClick = [this] { updateFollowTransportState(); };

       #if (JUCE_ANDROID || JUCE_IOS)
        addAndMakeVisible (chooseFileButton);
        chooseFileButton.addListener (this);
       #else
        addAndMakeVisible(chooseFileButton);
        chooseFileButton.onClick = [this] { openFileChooser(); };
       #endif

        addAndMakeVisible (zoomSlider);
        zoomSlider.setRange (0, 1, 0);
        zoomSlider.onValueChange = [this] { thumbnail->setZoomFactor (zoomSlider.getValue()); };
        zoomSlider.setSkewFactor (2);

        thumbnail = std::make_unique<DemoThumbnailComp> (formatManager, transportSource, zoomSlider);
        addAndMakeVisible (thumbnail.get());
        thumbnail->addChangeListener (this);

        addAndMakeVisible (startStopButton);
        startStopButton.setColour (TextButton::buttonColourId, Colour (0xff79ed7f));
        startStopButton.setColour (TextButton::textColourOffId, Colours::black);
        startStopButton.onClick = [this] { startOrStop(); };

        // audio setup
        formatManager.registerBasicFormats();

        thread.startThread (Thread::Priority::normal);

       #ifndef JUCE_DEMO_RUNNER
        audioDeviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
       #endif

        audioDeviceManager.addAudioCallback (&audioSourcePlayer);
        audioSourcePlayer.setSource (&transportSource);


        // Load the initial file
        if (initialFileURL.isLocalFile())
        {
            addNewAudioFile (initialFileURL);
        }

        // initialize HARP UI
        // TODO: what happens if the model is nullptr rn?
        if (model == nullptr) {
            DBG("FATAL HARPProcessorEditor::HARPProcessorEditor: model is null");
            jassertfalse;
            return;
        }

        // Set setWantsKeyboardFocus to true for this component
        // Doing that, everytime we click outside the modelPathTextBox,
        // the focus will be taken away from the modelPathTextBox
        setWantsKeyboardFocus(true);

        // initialize load and process buttons
        processButton.setButtonText("process");
        model->ready() ? processButton.setEnabled(true)
                        : processButton.setEnabled(false);
        addAndMakeVisible(processButton);
        processButton.onClick = [this] { // Process callback
            DBG("HARPProcessorEditor::buttonClicked button listener activated");

            // set the button text to "processing {model.card().name}"
            processButton.setButtonText("processing " + String(model->card().name) + "...");
            processButton.setEnabled(false);

            // enable the cancel button
            cancelButton.setEnabled(true);
            saveButton.setEnabled(false);

            // TODO: get the current audio file and process it
            // if we don't have one, let the user know
            // TODO: need to only be able to do this if we don't have any other jobs in the threadpool right?
            if (model == nullptr){
                DBG("unhandled exception: model is null. we should probably open an error window here.");
                AlertWindow("Error", "Model is not loaded. Please load a model first.", AlertWindow::WarningIcon);
                return;
            }

            // print how many jobs are currently in the threadpool
            DBG("threadPool.getNumJobs: " << threadPool.getNumJobs());

            // empty customJobs
            customJobs.clear();

            customJobs.push_back(new CustomThreadPoolJob(
                [this] { // &jobsFinished, totalJobs
                    // Individual job code for each iteration
                    // copy the audio file, with the same filename except for an added _harp to the stem
                    model->process(currentAudioFile.getLocalFile());
                    DBG("Processing finished");
                    // load the audio file again
                    processBroadcaster.sendChangeMessage();
                    
                }
            ));

            // Now the customJobs are ready to be added to be run in the threadPool
            jobProcessorThread.signalTask();
        };

        processBroadcaster.addChangeListener(this);

        saveButton.setButtonText("commit to file (destructive)");
        addAndMakeVisible(saveButton);
        saveButton.onClick = [this] { // Save callback
            DBG("HARPProcessorEditor::buttonClicked save button listener activated");
            // copy the file to the target location
            DBG("copying from " << currentAudioFile.getLocalFile().getFullPathName() << " to " << currentAudioFileTarget.getLocalFile().getFullPathName());
            // make a backup for the undo button
            // rename the original file to have a _backup suffix
            File backupFile = File(
                currentAudioFileTarget.getLocalFile().getParentDirectory().getFullPathName() + "/"
                + currentAudioFileTarget.getLocalFile().getFileNameWithoutExtension() +
                + "_BACKUP" + currentAudioFileTarget.getLocalFile().getFileExtension()
            );

            currentAudioFileTarget.getLocalFile().copyFileTo(backupFile);
            DBG("made a backup of the original file at " << backupFile.getFullPathName());

            currentAudioFile.getLocalFile().moveFileTo(currentAudioFileTarget.getLocalFile());
            
            addNewAudioFile(currentAudioFileTarget);
            saveButton.setEnabled(false);
        };
        saveButton.setEnabled(false);


        cancelButton.setButtonText("cancel");
        cancelButton.setEnabled(false);
        addAndMakeVisible(cancelButton);
        cancelButton.onClick = [this] { // Cancel callback
            DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
            model->cancel();
        };


        loadModelButton.setButtonText("load");
        addAndMakeVisible(loadModelButton);
        loadModelButton.onClick = [this]{
            DBG("HARPProcessorEditor::buttonClicked load model button listener activated");

            // collect input parameters for the model.
            std::map<std::string, std::any> params = {
            {"url", modelPathTextBox.getText().toStdString()},
            };

            resetUI();
            // loading happens asynchronously.
            // the document controller trigger a change listener callback, which will update the UI
            threadPool.addJob([this, params] {
                DBG("executeLoad!!");
                try {
                    // timeout after 10 seconds
                    // TODO: this callback needs to be cleaned up in the destructor in case we quit
                    std::atomic<bool> success = false;
                    TimedCallback timedCallback([this, &success] {
                        if (success)
                            return;
                        DBG("HARPProcessorEditor::buttonClicked timedCallback listener activated");
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::WarningIcon,
                            "Loading Error",
                            "An error occurred while loading the WebModel: TIMED OUT! Please check that the space is awake."
                        );
                        model.reset(new WebWave2Wave());
                        loadBroadcaster.sendChangeMessage();
                        saveButton.setEnabled(false);
                    }, 10000);

                    model->load(params);
                    success = true;
                    DBG("executeLoad done!!");
                    loadBroadcaster.sendChangeMessage();
                    // since we're on a helper thread, 
                    // it's ok to sleep for 10s 
                    // to let the timeout callback do its thing
                    Thread::sleep(10000);
                } catch (const std::runtime_error& e) {
                    AlertWindow::showMessageBoxAsync(
                        AlertWindow::WarningIcon,
                        "Loading Error",
                        String("An error occurred while loading the WebModel: ") + e.what()
                    );
                    model.reset(new WebWave2Wave());
                    loadBroadcaster.sendChangeMessage();
                    saveButton.setEnabled(false);
                }
            });

            // disable the load button until the model is loaded
            loadModelButton.setEnabled(false);
            loadModelButton.setButtonText("loading...");

            // TODO: enable the cancel button
            // cancelButton.setEnabled(true);
            // if the cancel button is pressed, forget the job and reset the model and UI
            // cancelButton.onClick = [this] {
            //     DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
            //     threadPool.removeAllJobs(true, 1000);
            //     model->cancel();
            //     resetUI();
            // };

            // disable the process button until the model is loaded
            processButton.setEnabled(false);

            // set the descriptionLabel to "loading {url}..."
            // TODO: we need to get rid of the params map, and just pass the url around instead
            // since it looks like we're sticking to webmodels.
            String url = String(std::any_cast<std::string>(params.at("url")));
            descriptionLabel.setText("loading " + url + "...\n if this takes a while, check if the huggingface space is sleeping by visiting the space url below. Once the huggingface space is awake, try again." , dontSendNotification);

            // TODO: here, we should also reset the highlighting of the playback regions 


            // add a hyperlink to the hf space for the model
            // TODO: make this less hacky? 
            // we might have to append a "https://huggingface.co/spaces" to the url
            // IF the url (doesn't have localhost) and (doesn't have huggingface.co) and (doesn't have http) in it 
            // and (has only one slash in it)
            String spaceUrl = url;
            if (spaceUrl.contains("localhost") || spaceUrl.contains("huggingface.co") || spaceUrl.contains("http")) {
            DBG("HARPProcessorEditor::buttonClicked: spaceUrl is already a valid url");
            }
            else {
            DBG("HARPProcessorEditor::buttonClicked: spaceUrl is not a valid url");
            spaceUrl = "https://huggingface.co/spaces/" + spaceUrl;
            }
            spaceUrlButton.setButtonText("open " + url + " in browser");
            spaceUrlButton.setURL(URL(spaceUrl));
            addAndMakeVisible(spaceUrlButton);
        };

        loadBroadcaster.addChangeListener(this);

        std::string currentStatus = model->getStatus();
        if (currentStatus == "Status.LOADED" || currentStatus == "Status.FINISHED") {
            processButton.setEnabled(true);
            processButton.setButtonText("process");
        } else if (currentStatus == "Status.PROCESSING" || currentStatus == "Status.STARTING" || currentStatus == "Status.SENDING") {
            cancelButton.setEnabled(true);
            processButton.setButtonText("processing " + String(model->card().name) + "...");
        }
        

        // status label
        statusLabel.setText(currentStatus, dontSendNotification);
        addAndMakeVisible(statusLabel);

        // add a status timer to update the status label periodically
        mModelStatusTimer = std::make_unique<ModelStatusTimer>(model);
        mModelStatusTimer->addChangeListener(this);
        mModelStatusTimer->startTimer(100);  // 100 ms interval

        // model path textbox
        modelPathTextBox.setMultiLine(false);
        modelPathTextBox.setReturnKeyStartsNewLine(false);
        modelPathTextBox.setReadOnly(false);
        modelPathTextBox.setScrollbarsShown(false);
        modelPathTextBox.setCaretVisible(true);
        modelPathTextBox.setTextToShowWhenEmpty("path to a gradio endpoint", Colour::greyLevel(0.5f));  // Default text
        modelPathTextBox.onReturnKey = [this] { loadModelButton.triggerClick(); };
        if (model->ready()) {
            modelPathTextBox.setText(model->space_url());  // Chosen model path
        }
        addAndMakeVisible(modelPathTextBox);

        // glossary label
        glossaryLabel.setText("To view an index of available HARP-compatible models, please see our ", NotificationType::dontSendNotification);
        glossaryLabel.setJustificationType(Justification::centredRight);
        addAndMakeVisible(glossaryLabel);

        // glossary link
        glossaryButton.setButtonText("Model Glossary");
        glossaryButton.setURL(URL("https://github.com/audacitorch/HARP#available-models"));
        //glossaryButton.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(glossaryButton);

        // model controls
        ctrlComponent.setModel(model);
        addAndMakeVisible(ctrlComponent);
        ctrlComponent.populateGui();

        addAndMakeVisible(nameLabel);
        addAndMakeVisible(authorLabel);
        addAndMakeVisible(descriptionLabel);
        addAndMakeVisible(tagsLabel);

        // model card component
        // Get the modelCard from the EditorView
        auto &card = model->card();
        setModelCard(card);

        jobProcessorThread.startThread();

        // ARA requires that plugin editors are resizable to support tight integration
        // into the host UI
        setOpaque (true);
        setSize(800, 800);
        resized();
    }

    ~MainComponent() override
    {
        transportSource  .setSource (nullptr);
        audioSourcePlayer.setSource (nullptr);

        audioDeviceManager.removeAudioCallback (&audioSourcePlayer);

       #if (JUCE_ANDROID || JUCE_IOS)
        chooseFileButton.removeListener (this);
       #else

       #endif

        thumbnail->removeChangeListener (this);

        // remove listeners
        mModelStatusTimer->removeChangeListener(this);
        loadBroadcaster.removeChangeListener(this);
        processBroadcaster.removeChangeListener(this);

        jobProcessorThread.signalThreadShouldExit();
        // This will not actually run any processing task
        // It'll just make sure that the thread is not waiting
        // and it'll allow it to check for the threadShouldExit flag
        jobProcessorThread.signalTask();
        jobProcessorThread.waitForThreadToExit(-1);
    }

    void openFileChooser()
    {
        fileChooser = std::make_unique<FileChooser>("Select an audio file...", File(), "*.wav;*.aiff;*.mp3;*.flac");
        fileChooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                [this](const FileChooser& chooser)
                                {
                                    File file = chooser.getResult();
                                    if (file != File{})
                                    {
                                        URL fileURL = URL(file);
                                        addNewAudioFile(fileURL);
                                    }
                                });
    }


    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto margin = 10;  // Adjusted margin value for top and bottom spacing

        auto docViewHeight = 100;

        auto mainArea = area.removeFromTop(area.getHeight() - docViewHeight);
        auto documentViewArea = area;  // what remains is the 15% area for documentView

        // Row 1: Model Path TextBox and Load Model Button
        auto row1 = mainArea.removeFromTop(40);  // adjust height as needed
        modelPathTextBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
        loadModelButton.setBounds(row1.reduced(margin));

        // Row 2: Glossary Label and Hyperlink
        auto row2 = mainArea.removeFromTop(30);  // adjust height as needed
        glossaryLabel.setBounds(row2.removeFromLeft(row2.getWidth() * 0.8f).reduced(margin));
        glossaryButton.setBounds(row2.reduced(margin));
        glossaryLabel.setFont(Font(11.0f));
        glossaryButton.setFont(Font(11.0f), false, Justification::centredLeft);

        // Row 3: Name and Author Labels
        auto row3a = mainArea.removeFromTop(40);  // adjust height as needed
        nameLabel.setBounds(row3a.removeFromLeft(row3a.getWidth() / 2).reduced(margin));
        nameLabel.setFont(Font(20.0f, Font::bold));
        // nameLabel.setColour(Label::textColourId, mHARPLookAndFeel.textHeaderColor);
 
        auto row3b = mainArea.removeFromTop(30);
        authorLabel.setBounds(row3b.reduced(margin));
        authorLabel.setFont(Font(10.0f));

        // Row 4: Description Label
        auto row4 = mainArea.removeFromTop(80);  // adjust height as needed
        descriptionLabel.setBounds(row4.reduced(margin));
        // TODO: put the space url below the description

        // Row 4.5: Space URL Hyperlink
        auto row45 = mainArea.removeFromTop(30);  // adjust height as needed
        spaceUrlButton.setBounds(row45.reduced(margin));
        spaceUrlButton.setFont(Font(11.0f), false, Justification::centredLeft);

        // Row 5: CtrlComponent (flexible height)
        auto row5 = mainArea.removeFromTop(150);  // the remaining area is for row 4
        ctrlComponent.setBounds(row5.reduced(margin));

        // Row 6: Process Button (taken out in advance to preserve its height)
        auto row6Height = 25;  // adjust height as needed
        auto row6 = mainArea.removeFromTop(row6Height);

        // Assign bounds to processButton
        processButton.setBounds(row6.withSizeKeepingCentre(100, 20));  // centering the button in the row

        // place the cancel button to the right of the process button (justified right)
        cancelButton.setBounds(processButton.getBounds().translated(110, 0));

        // place the status label to the left of the process button (justified left)
        statusLabel.setBounds(processButton.getBounds().translated(-200, 0));

        // place the save button to the right of the cancel button
        saveButton.setBounds(cancelButton.getBounds().translated(110, 0));

        auto controls = mainArea.removeFromBottom (90);

        auto controlRightBounds = controls.removeFromRight (controls.getWidth() / 3);

       #if (JUCE_ANDROID || JUCE_IOS)
        chooseFileButton.setBounds (controlRightBounds.reduced (10));
       #else
        chooseFileButton.setBounds (controlRightBounds.reduced (10));
       #endif

        auto zoom = controls.removeFromTop (25);
        zoomLabel .setBounds (zoom.removeFromLeft (50));
        zoomSlider.setBounds (zoom);

        followTransportButton.setBounds (controls.removeFromTop (25));
        startStopButton      .setBounds (controls);

        mainArea.removeFromBottom (6);

       #if JUCE_ANDROID || JUCE_IOS
        thumbnail->setBounds (mainArea);
       #else
        thumbnail->setBounds (mainArea.removeFromBottom (140));
        mainArea.removeFromBottom (6);

       #endif
    }

    void resetUI(){
        ctrlComponent.resetUI();
        // Also clear the model card components
        ModelCard empty;
        setModelCard(empty);
    }

    void setModelCard(const ModelCard& card) {
        // Set the text for the labels
        nameLabel.setText(String(card.name), dontSendNotification);
        descriptionLabel.setText(String(card.description), dontSendNotification);
        // set the author label text to "by {author}" only if {author} isn't empty
        card.author.empty() ?
            authorLabel.setText("", dontSendNotification) :
            authorLabel.setText("by " + String(card.author), dontSendNotification);
    }


private:
    // HARP UI 
    std::unique_ptr<ModelStatusTimer> mModelStatusTimer {nullptr};

    TextEditor modelPathTextBox;
    TextButton loadModelButton;
    TextButton saveChangesButton {"save changes"};
    Label glossaryLabel;
    HyperlinkButton glossaryButton;
    TextButton processButton;
    TextButton cancelButton;
    TextButton saveButton;
    Label statusLabel;

    CtrlComponent ctrlComponent;

    // model card
    Label nameLabel, authorLabel, descriptionLabel, tagsLabel;
    HyperlinkButton spaceUrlButton;

    // the model itself
    std::shared_ptr<WebWave2Wave> model {new WebWave2Wave()};

    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
    #ifndef JUCE_DEMO_RUNNER
    AudioDeviceManager audioDeviceManager;
    #else
    AudioDeviceManager& audioDeviceManager { getSharedAudioDeviceManager (0, 2) };
    #endif


    std::unique_ptr<FileChooser> fileChooser;

    AudioFormatManager formatManager;
    TimeSliceThread thread  { "audio file preview" };

   #if (JUCE_ANDROID || JUCE_IOS)
    std::unique_ptr<FileChooser> fileChooser;
    TextButton chooseFileButton {"Choose Audio File...", "Choose an audio file for playback"};
   #else
    TextButton chooseFileButton {"Load File", "Load an audio file for playback"}; // Changed for desktop
   #endif

    URL currentAudioFile;
    URL currentAudioFileTarget;
    AudioSourcePlayer audioSourcePlayer;
    AudioTransportSource transportSource;
    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;

    std::unique_ptr<DemoThumbnailComp> thumbnail;
    Label zoomLabel                     { {}, "zoom:" };
    Slider zoomSlider                   { Slider::LinearHorizontal, Slider::NoTextBox };
    ToggleButton followTransportButton  { "Follow Transport" };
    TextButton startStopButton          { "Play/Stop" };


    /// CustomThreadPoolJob
    // This one is used for Loading the models
    // The thread pull for Processing lives inside the JobProcessorThread
    ThreadPool threadPool {1};
    int jobsFinished;
    int totalJobs;
    JobProcessorThread jobProcessorThread;
    std::vector<CustomThreadPoolJob*> customJobs;
    // ChangeBroadcaster processBroadcaster;
    ChangeBroadcaster loadBroadcaster;
    ChangeBroadcaster processBroadcaster;

    //==============================================================================
    void showAudioResource (URL resource)
    {
        if (! loadURLIntoTransport (currentAudioFile))
        {
            // Failed to load the audio file!
            jassertfalse;
            return;
        }

        zoomSlider.setValue (0, dontSendNotification);
        thumbnail->setURL (currentAudioFile);
    }

    void addNewAudioFile (URL resource) 
    {
        currentAudioFileTarget = resource;
        
        currentAudioFile = URL(File(
            File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory).getFullPathName() + "/HARP/"
            + currentAudioFileTarget.getLocalFile().getFileNameWithoutExtension() 
            + "_harp" + currentAudioFileTarget.getLocalFile().getFileExtension()
        ));

        currentAudioFile.getLocalFile().getParentDirectory().createDirectory();
        if (!currentAudioFileTarget.getLocalFile().copyFileTo(currentAudioFile.getLocalFile())) {
            DBG("MainComponent::addNewAudioFile: failed to copy file to " << currentAudioFile.getLocalFile().getFullPathName());
            // show an error to the user, we cannot proceed!
            AlertWindow("Error", "Failed to make a copy of the input file for processing!! are you out of space?", AlertWindow::WarningIcon);
        }
        DBG("MainComponent::addNewAudioFile: copied file to " << currentAudioFileTarget.getLocalFile().getFullPathName());

        showAudioResource(currentAudioFile);
    }

    bool loadURLIntoTransport (const URL& audioURL)
    {
        // unload the previous file source and delete it..
        transportSource.stop();
        transportSource.setSource (nullptr);
        currentAudioFileSource.reset();

        const auto source = makeInputSource (audioURL);

        if (source == nullptr)
            return false;

        auto stream = rawToUniquePtr (source->createInputStream());

        if (stream == nullptr)
            return false;

        auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));

        if (reader == nullptr)
            return false;

        currentAudioFileSource = std::make_unique<AudioFormatReaderSource> (reader.release(), true);

        // ..and plug it into our transport source
        transportSource.setSource (currentAudioFileSource.get(),
                                   32768,                   // tells it to buffer this many samples ahead
                                   &thread,                 // this is the background thread to use for reading-ahead
                                   currentAudioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

        return true;
    }

    void startOrStop()
    {
        if (transportSource.isPlaying())
        {
            transportSource.stop();
        }
        else
        {
            transportSource.setPosition (0);
            transportSource.start();
        }
    }

    void updateFollowTransportState()
    {
        thumbnail->setFollowsTransport (followTransportButton.getToggleState());
    }

   #if (JUCE_ANDROID || JUCE_IOS)
    void buttonClicked (Button* btn) override
    {
        if (btn == &chooseFileButton && fileChooser.get() == nullptr)
        {
            if (! RuntimePermissions::isGranted (RuntimePermissions::readExternalStorage))
            {
                SafePointer<MainComponent> safeThis (this);
                RuntimePermissions::request (RuntimePermissions::readExternalStorage,
                                             [safeThis] (bool granted) mutable
                                             {
                                                 if (safeThis != nullptr && granted)
                                                     safeThis->buttonClicked (&safeThis->chooseFileButton);
                                             });
                return;
            }

            if (FileChooser::isPlatformDialogAvailable())
            {
                fileChooser = std::make_unique<FileChooser> ("Select an audio file...", File(), "*.wav;*.flac;*.aif");

                fileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                          [this] (const FileChooser& fc) mutable
                                          {
                                              if (fc.getURLResults().size() > 0)
                                              {
                                                  auto u = fc.getURLResult();

                                                  addNewAudioFile (std::move (u));
                                              }

                                              fileChooser = nullptr;
                                          }, nullptr);
            }
            else
            {
                NativeMessageBox::showAsync (MessageBoxOptions()
                                               .withIconType (MessageBoxIconType::WarningIcon)
                                               .withTitle ("Enable Code Signing")
                                               .withMessage ("You need to enable code-signing for your iOS project and enable \"iCloud Documents\" "
                                                             "permissions to be able to open audio files on your iDevice. See: "
                                                             "https://forum.juce.com/t/native-ios-android-file-choosers"),
                                             nullptr);
            }
        }
    }
   #else
   #endif

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == thumbnail.get())
            addNewAudioFile (URL (thumbnail->getLastDroppedFile()));
        else if (source == &loadBroadcaster) {
            DBG("Setting up model card, CtrlComponent, resizing.");
            setModelCard(model->card());
            ctrlComponent.setModel(model);
            ctrlComponent.populateGui();
            repaint();

            // now, we can enable the buttons
            processButton.setEnabled(true);
            loadModelButton.setEnabled(true);
            loadModelButton.setButtonText("load");

            // Set the focus to the process button
            // so that the user can press SPACE to trigger the playback
            processButton.grabKeyboardFocus();
        }
        else if (source == &processBroadcaster) {
            // refresh the display for the new updated file
            showAudioResource(currentAudioFile);

            // now, we can enable the process button
            processButton.setButtonText("process");
            processButton.setEnabled(true);
            cancelButton.setEnabled(false);
            saveButton.setEnabled(true); 
            repaint();
        }
        else if (source == mModelStatusTimer.get()) {
            // update the status label
            DBG("HARPProcessorEditor::changeListenerCallback: updating status label");
            statusLabel.setText(model->getStatus(), dontSendNotification);
        }
        else {
            DBG("HARPProcessorEditor::changeListenerCallback: unhandled change broadcaster");
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
