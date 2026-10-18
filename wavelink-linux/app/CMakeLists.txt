find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Widgets Quick QuickControls2 WebSockets)

qt_add_executable(wavelink
  src/main.cpp
  src/Assets.cpp
  src/EngineClient.cpp
  src/Tray.cpp
  src/AppSettings.cpp
)

set_source_files_properties(qml/Theme.qml qml/Icons.qml qml/Style.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

if(QT_KNOWN_POLICY_QTP0004)
  qt_policy(SET QTP0004 NEW)
endif()
qt_add_qml_module(wavelink
  URI WaveLink
  VERSION 1.0
  # Embedded at qrc:/qt/qml/WaveLink, which the QML engine searches by default: the module then loads
  # without an on-disk copy, as in the relocatable release bundle.
  RESOURCE_PREFIX /qt/qml
  OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/WaveLink
  QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/Icons.qml
    qml/Style.qml
    qml/WText.qml
    qml/FontIcon.qml
    qml/QuietButton.qml
    qml/MuteToggle.qml
    qml/LevelMeter.qml
    qml/LevelSlider.qml
    qml/ChannelIcon.qml
    qml/MixIcon.qml
    qml/ChannelRow.qml
    qml/MixHeader.qml
    qml/MixCell.qml
    qml/AddChannelButton.qml
    qml/OutputDeviceButton.qml
    qml/TitleBar.qml
    qml/NavPane.qml
    qml/MixesPage.qml
    qml/DevicePage.qml
    qml/WSlider.qml
    qml/WToggleSwitch.qml
    qml/WButton.qml
    qml/WComboBox.qml
    qml/Card.qml
    qml/TabStrip.qml
    qml/InfoIcon.qml
    qml/WTextField.qml
    qml/SectionHeader.qml
    qml/Separator.qml
    qml/VoiceFocusControl.qml
    qml/EffectRow.qml
    qml/EffectEditor.qml
    qml/EffectsList.qml
    qml/InputEffects.qml
    qml/AppRoutes.qml
    qml/ChannelDialog.qml
    qml/PropertySlider.qml
    qml/ToggleProperty.qml
    qml/SoundCheck.qml
    qml/EqEditor.qml
    qml/SettingsDialog.qml
    qml/MarketplacePage.qml
)

target_link_libraries(wavelink PRIVATE Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::WebSockets)
target_compile_options(wavelink PRIVATE -Wall -Wextra)
install(TARGETS wavelink RUNTIME DESTINATION bin BUNDLE DESTINATION .)
