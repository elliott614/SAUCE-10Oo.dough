/*
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
#include <bitset>
//==============================================================================
class MainContentComponent : public juce::Component,
    private juce::MidiInputCallback,
    private juce::MidiKeyboardStateListener
{
public:
    MainContentComponent()
        : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
        startTime(juce::Time::getMillisecondCounterHiRes() * 0.001),
        sostenutoPedalButton("\n\nSAUCE\n\n10\n\noO\n\ndough\n\n")
    {

        setOpaque(true);
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

        // find the first enabled device and use that by default
        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled(input.identifier))
            {
                setMidiInput(midiInputs.indexOf(input));
                break;
            }
        }

        // if no enabled devices were found just use the first one in the list
        if (midiInputList.getSelectedId() == 0)
            setMidiInput(0);

        addAndMakeVisible(keyboardComponent);
        keyboardState.addListener(this);

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

        addAndMakeVisible(loggingEnabledButton);
        loggingEnabledButton.setButtonText("Enable MIDI Logging");
        loggingEnabledButton.setToggleState(false, juce::dontSendNotification);
        loggingEnabledButton.onClick = [this] {
            loggingEnabled = loggingEnabledButton.getToggleState();
            if (!loggingEnabled)
            {
                // Clear the log when logging is disabled
                midiMessagesBox.clear();
                currentLogLines = 0;
            }
        };

        addAndMakeVisible(sostenutoPedalButton);
        sostenutoPedalButton.onClick = [this] { handleSostenutoPedalButton(); };

        setSize(600, 400);

        // Output setup
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
    }

    ~MainContentComponent() override
    {
        keyboardState.removeListener(this);
        deviceManager.removeMidiInputDeviceCallback(juce::MidiInput::getAvailableDevices()[midiInputList.getSelectedItemIndex()].identifier, this);
        if (midiOutput)
            midiOutput->stopBackgroundThread();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Position both MIDI input and output lists at the top
        auto topArea = area.removeFromTop(36);
        const int labelWidth = 100;
        const int comboBoxWidth = (getWidth() - labelWidth * 2) / 2 - 16;

        // Position input list
        midiInputList.setBounds(labelWidth, 8, comboBoxWidth, 24);

        // Position output list
        midiOutputList.setBounds(labelWidth + comboBoxWidth + labelWidth, 8, comboBoxWidth, 24);

        // Add checkbox position - next to the pedal
        const int checkboxWidth = 150;
        const int checkboxHeight = 24;
        const int pedalWidth = 50;
        const int pedalHeight = 100;
        const int pedalMargin = 10;

        // Position the keyboard below the input/output lists
        auto keyboardArea = area.removeFromTop(80);
        keyboardComponent.setBounds(keyboardArea.reduced(8));  // Add this line back

        // Remove space for the bottom controls
        auto bottomArea = area.removeFromBottom(pedalHeight + pedalMargin);

        // Position the message box in the remaining space
        midiMessagesBox.setBounds(area.reduced(8));

        // Position the pedal button centered horizontally
        const int pedalX = (getWidth() - pedalWidth - checkboxWidth - 20) / 2;
        const int pedalY = getHeight() - pedalHeight - pedalMargin;
        sostenutoPedalButton.setBounds(pedalX, pedalY, pedalWidth, pedalHeight);

        // Position checkbox next to the pedal
        loggingEnabledButton.setBounds(pedalX + pedalWidth + 20, pedalY + (pedalHeight - checkboxHeight) / 2,
            checkboxWidth, checkboxHeight);
    }



private:
    static juce::String getMidiMessageDescription(const juce::MidiMessage& m)
    {
        if (m.isNoteOn())           return "Note on " + juce::MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);
        if (m.isNoteOff())          return "Note off " + juce::MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);
        if (m.isProgramChange())    return "Program change " + juce::String(m.getProgramChangeNumber());
        if (m.isPitchWheel())       return "Pitch wheel " + juce::String(m.getPitchWheelValue());
        if (m.isAftertouch())       return "After touch " + juce::MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3) + ": " + juce::String(m.getAfterTouchValue());
        if (m.isChannelPressure())  return "Channel pressure " + juce::String(m.getChannelPressureValue());
        if (m.isAllNotesOff())      return "All notes off";
        if (m.isAllSoundOff())      return "All sound off";
        if (m.isMetaEvent())        return "Meta event";

        if (m.isController())
        {
            juce::String name(juce::MidiMessage::getControllerName(m.getControllerNumber()));

            if (name.isEmpty())
                name = "[" + juce::String(m.getControllerNumber()) + "]";

            return "Controller " + name + ": " + juce::String(m.getControllerValue());
        }

        return juce::String::toHexString(m.getRawData(), m.getRawDataSize());
    }

    void logMessage(const juce::String& m)
    {
        if (!loggingEnabled)
            return;

        // Add new message to end
        midiMessagesBox.moveCaretToEnd();
        midiMessagesBox.insertTextAtCaret(m + juce::newLine);

        // Increment line counter
        currentLogLines++;

        // Check if we need to trim the log
        if (currentLogLines > MAX_LOG_LINES)
        {
            // Get current text
            auto text = midiMessagesBox.getText();

            // Find position of the (currentLogLines - MAX_LOG_LINES + 1)th newline
            // This approach removes multiple lines at once if needed
            int linesToRemove = currentLogLines - MAX_LOG_LINES;
            int currentPos = 0;
            int newlineCount = 0;

            // Find the position after the last newline we need to remove
            while (newlineCount < linesToRemove && currentPos < text.length())
            {
                if (text[currentPos] == '\n')
                    newlineCount++;

                currentPos++;
            }

            // If we found enough newlines, remove all text up to that position
            if (currentPos < text.length())
            {
                midiMessagesBox.setHighlightedRegion({ 0, currentPos });
                midiMessagesBox.insertTextAtCaret("");

                // Update line count
                currentLogLines -= linesToRemove;
            }
            else
            {
                // Fallback: clear everything if things got out of sync
                midiMessagesBox.clear();
                currentLogLines = 1; // Just the line we're about to add
                midiMessagesBox.insertTextAtCaret(m + juce::newLine);
            }
        }

        // Always ensure caret is at the end for auto-scrolling
        midiMessagesBox.moveCaretToEnd();
    }

    /** Starts listening to a MIDI input device, enabling it if necessary. */
    void setMidiInput(int index)
    {
        auto list = juce::MidiInput::getAvailableDevices();

        deviceManager.removeMidiInputDeviceCallback(list[lastInputIndex].identifier, this);

        auto newInput = list[index];

        if (!deviceManager.isMidiInputDeviceEnabled(newInput.identifier))
            deviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);

        deviceManager.addMidiInputDeviceCallback(newInput.identifier, this);
        midiInputList.setSelectedId(index + 1, juce::dontSendNotification);

        lastInputIndex = index;
    }

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

    void handleSostenutoPedalButton()
    {
        bool isDown = sostenutoPedalButton.getToggleState();
        juce::MidiMessage message = juce::MidiMessage::controllerEvent(1, 66, isDown ? 127 : 0);
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);

        // Since we know isDown is changing, we can immediately handle the pedal state here
        if (isDown) // Pedal just pressed
        {
            // Capture currently held notes
            for (int note = 0; note < 128; ++note)
            {
                if (keyboardState.isNoteOn(1, note))
                {
                    setSostenutoPedalHeldNote(note);
                }
            }
        }
        else // Pedal just released
        {
            // Handle pedal release (your existing code for this)
            handlePedalRelease();
        }

        // Log and send the CC message
        postMessageToList(message, "Pedal Button");
        if (midiOutput)
        {
            midiOutput->sendMessageNow(message);
        }
    }

    void handleMidiOutput(const juce::MidiMessage& message)
    {
        if (midiOutput != nullptr)
        {
            midiOutput->sendMessageNow(message);
            postMessageToList(message, midiOutput->getName());
        }
    }

    // These methods handle callbacks from the midi device + on-screen keyboard..
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        const juce::ScopedValueSetter<bool> scopedInputFlag(isAddingFromMidiInput, true);

        // Always log incoming message first
        postMessageToList(message, source->getName() + " (Input)");

        // Process the message as before
        if (message.isNoteOnOrOff())
        {
            keyboardState.processNextMidiEvent(message);

            // Only process note-offs if not held by sostenuto
            if (message.isNoteOff() && isSostenutoPedalHeldNote(message.getNoteNumber()))
                return;

            if (midiOutput)
            {
                midiOutput->sendMessageNow(message);
                postMessageToList(message, midiOutput->getName());
            }
            return;
        }
        else if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();

            // Check if this note is being held by sostenuto
            if (isSostenutoPedalHeldNote(noteNumber))
            {
                // Don't forward note-off if the note is being held by sostenuto
                keyboardState.processNextMidiEvent(message); // Update visual state
            }
            else
            {
                // Process normally if note isn't held by sostenuto
                keyboardState.processNextMidiEvent(message);
                if (midiOutput)
                {
                    midiOutput->sendMessageNow(message);
                    postMessageToList(message, midiOutput->getName());
                }
            }
        }
        else if (message.isController() && message.getControllerNumber() == 66)
        {
            // Handle sostenuto pedal change
            bool pedalDown = message.getControllerValue() >= 64; // Message will change pedal to down
            bool wasDown = sostenutoPedalButton.getToggleState(); // Pedal state marked down
            if (pedalDown && !wasDown)
            {
                // Pedal just pressed - capture currently held notes
                for (int note = 0; note < 128; ++note)
                {
                    if (keyboardState.isNoteOn(1, note))  // Check all notes on channel 1
                    {
                        setSostenutoPedalHeldNote(note);
                    }
                }
            }
            else if (!pedalDown && wasDown)
            {
				handlePedalRelease(); // Pedal just released
            }

            sostenutoPedalButton.handleCC66(message.getControllerValue());

        }
        else
        {
            // Process all other MIDI messages normally
            keyboardState.processNextMidiEvent(message);
            if (midiOutput)
            {
                midiOutput->sendMessageNow(message);
                postMessageToList(message, midiOutput->getName());
            }
        }

    }

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (!isAddingFromMidiInput)
        {
            auto m = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
            m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
            postMessageToList(m, "On-Screen Keyboard");
            if (midiOutput)
            {
                midiOutput->sendMessageNow(m);
                // Log the forwarded message to MIDI device
                postMessageToList(m, midiOutput->getName());
            }
        }
    }
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        if (!isAddingFromMidiInput)
        {
            // Only block note-off if specifically held by sostenuto
			if (isSostenutoPedalHeldNote(midiNoteNumber))
            {
                auto m = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber);
                m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
                postMessageToList(m, "On-Screen Keyboard (Held by Sostenuto)");
                // Don't send MIDI - note is held by sostenuto
            }
            else
            {
                auto m = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber);
                m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
                postMessageToList(m, "On-Screen Keyboard");

                if (midiOutput)
                {
                    midiOutput->sendMessageNow(m);
                    postMessageToList(m, midiOutput->getName());
                }
            }
        }
    }

    // This is used to dispach an incoming message to the message thread
    class IncomingMessageCallback : public juce::CallbackMessage
    {
    public:
        IncomingMessageCallback(MainContentComponent* o, const juce::MidiMessage& m, const juce::String& s)
            : owner(o), message(m), source(s)
        {
        }

        void messageCallback() override
        {
            if (owner != nullptr)
                owner->addMessageToList(message, source);
        }

        Component::SafePointer<MainContentComponent> owner;
        juce::MidiMessage message;
        juce::String source;
    };

    void postMessageToList(const juce::MidiMessage& message, const juce::String& source)
    {
        if (loggingEnabled)
            (new IncomingMessageCallback(this, message, source))->post();
    }

    void addMessageToList(const juce::MidiMessage& message, const juce::String& source)
    {
        auto time = message.getTimeStamp() - startTime;

        auto hours = ((int)(time / 3600.0)) % 24;
        auto minutes = ((int)(time / 60.0)) % 60;
        auto seconds = ((int)time) % 60;
        auto millis = ((int)(time * 1000.0)) % 1000;

        auto timecode = juce::String::formatted("%02d:%02d:%02d.%03d",
            hours,
            minutes,
            seconds,
            millis);

        auto description = getMidiMessageDescription(message);

        juce::String midiMessageString(timecode + "  -  " + description + " (" + source + ")"); // [7]
        logMessage(midiMessageString);
    }

    // Let's assume we have a bitmap representation for the sostenuto pedal held notes
    // Using 128-bit bitmap (2 uint64_t) for 128 MIDI notes (0-127)
    uint64_t sostenutoPedalHeldNotesBitmap[2] = { 0, 0 }; // Initialized to all zeros

    // Function to set a note in the bitmap
    constexpr inline void setSostenutoPedalHeldNote(int note) {
        if (note >= 0 && note < 128) {
            size_t index = note / 64;
            size_t bit = note % 64;
            sostenutoPedalHeldNotesBitmap[index] |= (1ULL << bit);
        }
    }

    // Function to clear a note from the bitmap
    constexpr inline void clearSostenutoPedalHeldNote(int note) {
        if (note >= 0 && note < 128) {
            size_t index = note / 64;
            size_t bit = note % 64;
            sostenutoPedalHeldNotesBitmap[index] &= ~(1ULL << bit);
        }
    }

    // Function to check if a note is in the bitmap
    constexpr inline bool isSostenutoPedalHeldNote(int note) {
        if (note >= 0 && note < 128) {
            size_t index = note / 64;
            size_t bit = note % 64;
            return (sostenutoPedalHeldNotesBitmap[index] & (1ULL << bit)) != 0;
        }
        return false;
    }

    // Function to reset (clear) all notes from the bitmap
    constexpr inline void resetSostenutoPedalHeldNotes() {
        sostenutoPedalHeldNotesBitmap[0] = 0;
        sostenutoPedalHeldNotesBitmap[1] = 0;
    }

	// Function to handle pedal release
    void handlePedalRelease()
    {
        // Pedal released - send note-offs for any held notes that aren't still pressed
        for (size_t k = 0; k < bitmapSize; ++k)
        {
            uint64_t bitset = sostenutoPedalHeldNotesBitmap[k];
            while (bitset != 0)
            {
                uint64_t t = bitset & -bitset;
                int r = countTrailingZeros(bitset);
                int note = k * 64 + r;

                if (!keyboardState.isNoteOn(1, note))
                {  // If note isn't physically held anymore
                    auto noteOff = juce::MidiMessage::noteOff(1, note);
                    if (midiOutput)
                    {
                        midiOutput->sendMessageNow(noteOff);
                        postMessageToList(noteOff, "Sostenuto Release");
                    }
                }

                bitset ^= t;
            }
        }
        resetSostenutoPedalHeldNotes();
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
    static constexpr size_t bitmapSize = 2; // 2 uint64_t for 128 MIDI notes
    double           startTime;
    bool                 loggingEnabled = false; // Start with logging disabled
    int                  currentLogLines = 0;  // Add this member variable to track line count
    static constexpr int MAX_LOG_LINES = 99;  // Maximum number of lines to keep in the log
    int  lastInputIndex = 0;
    bool isAddingFromMidiInput = false;
    juce::AudioDeviceManager deviceManager;       
    std::unique_ptr<juce::MidiOutput> midiOutput = nullptr;
    juce::ComboBox midiOutputList;
    juce::Label    midiOutputListLabel;
    juce::ComboBox midiInputList;                    
    juce::Label    midiInputListLabel;       
    juce::MidiKeyboardState     keyboardState;  
    juce::MidiKeyboardComponent keyboardComponent;  
    juce::TextEditor midiMessagesBox;
    PedalButton   sostenutoPedalButton;
    juce::ToggleButton   loggingEnabledButton;
    juce::String  onScreenKeyboardSource = juce::String("On-Screen Keyboard");
    juce::String  sostenutoPedalSource   = juce::String("Sostenuto Release" );
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};