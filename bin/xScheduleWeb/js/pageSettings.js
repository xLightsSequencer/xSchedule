$(document).ready(function() {
  loadSettings();
  $('#webColorSetting').colorpicker();
  $('#settingsForm').on('submit', function(e) {
    e.preventDefault();
    updateSettings();
  });
  $('#settingsAbout').html('xSchedule ' + xsEscape(playingStatus.version || '') + ' &middot; <a href="https://xlights.org" target="_blank" rel="noopener">xlights.org</a>');
});

function loadSettings() {
  if (uiSettings != undefined) {
    var defaults = defaultUISettings();
    var nb = (uiSettings.navbuttons && uiSettings.navbuttons[0]) || defaults.navbuttons[0];
    $('#webNameSetting').val(uiSettings.webName);
    $('#webColorSetting').val(uiSettings.webColor);
    $('#themeSetting').val(uiSettings.theme == 'light' || uiSettings.theme == 'dark' ? uiSettings.theme : 'auto');
    $('#notificationLevelSetting').val(uiSettings.notificationLevel);
    $("#smartVolumeSetting").prop("checked", nb.volumeMute);
    $("#smartBrightnessSetting").prop("checked", nb.brightnessLevel);
    $("#outputToLightsSetting").prop("checked", nb.outputtolights);
    $("#repeatPlaylistSetting").prop("checked", nb.playlistlooping);
    $("#repeatStepsSetting").prop("checked", nb.steplooping);
    $("#toggleRandomSetting").prop("checked", nb.random);
    $("#toggleMuteSetting").prop("checked", nb.toggleMute);
  } else {
    setTimeout(loadSettings, 100);
  }
}

function updateSettings() {
  window.scrollTo(0, 0);
  // keep everything already stored (including keys this page does not show) and change only what it edits
  var updatedSettings = $.extend(true, {}, defaultUISettings(), uiSettings, {
    "webName": $('#webNameSetting').val(),
    "webColor": $('#webColorSetting').val(),
    "notificationLevel": $('#notificationLevelSetting').val(),
    "theme": $('#themeSetting').val()
  });
  updatedSettings.pages = (uiSettings && uiSettings.pages) || defaultUISettings().pages;
  updatedSettings.navbuttons = [{
    "random": $("#toggleRandomSetting").prop("checked"),
    "steplooping": $("#repeatStepsSetting").prop("checked"),
    "playlistlooping": $("#repeatPlaylistSetting").prop("checked"),
    "volumeMute": $("#smartVolumeSetting").prop("checked"),
    "brightnessLevel": $("#smartBrightnessSetting").prop("checked"),
    "outputtolights": $("#outputToLightsSetting").prop("checked"),
    "toggleMute": $("#toggleMuteSetting").prop("checked")
  }];
  //Save settings, Update UI
  storeKey('uiSettings', JSON.stringify(updatedSettings));
  uiSettings = updatedSettings;
  populateUI();
}
