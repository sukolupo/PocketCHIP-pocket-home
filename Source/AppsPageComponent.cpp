#include "AppsPageComponent.h"
#include "Utils.h"

AppsPageComponent::AppsPageComponent() {
  train = new TrainComponent();
  addAndMakeVisible(train);
}

AppsPageComponent::~AppsPageComponent() {}

void AppsPageComponent::paint(Graphics &g) {}

void AppsPageComponent::resized() {
  auto bounds = getLocalBounds();

  train->centreWithSize(bounds.getWidth(), 96);
}

void AppsPageComponent::addIcon(const String &name, const String &iconPath) {
  auto iconFile = File::isAbsolutePath(iconPath)
                      ? File(iconPath)
                      : File::getCurrentWorkingDirectory().getChildFile(iconPath);

  auto button = createIconButton(name, iconFile);
  trainIcons.add(button);
  train->addItem(button);
}

void AppsPageComponent::addIconsFromJsonArray(const var &json) {
  if (json.isArray()) {
    for (const auto &item : *json.getArray()) {
      auto name = item["name"];
      auto icon = item["icon"];
      if (name.isString()) {
        addIcon(name, icon);
      }
    }
  }
}
