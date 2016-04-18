#include "AppsPageComponent.h"
#include "LauncherComponent.h"
#include "PokeLookAndFeel.h"
#include "Main.h"
#include "Utils.h"

void AppCheckTimer::timerCallback() {
  DBG("AppCheckTimer::timerCallback - check running apps");
  if (appsPage) {
    appsPage->checkRunningApps();
  }
}

void AppDebounceTimer::timerCallback() {
  DBG("AppDebounceTimer::timerCallback - check launch debounce");
  if (appsPage) {
    appsPage->debounce = false;
  }
  stopTimer();
}

AppIconButton::AppIconButton(const String &label, const String &shell, const Drawable *image)
: DrawableButton(label, DrawableButton::ImageAboveTextLabel),
  shell(shell) {
  // FIXME: supposedly setImages will "create internal copies of its drawables"
  // this relates to AppsPageComponent ownership of drawable icons ... docs are unclear
  setImages(image);
}

Rectangle<float> AppIconButton::getImageBounds() const {
  auto bounds = getLocalBounds();
  return bounds.withHeight(PokeLookAndFeel::getDrawableButtonImageHeightForBounds(bounds)).toFloat();
}

AppListComponent::AppListComponent() :
  train(new TrainComponent(TrainComponent::Orientation::kOrientationGrid)),
  nextPageBtn(createImageButton("NextAppsPage",
                                ImageFileFormat::loadFrom(assetFile("pageDownIcon.png")))),
  prevPageBtn(createImageButton("PrevAppsPage",
                                ImageFileFormat::loadFrom(assetFile("pageUpIcon.png"))))
{
  addChildComponent(nextPageBtn);
  addChildComponent(prevPageBtn);
  nextPageBtn->addListener(this);
  prevPageBtn->addListener(this);
  
  addAndMakeVisible(train);
}
AppListComponent::~AppListComponent() {}

DrawableButton *AppListComponent::createAndOwnIcon(const String &name, const String &iconPath, const String &shell) {
  auto image = createImageFromFile(assetFile(iconPath));
  auto drawable = new DrawableImage();
  drawable->setImage(image);
  // FIXME: is this OwnedArray for the drawables actually necessary?
  // won't the AppIconButton correctly own the drawable?
  // Further we don't actually use this list anywhere.
  iconDrawableImages.add(drawable);
  auto button = new AppIconButton(name, shell, drawable);
  addAndOwnIcon(name, button);
  return button;
}

void AppListComponent::resized() {
  auto b = getLocalBounds();
  
  prevPageBtn->setSize(btnHeight, btnHeight);
  nextPageBtn->setSize(btnHeight, btnHeight);
  prevPageBtn->setBoundsToFit(b.getX(), b.getY(), b.getWidth(), b.getHeight(), Justification::centredTop, true);
  nextPageBtn->setBoundsToFit(b.getX(), b.getY(), b.getWidth(), b.getHeight(), Justification::centredBottom, true);
  
  // drop the page buttons from our available layout size
  auto trainWidth = b.getWidth();
  auto trainHeight = b.getHeight() - (2.1*btnHeight);
  train->setSize(trainWidth, trainHeight);
  train->setBoundsToFit(b.getX(), b.getY(), b.getWidth(), b.getHeight(), Justification::centred, true);
}

void AppListComponent::checkShowPageNav() {
  if (train->hasNextPage()) {
    nextPageBtn->setVisible(true); nextPageBtn->setEnabled(true);
  }
  else {
    nextPageBtn->setVisible(false); nextPageBtn->setEnabled(false);
  }
  
  if (train->hasPrevPage()) {
    prevPageBtn->setVisible(true); prevPageBtn->setEnabled(true);
  }
  else {
    prevPageBtn->setVisible(false); prevPageBtn->setEnabled(false);
  }
}

void AppListComponent::addAndOwnIcon(const String &name, Component *icon) {
  trainIcons.add(icon);
  train->addItem(icon);
  ((Button*)icon)->setTriggeredOnMouseDown(true);
  ((Button*)icon)->addListener(this);
}

Array<DrawableButton *> AppListComponent::createIconsFromJsonArray(const var &json) {
  Array<DrawableButton *> buttons;
  if (json.isArray()) {
    for (const auto &item : *json.getArray()) {
      auto name = item["name"];
      auto shell = item["shell"];
      auto iconPath = item["icon"];
      if (name.isString() && shell.isString() && iconPath.isString()) {
        auto icon = createAndOwnIcon(name, iconPath, shell);
        if (icon) {
          buttons.add(icon);
        }
      }
    }
  }
  
  checkShowPageNav();
  return buttons;
}

LibraryListComponent::LibraryListComponent() :
  AppListComponent()
{
  bgColor = Colour(PokeLookAndFeel::chipPurple);
}
LibraryListComponent::~LibraryListComponent() {}

