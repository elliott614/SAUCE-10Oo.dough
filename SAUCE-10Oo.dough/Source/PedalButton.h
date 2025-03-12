class PedalButton : public juce::Button
{
public:
    PedalButton(const juce::String& buttonName)
        : juce::Button(buttonName)
    {
        setMouseClickGrabsKeyboardFocus(false);
        // Change to non-latching behavior
        setClickingTogglesState(false);
    }

    // Override mouseDown to immediately trigger the sostenuto effect
    void mouseDown(const juce::MouseEvent& e) override
    {
        // Call the base class implementation
        juce::Button::mouseDown(e);

        // Immediately trigger as if it was toggled on
        setToggleState(true, juce::sendNotification);
    }

    // Override mouseUp to release the sostenuto effect
    void mouseUp(const juce::MouseEvent& e) override
    {
        // Call the base class implementation
        juce::Button::mouseUp(e);

        // Reset toggle state when mouse is released
        setToggleState(false, juce::sendNotification);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat();
        // Draw the pedal shape
        juce::Path pedalPath;
        const float topRounding = 12.0f;    // More rounded at the top
        const float bottomRounding = 6.0f;   // Less rounded at the bottom

        pedalPath.addRoundedRectangle(bounds.getX(), bounds.getY(),
            bounds.getWidth(), bounds.getHeight() * 0.5f,
            topRounding, topRounding, true, true, false, false);

        pedalPath.addRoundedRectangle(bounds.getX(), bounds.getY() + bounds.getHeight() * 0.5f,
            bounds.getWidth(), bounds.getHeight() * 0.5f,
            bottomRounding, bottomRounding, false, false, true, true);
        // Fill with gradient - darker when pressed
        juce::Colour baseColour = shouldDrawButtonAsDown ? juce::Colours::darkred.darker(0.2f)
            : juce::Colours::goldenrod;

        juce::ColourGradient gradient(baseColour.brighter(0.2f), bounds.getTopLeft(),
            baseColour.darker(0.2f), bounds.getBottomRight(), false);
        g.setGradientFill(gradient);
        g.fillPath(pedalPath);
        // Add lighting effect when pressed
        if (shouldDrawButtonAsDown || getToggleState())
        {
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.fillPath(pedalPath);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            // Highlight effect when mouse is over
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.fillPath(pedalPath);
        }
        // Draw the outline
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.strokePath(pedalPath, juce::PathStrokeType(1.5f));

        // Draw pedal name
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(12.0f);

        // Split the text into lines
        juce::StringArray lines;
        lines.addLines(getName());

        const float lineHeight = g.getCurrentFont().getHeight();
        const float totalTextHeight = lineHeight * lines.size();

        // Center text vertically
        float yPos = bounds.getY() + (bounds.getHeight() - totalTextHeight) / 2.0f;

        // Draw each line individually
        for (const auto& line : lines)
        {
            g.drawText(line, bounds.getX() + 4, (int)yPos, (int)bounds.getWidth() - 8, (int)lineHeight,
                juce::Justification::centred, true);
            yPos += lineHeight;
        }
    }

    // Call this when receiving CC66 messages to update the button state
    void handleCC66(int value)
    {
        bool shouldBeOn = value >= 64;
        if (getToggleState() != shouldBeOn)
        {
            setToggleState(shouldBeOn, juce::dontSendNotification);
            repaint(); // Ensure immediate visual update
        }
    }
};