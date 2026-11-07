// Brightness the lights-off button restores
var brightnessBeforeOff = 100;
// sliders the user is dragging are not moved by status updates
var levelDragging = {};

function updateNavStatus() {
  checkLogInStatus();
  if (playingStatus.status == undefined) return;

  var active = playingStatus.status != 'idle';
  $('#random, #steplooping, #playlistlooping').prop('disabled', !active);
  setPressed('#random', active && playingStatus['random'] == "true");
  setPressed('#steplooping', active && playingStatus['steplooping'] == "true");
  setPressed('#playlistlooping', active && playingStatus['playlistlooping'] == "true");

  // volume
  var volume = parseInt(playingStatus['volume'], 10);
  if (!isNaN(volume)) {
    if (!levelDragging.volume) $('#volumeSlider').val(volume);
    $('#volumeValue').text(volume == 0 ? 'Muted' : volume + '%');
    setPressed('#toggleMute', volume == 0);
    $('#toggleMute').html(xsIcon(volume == 0 ? 'mute' : 'volume', 18)).attr('title', volume == 0 ? 'Unmute' : 'Mute');
  }

  // brightness
  var brightness = parseInt(playingStatus['brightness'], 10);
  if (!isNaN(brightness)) {
    if (brightness > 0) brightnessBeforeOff = brightness;
    if (!levelDragging.brightness) $('#brightnessSlider').val(brightness);
    $('#brightnessValue').text(brightness == 0 ? 'Off' : brightness + '%');
    setPressed('#brightnessLevel', brightness == 0);
    $('#brightnessLevel').html(xsIcon(brightness == 0 ? 'dark' : 'sun', 18)).attr('title', brightness == 0 ? 'Restore brightness' : 'Lights off');
  }

  //output to lights
  var lightsOn = playingStatus['outputtolights'] == 'true';
  $('#outputtolights').toggleClass('is-bad', !lightsOn)
    .attr('title', lightsOn ? 'Output to lights is on. Click to turn it off.' : 'Output to lights is off. Click to turn it on.')
    .find('.label').text(lightsOn ? 'Lights on' : 'Lights off');

  updateControllerStatus();

  //update xlights version
  $("#version").html(" " + xsEscape(playingStatus['version']) + (playingStatus['build'] ? ' (' + xsEscape(playingStatus['build']) + ')' : ''));
}

function setPressed(selector, pressed) {
  $(selector).attr('aria-pressed', pressed ? 'true' : 'false');
}

// Wires the volume slider: sends the level while dragging (at most every 200ms) and when released.
function smartVolume() {
  wireLevelSlider('#volumeSlider', 'volume', 'Set volume to', function(v) {
    $('#volumeValue').text(v == 0 ? 'Muted' : v + '%');
  });
}

function smartBrightness() {
  wireLevelSlider('#brightnessSlider', 'brightness', 'Set brightness to n%', function(v) {
    $('#brightnessValue').text(v == 0 ? 'Off' : v + '%');
  });
}

function wireLevelSlider(selector, key, command, show) {
  var last = 0;
  var pending;
  $(selector).on('input', function() {
    var value = $(this).val();
    levelDragging[key] = true;
    show(value);
    clearTimeout(pending);
    var now = Date.now();
    if (now - last > 200) {
      last = now;
      runCommand(command, value);
    } else {
      pending = setTimeout(function() {
        last = Date.now();
        runCommand(command, value);
      }, 200);
    }
  }).on('change', function() {
    clearTimeout(pending);
    runCommand(command, $(this).val());
    setTimeout(function() { levelDragging[key] = false; }, 1500);
  });
}

function toggleLightsOff() {
  var brightness = parseInt(playingStatus['brightness'], 10);
  if (brightness == 0) {
    runCommand('Set brightness to n%', brightnessBeforeOff > 0 ? brightnessBeforeOff : 100);
  } else {
    runCommand('Set brightness to n%', 0);
  }
}