void LibraryListComponent::paint(Graphics &g) {
  g.fillAll(bgColor);
}

void LibraryListComponent::resized() {
  AppListComponent::resized();
  
  const auto& b = getLocalBounds();
  auto trainWidth = b.getWidth() - 2*btnHeight;
  auto trainHeight = b.getHeight() - (2.1*btnHeight);
  train->setSize(trainWidth, trainHeight);
  train->setBoundsToFit(b.getX(), b.getY(), b.getWidth(), b.getHeight(), Justification::centred, true);
  
  backButton->setBounds(b.getWidth()-60, b.getY(), 60, b.getHeight());
}

void LibraryListComponent::buttonClicked(Button *button) {
  if (button == prevPageBtn) {
    train->showPrevPage();
    checkShowPageNav();
  }
  else if (button == nextPageBtn) {
    train->showNextPage();
    checkShowPageNav();
  }
  else {
    getMainStack().popPage(PageStackComponent::kTransitionTranslateHorizontalLeft);
  }
}

AppsPageComponent::AppsPageComponent(LauncherComponent* launcherComponent) :
  AppListComponent(),
  launcherComponent(launcherComponent),
  runningCheckTimer(),
  debounceTimer()
{
  runningCheckTimer.appsPage = this;
  debounceTimer.appsPage = this;
}

AppsPageComponent::~AppsPageComponent() {}

Array<DrawableButton *> AppsPageComponent::createIconsFromJsonArray(const var &json) {
  auto buttons = AppListComponent::createIconsFromJsonArray(json);
  
  // hard coded "virtual" application. Cannot be removed.
  appsLibraryBtn = createAndOwnIcon("App Get", "appIcons/update.png", String::empty);
  buttons.add(appsLibraryBtn);
  
  checkShowPageNav();
  return buttons;
}

void AppsPageComponent::startApp(AppIconButton* appButton) {
  DBG("AppsPageComponent::startApp - " << appButton->shell);
  auto launchApp = new ChildProcess();
  if (launchApp->start(appButton->shell)) {
    runningApps.add(launchApp);
    runningAppsByButton.set(appButton, runningApps.indexOf(launchApp));
    // FIXME: uncomment when process running check works
    // runningCheckTimer.startTimer(5 * 1000);
    
    debounce = true;
    debounceTimer.startTimer(2 * 1000);
    
    // TODO: should probably put app button clicking logic up into LauncherComponent
    // correct level for event handling needs more thought
    launcherComponent->showLaunchSpinner();
  }
};

void AppsPageComponent::focusApp(AppIconButton* appButton, const String& windowId) {
  DBG("AppsPageComponent::focusApp - " << appButton->shell);
  StringArray focusCmd{"xdotool", "windowactivate", windowId.toRawUTF8()};
  ChildProcess focusWindow;
  focusWindow.start(focusCmd);
};

void AppsPageComponent::startOrFocusApp(AppIconButton* appButton) {
  if (debounce) return;
  
  bool shouldStart = true;
  int appIdx = runningAppsByButton[appButton];
  bool hasLaunched = runningApps[appIdx] != nullptr;
  String windowId;
  
  if(hasLaunched) {
    StringArray findCmd{"xdotool", "search", "--all", "--limit", "1", "--class", appButton->shell.toRawUTF8()};
    ChildProcess findWindow;
    findWindow.start(findCmd);
    findWindow.waitForProcessToFinish(1000);
    windowId = findWindow.readAllProcessOutput().trimEnd();
    
    // does xdotool find a window id? if so, we shouldn't start a new one
    shouldStart = (windowId.length() > 0) ? false : true;
  }
  
  if (shouldStart) {
    startApp(appButton);
  }
  else {
    focusApp(appButton, windowId);
  }
  
};

void AppsPageComponent::openAppsLibrary() {
  launcherComponent->showAppsLibrary();
}

void AppsPageComponent::checkRunningApps() {
  Array<int> needsRemove{};
  
  // check list to mark any needing removal
  for (const auto& cp : runningApps) {
    if (!cp->isRunning()) {
      needsRemove.add(runningApps.indexOf(cp));
    }
  }
  
  // cleanup list
  for (const auto appIdx : needsRemove) {
    runningApps.remove(appIdx);
    runningAppsByButton.removeValue(appIdx);
  }
  
  if (!runningApps.size()) {
    // FIXME: uncomment when process running check works
    // runningCheckTimer.stopTimer();
    launcherComponent->hideLaunchSpinner();
  }
};

void AppsPageComponent::buttonClicked(Button *button) {
  if (button == prevPageBtn) {
    train->showPrevPage();
    checkShowPageNav();
  }
  else if (button == nextPageBtn) {
    train->showNextPage();
    checkShowPageNav();
  }
  else if (button == appsLibraryBtn) {
    openAppsLibrary();
  }
  else {
    auto appButton = (AppIconButton*)button;
    startOrFocusApp(appButton);
  }
}
