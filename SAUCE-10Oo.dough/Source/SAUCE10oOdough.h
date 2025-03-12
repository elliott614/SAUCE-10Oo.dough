// Count newlines - MSVC-compatible version without GCC/Clang intrinsics
static int countNewlines(const juce::String& str)
{
    int count = 0;
    const char* data = str.toRawUTF8();
    const int length = str.length();

    // Process 4 bytes at a time for better cache usage and instruction parallelism
    const int vectorEnd = length - (length % 4);

    for (int i = 0; i < vectorEnd; i += 4)
    {
        // Process 4 bytes in parallel with fewer branches
        count += (data[i] == '\n');
        count += (data[i + 1] == '\n');
        count += (data[i + 2] == '\n');
        count += (data[i + 3] == '\n');
    }

    // Process remaining bytes
    for (int i = vectorEnd; i < length; ++i)
    {
        count += (data[i] == '\n');
    }

    return count;
}/*
==============================================================================

 This file is part of the JUCE tutorials.
 Copyright (c) 2020 - Raw Material Software Limited

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

 name:             SAUCE10oOdough
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Sostenuto pedal simulator (real-time)

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once
#include <JuceHeader.h>
#include "PedalButton.h"

//==============================================================================
class MainContentComponent : public juce::Component,
    private juce::MidiInputCallback,
    private juce::MidiKeyboardStateListener,
    private juce::AsyncUpdater
{
public:
    // Simple structure to hold log entries
    struct LogEntry {
        juce::MidiMessage message;
        juce::String source;
        double timestamp;
    };

    // Timer class to handle log updates at a consistent rate
    class LogTimer : public juce::Timer
    {
    public:
        LogTimer(MainContentComponent* owner) : owner(owner) {}
        void timerCallback() override { owner->processLogEntries(); }
    private:
        MainContentComponent* owner;
    };

    // Job class for background MIDI processing
    class MidiProcessingJob : public juce::ThreadPoolJob
    {
    public:
        MidiProcessingJob(MainContentComponent& owner, const juce::MidiMessage& msg)
            : ThreadPoolJob("MIDI Processing"),
            parent(owner),
            message(msg)
        {
        }

        JobStatus runJob() override
        {
            // Only process non-critical messages here
            if (!message.isNoteOnOrOff() &&
                !message.isSostenutoPedalOn() &&
                !message.isSostenutoPedalOff())
            {
                if (parent.midiOutput != nullptr)
                    parent.midiOutput->sendMessageNow(message);
            }
            return JobStatus::jobHasFinished;
        }

    private:
        MainContentComponent& parent;
        juce::MidiMessage message;
    };

    MainContentComponent()
        : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
        startTime(juce::Time::getMillisecondCounterHiRes() * 0.001),
        sostenutoPedalButton("\n\nSAUCE\n\n10\n\noO\n\ndough\n\n")
    {
        setOpaque(true);

        // Setup MIDI input components
        addAndMakeVisible(midiInputListLabel);
        midiInputListLabel.setText("MIDI Input:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent(&midiInputList, true);

        addAndMakeVisible(midiInputList);
        midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Enabled");

        auto midiInputs = juce::MidiInput::getAvailableDevices();
        juce::StringArray midiInputNames;
        for (auto input : midiInputs)
            midiInputNames.add(input.name);

        midiInputList.addItemList(midiInputNames, 1);
        midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedItemIndex()); };

        // Find the first enabled device and use that by default
        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled(input.identifier))
            {
                setMidiInput(midiInputs.indexOf(input));
                break;
            }
        }

        // If no enabled devices were found just use the first one in the list
        if (midiInputList.getSelectedId() == 0)
            setMidiInput(0);

        // Setup keyboard component
        addAndMakeVisible(keyboardComponent);
        keyboardState.addListener(this);

        // Setup MIDI message display box
        addAndMakeVisible(midiMessagesBox);
        midiMessagesBox.setMultiLine(true);
        midiMessagesBox.setReturnKeyStartsNewLine(true);
        midiMessagesBox.setReadOnly(true);
        midiMessagesBox.setScrollbarsShown(true);
        midiMessagesBox.setCaretVisible(false);
        midiMessagesBox.setPopupMenuEnabled(true);
        midiMessagesBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x32ffffff));
        midiMessagesBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x1c000000));
        midiMessagesBox.setColour(juce::TextEditor::shadowColourId, juce::Colour(0x16000000));
        StringArray fbf;
        fbf.insert(0, Font::getDefaultMonospacedFontName());
        Font fo(FontOptions("Consolas", 15, Font::plain));
        fo.setPreferredFallbackFamilies(fbf);
        midiMessagesBox.setFont(fo);

        midiMessagesBox.insertTextAtCaret("          ||#|#|||#|#|#|||#|#||\n          ||w|e|||t|y|u|||o|p||\n          |aTsTd|fTgThTj|kTlT;|\n          |_|_|_|_|_|_|_|_|_|_|\nwill play keys on one type of keyboard with the other\n");

        // Setup logging toggle button
        addAndMakeVisible(loggingEnabledButton);
        loggingEnabledButton.setButtonText("Enable MIDI Logging");
        loggingEnabledButton.setToggleState(false, juce::dontSendNotification);
        loggingEnabledButton.onClick = [this] {
            loggingEnabled = loggingEnabledButton.getToggleState();
            if (!loggingEnabled)
            {
                midiMessagesBox.clear();
                currentLogLines = 0;
            }
        };

        // Setup sostenuto pedal button
        addAndMakeVisible(sostenutoPedalButton);
        sostenutoPedalButton.onClick = [this] { handleSostenutoPedalButton(); };

        // Setup MIDI output components
        auto midiOutputs = juce::MidiOutput::getAvailableDevices();

        addAndMakeVisible(midiOutputListLabel);
        midiOutputListLabel.setText("MIDI Output:", juce::dontSendNotification);
        midiOutputListLabel.attachToComponent(&midiOutputList, true);

        addAndMakeVisible(midiOutputList);
        midiOutputList.setTextWhenNoChoicesAvailable("No MIDI Outputs Available");

        juce::StringArray midiOutputNames;
        for (auto& output : midiOutputs)
            midiOutputNames.add(output.name);

        midiOutputList.addItemList(midiOutputNames, 1);
        midiOutputList.onChange = [this] { setMidiOutput(midiOutputList.getSelectedItemIndex()); };

        // Find and select the first available output device
        setMidiOutput(0);

        // Setup thread pool for background processing
        configureThreadPool();

        // Setup log timer with configurable refresh rate
        logTimer = std::make_unique<LogTimer>(this);
        logTimer->startTimerHz(LOG_TIMER_FREQUENCY);

        setSize(600, 400);
        keyboardComponent.grabKeyboardFocus();
    }

    ~MainContentComponent() override
    {
        logTimer->stopTimer();
        midiThreadPool->removeAllJobs(true, 2000);
        keyboardState.removeListener(this);

        if (lastInputIndex >= 0 && lastInputIndex < juce::MidiInput::getAvailableDevices().size())
            deviceManager.removeMidiInputDeviceCallback(
                juce::MidiInput::getAvailableDevices()[lastInputIndex].identifier, this);

        if (midiOutput)
            midiOutput->stopBackgroundThread();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        keyboardComponent.grabKeyboardFocus();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Position both MIDI input and output lists at the top
        auto topArea = area.removeFromTop(36);
        const int labelWidth = 100;
        const int comboBoxWidth = (getWidth() - labelWidth * 2) / 2 - 16;

        // Position input and output lists
        midiInputList.setBounds(labelWidth, 8, comboBoxWidth, 24);
        midiOutputList.setBounds(labelWidth + comboBoxWidth + labelWidth, 8, comboBoxWidth, 24);

        // Define sizes for other controls
        const int checkboxWidth = 150;
        const int checkboxHeight = 24;
        const int pedalWidth = 50;
        const int pedalHeight = 100;
        const int pedalMargin = 10;

        // Position the keyboard
        auto keyboardArea = area.removeFromTop(80);
        keyboardComponent.setBounds(keyboardArea.reduced(8));

        // Remove space for the bottom controls
        auto bottomArea = area.removeFromBottom(pedalHeight + pedalMargin);

        // Position the message box
        midiMessagesBox.setBounds(area.reduced(8));

        // Position the pedal button and checkbox
        const int pedalX = (getWidth() - pedalWidth - checkboxWidth - 20) / 2;
        const int pedalY = getHeight() - pedalHeight - pedalMargin;
        sostenutoPedalButton.setBounds(pedalX, pedalY, pedalWidth, pedalHeight);
        loggingEnabledButton.setBounds(pedalX + pedalWidth + 20, pedalY + (pedalHeight - checkboxHeight) / 2,
            checkboxWidth, checkboxHeight);
        sostenutoPedalButton.setMouseClickGrabsKeyboardFocus(false);
        loggingEnabledButton.setMouseClickGrabsKeyboardFocus(false);
        midiMessagesBox.setMouseClickGrabsKeyboardFocus(false);
        keyboardComponent.grabKeyboardFocus();
    }

    // Process incoming MIDI messages
    void processMessageOnThread(const juce::MidiMessage& message)
    {
        const juce::ScopedLock sl(midiProcessLock);

        // Handle time-critical messages immediately
        if (message.isNoteOnOrOff() || (message.isController() && message.getControllerNumber() == 66))
        {
            processMidiRealTime(message);
        }
        else
        {
            // For non-time-critical messages, add to buffer for batch processing
            midiMessageBuffer.addEvent(message, 0);
        }
    }

    // Process and display log entries
    void processLogEntries()
    {
        if (!loggingEnabled)
            return;

        const int numReady = logFifo.getNumReady();
        if (numReady == 0)
            return;

        // Build text off the message thread
        juce::String textToAdd;
        int start1, size1, start2, size2;
        logFifo.prepareToRead(numReady, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            textToAdd += formatLogEntry(logEntries[start1 + i]);

        for (int i = 0; i < size2; ++i)
            textToAdd += formatLogEntry(logEntries[start2 + i]);

        logFifo.finishedRead(size1 + size2);

        // Single update to text editor
        if (textToAdd.isNotEmpty())
        {
            juce::MessageManager::callAsync([this, text = std::move(textToAdd)]() mutable
            {
                midiMessagesBox.moveCaretToEnd();
                midiMessagesBox.insertTextAtCaret(text);

                // Count newlines using direct access to the string data with getCharPointer()
                auto charPtr = text.getCharPointer();
                int newlineCount = 0;

                while (!charPtr.isEmpty())
                {
                    if (*charPtr == '\n')
                        newlineCount++;
                    ++charPtr;
                }

                currentLogLines += newlineCount;
                trimLogIfNeeded();
            });
        }
    }

private:
    // Optimized for branchless comparison of MIDI message types
    static juce::String getMidiMessageDescription(const juce::MidiMessage& m)
    {
        juce::String result;

        // Pre-check properties to avoid multiple evaluations
        const bool isNoteOn = m.isNoteOn();
        const bool isNoteOff = m.isNoteOff();
        const bool isProgramChange = m.isProgramChange();
        const bool isPitchWheel = m.isPitchWheel();
        const bool isAftertouch = m.isAftertouch();
        const bool isChannelPressure = m.isChannelPressure();
        const bool isAllNotesOff = m.isAllNotesOff();
        const bool isAllSoundOff = m.isAllSoundOff();
        const bool isMetaEvent = m.isMetaEvent();
        const bool isController = m.isController();

        // Note handling with common shared code
        if (isNoteOn || isNoteOff)
        {
            result = (isNoteOn ? "Note on " : "Note off ") +
                juce::MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);

            return result;
        }

        // Program change
        if (isProgramChange)
            return "Program change " + juce::String(m.getProgramChangeNumber());

        // Pitch wheel
        if (isPitchWheel)
            return "Pitch wheel " + juce::String(m.getPitchWheelValue());

        // Aftertouch
        if (isAftertouch)
            return "After touch " + juce::MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3) + ": " + juce::String(m.getAfterTouchValue());

        // Channel pressure
        if (isChannelPressure)
            return "Channel pressure " + juce::String(m.getChannelPressureValue());

        // All notes off
        if (isAllNotesOff)
            return "All notes off";

        // All sound off
        if (isAllSoundOff)
            return "All sound off";

        // Meta event
        if (isMetaEvent)
            return "Meta event";

        // Controller
        if (isController)
        {
            juce::String name(juce::MidiMessage::getControllerName(m.getControllerNumber()));

            // Branchless empty string check using ternary
            name = name.isEmpty() ? "[" + juce::String(m.getControllerNumber()) + "]" : name;

            return "Controller " + name + ": " + juce::String(m.getControllerValue());
        }

        // Default - raw MIDI data as hex
        return juce::String::toHexString(m.getRawData(), m.getRawDataSize());
    }

    // Configure the thread pool for MIDI processing
    void configureThreadPool()
    {
        int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0)
            numThreads = 2; // Reasonable default

        juce::ThreadPool::Options options;
        options = options.withThreadName("MIDI Processing")
            .withNumberOfThreads(numThreads);

        midiThreadPool = std::make_unique<juce::ThreadPool>(options);
    }

    // Process MIDI messages
    void processMidiMessage(const juce::MidiMessage& message)
    {
        // Handle timing-critical messages first
        if (message.isNoteOnOrOff() || (message.isController() && message.getControllerNumber() == 66))
        {
            processMidiRealTime(message);
        }
        else
        {
            // Add non-critical messages to processing queue
            auto* job = new MidiProcessingJob(*this, message);
            midiThreadPool->addJob(job, true);
        }

        // Add to log if enabled (separate path)
        if (loggingEnabled)
        {
            const juce::ScopedLock sl(logMutex);
            int start1, size1, start2, size2;
            logFifo.prepareToWrite(1, start1, size1, start2, size2);

            if (size1 + size2 > 0)
            {
                const int writeIndex = size1 == 1 ? start1 : start2;
                logEntries[writeIndex] = { message, "MIDI", message.getTimeStamp() };
                logFifo.finishedWrite(1);
            }
        }
    }

    // Process real-time MIDI messages - MSVC optimized
    void processMidiRealTime(const juce::MidiMessage& message)
    {
        if (message.isNoteOnOrOff())
        {
            // Always update keyboard state
            keyboardState.processNextMidiEvent(message);

            // For note-offs, check if held by sostenuto
            if (message.isNoteOff())
            {
                // Skip sending if held by sostenuto
                if (isSostenutoPedalHeldNote(message.getNoteNumber()))
                    return;
            }

            // Send the note message
            if (midiOutput != nullptr)
            {
                midiOutput->sendMessageNow(message);
            }
        }
        else // Must be sostenuto pedal message
        {
            const bool pedalDown = message.isSostenutoPedalOn();
            const bool wasDown = sostenutoPedalButton.getToggleState();

            // Handle pedal state change
            if (pedalDown && !wasDown) // Pedal pressed
            {
                // Capture all currently held notes
                for (int note = 0; note < 128; ++note)
                {
                    if (keyboardState.isNoteOn(1, note))
                    {
                        setSostenutoPedalHeldNote(note);
                    }
                }
            }
            else if (!pedalDown && wasDown) // Pedal released
            {
                handlePedalRelease(message.getTimeStamp());
            }

            // Update pedal button state
            sostenutoPedalButton.handleCC66(message.getControllerValue());
        }
    }

    // Process batched MIDI messages
    void processBatchedMessages()
    {
        const juce::ScopedLock sl(midiProcessLock);

        juce::MidiBuffer::Iterator it(midiMessageBuffer);
        juce::MidiMessage message;
        int samplePosition;

        while (it.getNextEvent(message, samplePosition))
        {
            // Process non-time-critical messages
            if (!message.isNoteOnOrOff() &&
                !message.isSostenutoPedalOn() &&
                !message.isSostenutoPedalOff())
            {
                if (midiOutput)
                    midiOutput->sendMessageNow(message);
            }
        }

        midiMessageBuffer.clear();
    }

    // Format a log entry - branchless optimization for string formatting
    juce::String formatLogEntry(const LogEntry& entry) const
    {
        auto time = entry.timestamp - startTime;

        // Use integer division and modulo for time components
        const int hours = static_cast<int>(time / 3600.0) % 24;
        const int minutes = static_cast<int>(time / 60.0) % 60;
        const int seconds = static_cast<int>(time) % 60;
        const int millis = static_cast<int>(time * 1000.0) % 1000;

        // Pre-allocate the string to avoid reallocations
        juce::String result;
        result.preallocateBytes(100); // Average log line length

        // Format time with fixed width fields
        result
            << juce::String::formatted("%02d:%02d:%02d.%03d", hours, minutes, seconds, millis)
            << "  -  "
            << getMidiMessageDescription(entry.message)
            << " (" << entry.source << ")\n";

        return result;
    }

    // Trim log to keep it within size limits - simplified for better performance
    void trimLogIfNeeded()
    {
        // Only trim if we're over the limit
        const int excess = currentLogLines - MAX_LOG_LINES;
        if (excess <= 0)
            return;

        // Optimization: If we're way over the limit, just clear and start fresh
        if (excess > MAX_LOG_LINES / 2)
        {
            midiMessagesBox.clear();
            currentLogLines = 0;
            return;
        }

        // Otherwise do a targeted trim of exactly the lines we need to remove
        juce::String text = midiMessagesBox.getText();

        // Find the position after the last line we need to remove
        int pos = 0;
        for (int i = 0; i < excess && pos < text.length(); ++i)
        {
            int nextPos = text.indexOfChar(pos, '\n');
            if (nextPos == -1)
                break;
            pos = nextPos + 1; // Move past the newline
        }

        // Only modify the text if we found a valid position
        if (pos > 0 && pos < text.length())
        {
            // Replace text in one operation
            midiMessagesBox.setHighlightedRegion({ 0, pos });
            midiMessagesBox.insertTextAtCaret("");
            currentLogLines -= excess;
        }
    }

    // Set up MIDI input device
    void setMidiInput(int index)
    {
        auto list = juce::MidiInput::getAvailableDevices();

        if (lastInputIndex >= 0 && lastInputIndex < list.size())
            deviceManager.removeMidiInputDeviceCallback(list[lastInputIndex].identifier, this);

        auto newInput = list[index];

        if (!deviceManager.isMidiInputDeviceEnabled(newInput.identifier))
            deviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);

        deviceManager.addMidiInputDeviceCallback(newInput.identifier, this);
        midiInputList.setSelectedId(index + 1, juce::dontSendNotification);

        lastInputIndex = index;
    }

    // Set up MIDI output device
    void setMidiOutput(int index)
    {
        auto list = juce::MidiOutput::getAvailableDevices();

        if (index >= 0 && index < list.size())
        {
            midiOutput = nullptr; // Reset any existing output

            midiOutput = juce::MidiOutput::openDevice(list[index].identifier);

            if (midiOutput != nullptr)
                midiOutput->startBackgroundThread();

            midiOutputList.setSelectedId(index + 1, juce::dontSendNotification);
        }
    }

    // Handle async updates - simplified to reduce branching
    void handleAsyncUpdate() override
    {
        // Process any batched messages first (direct access, no thread synchronization needed)
        const juce::ScopedLock sl(midiProcessLock);

        if (!midiMessageBuffer.isEmpty() && midiOutput != nullptr)
        {
            // Simple iteration without conditional branches inside the loop
            juce::MidiBuffer::Iterator it(midiMessageBuffer);
            juce::MidiMessage message;
            int samplePosition;

            while (it.getNextEvent(message, samplePosition))
            {
                // Only send if this is not a time-critical message
                // (time critical messages are sent directly in processMidiRealTime)
                const bool isTimeCritical = message.isNoteOnOrOff() ||
                    message.isSostenutoPedalOn() ||
                    message.isSostenutoPedalOff();

                if (!isTimeCritical)
                    midiOutput->sendMessageNow(message);
            }

            // Clear buffer once done
            midiMessageBuffer.clear();
        }
    }

    // Handle sostenuto pedal button click
    void handleSostenutoPedalButton()
    {
        bool isDown = sostenutoPedalButton.getToggleState();
        juce::MidiMessage message = juce::MidiMessage::controllerEvent(1, 66, isDown ? 127 : 0);
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);

        if (isDown) // Pedal just pressed
        {
            // Capture currently held notes
            for (int note = 0; note < 128; ++note)
            {
                if (keyboardState.isNoteOn(1, note))
                    setSostenutoPedalHeldNote(note);
            }
        }
        else // Pedal just released
        {
            handlePedalRelease(message.getTimeStamp());
        }

        // Send CC message
        if (midiOutput)
            midiOutput->sendMessageNow(message);

        // Log the action
        if (loggingEnabled)
        {
            const juce::ScopedLock sl(logMutex);
            int start1, size1, start2, size2;
            logFifo.prepareToWrite(1, start1, size1, start2, size2);

            if (size1 + size2 > 0)
            {
                const int writeIndex = size1 == 1 ? start1 : start2;
                logEntries[writeIndex] = { message, "Pedal Button", message.getTimeStamp() };
                logFifo.finishedWrite(1);
            }
        }
    }

    // MidiInputCallback implementation - direct processing with minimal branching
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        // Process message directly - no FIFO needed for such a simple operation
        isAddingFromMidiInput = true;

        // High-priority path: For time-critical messages, process immediately
        if (message.isNoteOnOrOff() || message.isSostenutoPedalOn() || message.isSostenutoPedalOff())
        {
            processMidiRealTime(message);
        }
        else
        {
            // Low-priority path: For other messages, add to buffer for batch processing
            const juce::ScopedLock sl(midiProcessLock);
            midiMessageBuffer.addEvent(message, 0);
            triggerAsyncUpdate();
        }

        // Add to logging system if enabled (non-blocking)
        if (loggingEnabled.load(std::memory_order_relaxed))
        {
            // Try to add to log FIFO - if full, simply drop the message
            int start1, size1, start2, size2;
            logFifo.prepareToWrite(1, start1, size1, start2, size2);

            if (size1 + size2 > 0)
            {
                const int writeIndex = size1 == 1 ? start1 : start2;
                logEntries[writeIndex] = { message, source->getName() + " (Input)", message.getTimeStamp() };
                logFifo.finishedWrite(1);
            }
        }

        isAddingFromMidiInput = false;
    }

    // MidiKeyboardStateListener implementation for note on
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (!isAddingFromMidiInput)
        {
            auto m = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
            m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);

            // Send MIDI message
            if (midiOutput)
                midiOutput->sendMessageNow(m);

            // Add to log if enabled
            if (loggingEnabled)
            {
                const juce::ScopedLock sl(logMutex);
                int start1, size1, start2, size2;
                logFifo.prepareToWrite(1, start1, size1, start2, size2);

                if (size1 + size2 > 0)
                {
                    const int writeIndex = size1 == 1 ? start1 : start2;
                    logEntries[writeIndex] = { m, "On-Screen Keyboard", m.getTimeStamp() };
                    logFifo.finishedWrite(1);
                }
            }
        }
    }

    // MidiKeyboardStateListener implementation for note off
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        if (!isAddingFromMidiInput)
        {
            auto m = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber);
            m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);

            // Skip if held by sostenuto
            if (isSostenutoPedalHeldNote(midiNoteNumber))
            {
                if (loggingEnabled)
                {
                    const juce::ScopedLock sl(logMutex);
                    int start1, size1, start2, size2;
                    logFifo.prepareToWrite(1, start1, size1, start2, size2);

                    if (size1 + size2 > 0)
                    {
                        const int writeIndex = size1 == 1 ? start1 : start2;
                        logEntries[writeIndex] = { m, "On-Screen Keyboard (Held by Sostenuto)", m.getTimeStamp() };
                        logFifo.finishedWrite(1);
                    }
                }
                return;
            }

            // Send note-off
            if (midiOutput)
                midiOutput->sendMessageNow(m);

            // Add to log if enabled
            if (loggingEnabled)
            {
                const juce::ScopedLock sl(logMutex);
                int start1, size1, start2, size2;
                logFifo.prepareToWrite(1, start1, size1, start2, size2);

                if (size1 + size2 > 0)
                {
                    const int writeIndex = size1 == 1 ? start1 : start2;
                    logEntries[writeIndex] = { m, "On-Screen Keyboard", m.getTimeStamp() };
                    logFifo.finishedWrite(1);
                }
            }
        }
    }

    // Sostenuto pedal note handling
    // Branchless sostenuto note setting
    constexpr inline void setSostenutoPedalHeldNote(int note)
    {
        // Only perform operation if note is in valid range, without branching
        const bool isValidNote = (note >= 0 && note < 128);
        const size_t index = note / 64;
        const size_t bit = note % 64;

        // This will have no effect if isValidNote is false (out of range)
        sostenutoPedalHeldNotesBitmap[index] |= isValidNote * (1ULL << bit);
    }

    // Branchless sostenuto note clearing
    constexpr inline void clearSostenutoPedalHeldNote(int note)
    {
        // Only perform operation if note is in valid range, without branching
        const bool isValidNote = (note >= 0 && note < 128);
        const size_t index = note / 64;
        const size_t bit = note % 64;

        // This will have no effect if isValidNote is false (out of range)
        sostenutoPedalHeldNotesBitmap[index] &= ~(isValidNote * (1ULL << bit));
    }

    // Branchless implementation of sostenuto note check
    constexpr inline bool isSostenutoPedalHeldNote(int note) const
    {
        // Use arithmetic to avoid branching
        // This evaluates to 0 if note is out of range, allowing the rest of the expression to be safe
        return (note >= 0 && note < 128) &&
            ((sostenutoPedalHeldNotesBitmap[note / 64] & (1ULL << (note % 64))) != 0);
    }

    void resetSostenutoPedalHeldNotes() {
        sostenutoPedalHeldNotesBitmap[0] = 0;
        sostenutoPedalHeldNotesBitmap[1] = 0;
    }

    // Handle pedal release - optimized for MSVC 
    void handlePedalRelease(double timeStamp)
    {
        // Cache this to avoid repeated atomic loads
        const bool shouldLog = loggingEnabled.load(std::memory_order_relaxed);

        // Process both bitmap segments
        for (size_t k = 0; k < 2; ++k)
        {
            uint64_t bitset = sostenutoPedalHeldNotesBitmap[k];

            // Process all set bits
            while (bitset != 0)
            {
                // Find and clear lowest set bit
                // MSVC-friendly bit manipulation
#if defined(_MSC_VER)
                unsigned long bitIndex;
                _BitScanForward64(&bitIndex, bitset);
                uint64_t mask = 1ULL << bitIndex;
                bitset &= ~mask;  // Clear the bit
                int note = k * 64 + static_cast<int>(bitIndex);
#else
                uint64_t t = bitset & -bitset;
                int r = countTrailingZeros(bitset);
                bitset ^= t;  // Clear the bit
                int note = k * 64 + r;
#endif

                // Only send note-off if the note isn't physically pressed
                if (!keyboardState.isNoteOn(1, note) && midiOutput != nullptr)
                {
                    auto noteOff = juce::MidiMessage::noteOff(1, note);
                    noteOff.setTimeStamp(timeStamp);
                    midiOutput->sendMessageNow(noteOff);

                    // Log if enabled (separate, non-blocking path)
                    if (shouldLog)
                    {
                        int start1, size1, start2, size2;
                        logFifo.prepareToWrite(1, start1, size1, start2, size2);

                        if (size1 + size2 > 0)
                        {
                            const int writeIndex = size1 == 1 ? start1 : start2;
                            logEntries[writeIndex] = { noteOff, "Sostenuto Release", noteOff.getTimeStamp() };
                            logFifo.finishedWrite(1);
                        }
                    }
                }
            }
        }

        // Reset bitmap after processing all notes
        sostenutoPedalHeldNotesBitmap[0] = 0;
        sostenutoPedalHeldNotesBitmap[1] = 0;
    }

    // Platform-independent trailing zero count
    inline int countTrailingZeros(uint64_t x) {
        if (x == 0) return 64;

#if defined(_MSC_VER)
        // MSVC implementation
        unsigned long index;
        _BitScanForward64(&index, x);
        return index;
#elif defined(__GNUC__) || defined(__clang__)
        // GCC/Clang implementation
        return __builtin_ctzll(x);
#else
        // Fallback implementation
        int count = 0;
        while ((x & 1) == 0) {
            x >>= 1;
            count++;
        }
        return count;
#endif
    }

    //==============================================================================
    // Constants and member variables
    static constexpr size_t bitmapSize = 2; // 2 uint64_t for 128 MIDI notes
    static constexpr int MAX_LOG_LINES = 500; // Maximum number of lines to keep in the log
    static constexpr int LOG_TIMER_FREQUENCY = 30; // Hz
    static constexpr size_t LOG_CHUNK_SIZE = 16; // Process logs in chunks of this size

    double startTime = 0;
    std::atomic<bool> loggingEnabled{ false }; // Start with logging disabled
    std::atomic<bool> isAddingFromMidiInput{ false };
    int currentLogLines = 0;
    int lastInputIndex = 0;

    // Audio and MIDI handling
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::MidiOutput> midiOutput = nullptr;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiMessageBuffer;

    // Thread-safe data structures
    std::unique_ptr<juce::ThreadPool> midiThreadPool;
    juce::AbstractFifo midiFifo{ 256 }; // Smaller size for better performance
    std::array<juce::MidiMessage, 256> midiMessageArray;
    juce::CriticalSection midiProcessLock;

    // Logging components
    juce::AbstractFifo logFifo{ 512 }; // Smaller buffer for better performance
    std::vector<LogEntry> logEntries{ 512 };
    std::unique_ptr<LogTimer> logTimer;
    juce::CriticalSection logMutex;

    // UI Components
    juce::ComboBox midiInputList;
    juce::Label midiInputListLabel;
    juce::ComboBox midiOutputList;
    juce::Label midiOutputListLabel;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::TextEditor midiMessagesBox;
    PedalButton sostenutoPedalButton;
    juce::ToggleButton loggingEnabledButton;

    // Sostenuto pedal state
    uint64_t sostenutoPedalHeldNotesBitmap[2] = { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};