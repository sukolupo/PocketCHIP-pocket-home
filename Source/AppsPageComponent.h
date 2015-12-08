#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "TrainComponent.h"

class AppsPageComponent : public Component {
public:
  ScopedPointer<TrainComponent> train;
  OwnedArray<Component> trainIcons;

  AppsPageComponent();
  ~AppsPageComponent();

  void paint(Graphics &) override;
  void resized() override;

  void addIcon(const String &name, const String &iconPath);
  void addIconsFromJsonArray(const var &json);

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppsPageComponent)
};
