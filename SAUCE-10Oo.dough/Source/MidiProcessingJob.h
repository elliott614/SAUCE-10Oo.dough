#pragma once
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
        parent.processMessageOnThread(message);
        return JobStatus::jobHasFinished;
    }

private:
    MainContentComponent& parent;
    juce::MidiMessage message;
};

// Add this method to MainContentComponent to process messages on worker threads
void processMessageOnThread(const juce::MidiMessage& message)
{
    const juce::ScopedLock sl(midiProcessLock);

    // Handle time-critical messages immediately
    if (message.isNoteOnOrOff() || (message.isController() && message.getControllerNumber() == 66))
    {
        processMidiMessage(message);
    }
    else
    {
        // For non-time-critical messages, add to buffer for batch processing
        midiMessageBuffer.addEvent(message, 0);
        triggerAsyncUpdate();
    }
}
