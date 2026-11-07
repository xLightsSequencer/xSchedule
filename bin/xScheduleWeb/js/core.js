$(document).ready(function() {
  // fall back to polling whenever the web socket is not connected
  xsNativeSetInterval.call(window, function() {
    if (socket.readyState != 1) {
      updateStatus();
    }
  }, 1000);

  xsFillIcons(document);
  loadUISettings();
  navLoadPlaylists();
  navLoadPlugins();
  checkLogInStatus();
  loadXyzzyData();
  loadXyzzy2Data();

  if (/Android|webOS|iPhone|iPad|iPod|BlackBerry/i.test(navigator.userAgent)) {
    isMobile = true;
  }
});
var isMobile;
//global status
var playingStatus = `{status: "unknown"}`;

function updateStatus() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayingStatus',
    dataType: "json",
    success: function(response) {
      playingStatus = response;
      if (response.status != undefined) {
        updateNavStatus();
        callRegisteredForStatus(response);
      }
    }
  });
}

var registeredForStatus = [];
var statusHandlers = {};
var availableMatrices;
var xyzzyHighScore;
var xyzzy2HighScore;
var uiSettings;

function defaultUISettings() {
  return {
    "webName": "xLights Scheduler",
    "webColor": "#004800",
    "notificationLevel": "1",
    "theme": "auto",
    "pages": [{
      "page": "home",
      "values": [true, false]
    }, {
      "page": "playlists",
      "values": [true, true]
    }, {
      "page": "settings",
      "values": [true, true]
    }],
    "navbuttons": [{
      "random": true,
      "steplooping": false,
      "playlistlooping": true,
      "volumeMute": true,
      "brightnessLevel": false,
      "outputtolights": true,
      "toggleMute": false
    }]
  };
}

function loadUISettings(resetToDefault) {
  $.ajax({
    type: "GET",
    url: '/xScheduleStash?Command=Retrieve&Key=uiSettings',
    success: function(response) {

      if (response.result == "not logged in") {
        window.location.href = "login.html";
      } else {
        if (response.result == 'failed' && response.message == '' || resetToDefault == true) {
          var defaultSettings = defaultUISettings();
          storeKey('uiSettings', JSON.stringify(defaultSettings), '2');
          uiSettings = defaultSettings;
          if (currentPage == 'settings' && typeof loadSettings === "function")
            loadSettings();
          window.scrollTo(0, 0);
        } else {
          try {
            uiSettings = (typeof response === "string") ? JSON.parse(response) : response;
          } catch (e) {
            uiSettings = defaultUISettings();
          }
        }
        populateUI();
      }
    },
    error: function(response) {},
  });
}

function getPage(settings, pageName) {
  for (var page in settings.pages) {
    if (settings.pages[page].page == pageName) {
      return settings.pages[page];
    }
  }
}

function populateUI() {
  var defaults = defaultUISettings();
  var nb = (uiSettings.navbuttons && uiSettings.navbuttons[0]) || defaults.navbuttons[0];
  var name = uiSettings.webName || defaults.webName;

  $('#headerName').text(name);
  document.title = name;
  applyNavColor(uiSettings.webColor || defaults.webColor);
  applyTheme(uiSettings.theme);

  // body classes hide the quick controls that are switched off in settings, on whichever page is loaded
  ['random', 'steplooping', 'playlistlooping', 'volumeMute', 'brightnessLevel', 'outputtolights', 'toggleMute'].forEach(function(key) {
    $('body').toggleClass('xs-no-' + key, nb[key] !== true);
  });

  updateNavStatus();
  populateSideBar();
}

function applyTheme(theme) {
  if (theme == 'light' || theme == 'dark') {
    document.documentElement.setAttribute('data-theme', theme);
  } else {
    document.documentElement.removeAttribute('data-theme');
  }
  try {
    localStorage.setItem('xsTheme', theme || 'auto');
  } catch (e) {}
}

function xsParseColor(c) {
  c = String(c || '').trim();
  var m = c.match(/^#?([0-9a-f]{3}|[0-9a-f]{6})$/i);
  if (m) {
    var h = m[1];
    if (h.length == 3) h = h.replace(/(.)/g, '$1$1');
    return [parseInt(h.substr(0, 2), 16), parseInt(h.substr(2, 2), 16), parseInt(h.substr(4, 2), 16)];
  }
  m = c.match(/rgba?\(\s*(\d+)[,\s]+(\d+)[,\s]+(\d+)/i);
  if (m) return [+m[1], +m[2], +m[3]];
  return null;
}

function xsLuminance(rgb) {
  var l = rgb.map(function(v) {
    v /= 255;
    return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
  });
  return 0.2126 * l[0] + 0.7152 * l[1] + 0.0722 * l[2];
}

function xsMix(a, b, t) {
  return [0, 1, 2].map(function(i) { return Math.round(a[i] + (b[i] - a[i]) * t); });
}

function xsHex(rgb) {
  return '#' + rgb.map(function(v) { return ('0' + v.toString(16)).slice(-2); }).join('');
}

// The Nav Color colors the top bar as before, and also becomes the accent, adjusted so it stays readable on
// light and dark backgrounds.
function applyNavColor(color) {
  var base = xsParseColor(color) || [0, 72, 0];
  var white = [255, 255, 255], black = [0, 0, 0], panel = [38, 38, 38];
  var navDark = xsLuminance(base) > 0.45;

  var accL = base;
  for (var i = 0; i < 12 && xsLuminance(accL) > 0.22; i++) accL = xsMix(accL, black, 0.15);
  var accD = base;
  for (var i = 0; i < 12 && xsLuminance(accD) < 0.12; i++) accD = xsMix(accD, white, 0.15);
  var textD = accD;
  for (var i = 0; i < 12 && xsLuminance(textD) < 0.4; i++) textD = xsMix(textD, white, 0.15);

  var vars = {
    '--nav-color': xsHex(base),
    '--nav-fg-color': navDark ? '#1b1f24' : '#ffffff',
    '--nav-fg-rgb-l': navDark ? '27, 31, 36' : '255, 255, 255',
    '--accent-l': xsHex(accL),
    '--on-accent-l': '#ffffff',
    '--accent-soft-l': xsHex(xsMix(accL, white, 0.88)),
    '--accent-text-l': xsHex(accL),
    '--accent-d': xsHex(accD),
    '--on-accent-d': xsLuminance(accD) > 0.45 ? '#111111' : '#ffffff',
    '--accent-soft-d': xsHex(xsMix(accD, panel, 0.72)),
    '--accent-text-d': xsHex(textD)
  };
  for (var k in vars) document.documentElement.style.setProperty(k, vars[k]);
  $('meta[name=theme-color]').attr('content', xsHex(base));
  try {
    localStorage.setItem('xsColorVars', JSON.stringify(vars));
  } catch (e) {}
}

function checkLogInStatus() {
  if (playingStatus.status == 'unknown') {
    $.ajax({
      url: '/xScheduleQuery?Query=GetPlayingStatus',
      dataType: "json",
      success: function(response) {
        if (response.result == "not logged in") {
          window.location.href = "login.html";
        }
      }

    });
  } else if (playingStatus.result == "not logged in") {
    window.location.href = "login.html";
  }
}

var navPlaylists = [];

function navLoadPlaylists() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayLists',
    dataType: "json",
    success: function(response) {
      navPlaylists = response.playlists || [];
    }
  });
}

function navLoadPlugins() {
  $.ajax({
    url: '/xScheduleQuery?Query=ListWebFolders&Parameters=Plugins',
    dataType: "json",
    success: function(response) {
      var folders = response.folders || [];
      var items = '';
      for (var i = 0; i < folders.length; i++) {
        items += '<li><a data-plugin="' + xsEscape(folders[i]) + '">' + xsEscape(folders[i]) + '</a></li>';
      }
      if (items == '') items = '<li class="dropdown-header">No plugins installed</li>';
      $('#plugins ul').html(items);
      $('#morePlugins').replaceWith(items);
    }
  });
}

$(document).on('click', '[data-plugin]', function() {
  updatePage('plugin', $(this).attr('data-plugin'));
});

function logout() {
  if (socket.readyState == 1) {
    socket.send('{"Type":"Login","Credential":"Log Out"}');
  } else {

    $.ajax({
      url: '/xScheduleLogin?Credential=logout',
      dataType: "json",
      success: function(response) {
        console.log(response);
        notification('Logged Out', 'success', '2');

      },
      error: function(error) {
        console.log("ERROR: " + error);
      }
    });
  }
}

function runCommand(name, param, reference) {

  //if websocket if open use that
  if (socket.readyState == 1) {
    var message = {
      "Type": "Command",
      "Command": name
    };
    if (param != undefined) message.Parameters = String(param);
    if (reference != undefined) message.Reference = reference;
    socket.send(JSON.stringify(message));
  } else {
    var command = encodeURIComponent(name);
    if (param != undefined)
      command += '&Parameters=' + encodeURIComponent(param);

    $.ajax({
      url: '/xScheduleCommand?Command=' + command,
      success: function(response) {
        if (response.result == 'ok')
          notification(response.result + ': ' + response.message, 'success', '2');
        if (response.result == 'failed')
          notification('Failed: ' + response.message, 'danger', '0');
      },
      error: function(response) {
        notification(response.result + ': ' + response.message, 'danger', '1');
      }
    });
  }
}

// "h:mm:ss.fff" or "m:ss.fff" to seconds
function xsSeconds(time) {
  var seconds = 0;
  String(time || '').split('.')[0].split(':').forEach(function(part) {
    seconds = seconds * 60 + (parseInt(part, 10) || 0);
  });
  return seconds;
}

function findPercent(length, left) {
  var secLength = xsSeconds(length);
  if (secLength <= 0) return "0%";
  var percent = (secLength - xsSeconds(left)) / secLength * 100;
  return Math.round(percent) + "%";
}

// drops the milliseconds from a time the API returns ("3:05.000" -> "3:05")
function xsDuration(time) {
  return String(time || '').split('.')[0];
}

// "YYYY-MM-DD HH:MM" -> "22:00" today, "Fri 22:00" this week, otherwise "Dec 24 17:00"
function xsWhen(when) {
  var m = String(when || '').match(/^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}:\d{2})/);
  if (!m) return when || '';
  var date = new Date(+m[1], +m[2] - 1, +m[3]);
  var now = new Date();
  var today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  var days = Math.round((date - today) / 86400000);
  if (days == 0) return 'Today ' + m[4];
  if (days == 1) return 'Tomorrow ' + m[4];
  if (days > 1 && days < 7) return date.toLocaleDateString(undefined, { weekday: 'short' }) + ' ' + m[4];
  return date.toLocaleDateString(undefined, { month: 'short', day: 'numeric' }) + ' ' + m[4];
}

function xsEscape(text) {
  return String(text == undefined ? '' : text).replace(/[&<>"']/g, function(c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}

var xsIcons = {
  play: '<path fill="currentColor" d="M7.5 4.8v14.4L19.5 12z"/>',
  pause: '<path fill="currentColor" d="M6.5 5h4v14h-4zm7 0h4v14h-4z"/>',
  stop: '<rect fill="currentColor" x="6" y="6" width="12" height="12" rx="1.5"/>',
  prev: '<path fill="currentColor" d="M6 5h2.2v14H6zM19.5 5v14L9 12z"/>',
  next: '<path fill="currentColor" d="M15.8 5H18v14h-2.2zM4.5 5 15 12 4.5 19z"/>',
  shuffle: '<path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M3 7h3c4.5 0 6.5 10 11 10h3M17 14l3 3-3 3M3 17h3c1.5 0 2.7-1.3 3.8-3M13.2 10c1.1-1.7 2.3-3 3.8-3h3M17 4l3 3-3 3"/>',
  repeat: '<path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M4 12V9.5A2.5 2.5 0 0 1 6.5 7H19M16 4l3 3-3 3M20 12v2.5a2.5 2.5 0 0 1-2.5 2.5H5M8 20l-3-3 3-3"/>',
  volume: '<path fill="currentColor" d="M3.5 9h4L13 4.5v15L7.5 15h-4z"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M16.5 9a4.2 4.2 0 0 1 0 6M19 6.5a7.8 7.8 0 0 1 0 11"/>',
  mute: '<path fill="currentColor" d="M3.5 9h4L13 4.5v15L7.5 15h-4z"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="m16 9.5 5 5m0-5-5 5"/>',
  sun: '<circle cx="12" cy="12" r="4" fill="currentColor"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M12 2.5v2.2M12 19.3v2.2M2.5 12h2.2M19.3 12h2.2M5.3 5.3l1.6 1.6M17.1 17.1l1.6 1.6M5.3 18.7l1.6-1.6M17.1 6.9l1.6-1.6"/>',
  dark: '<circle cx="12" cy="12" r="4" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="m4 4 16 16"/>',
  power: '<path fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" d="M12 3v8M6.6 6.9a7.5 7.5 0 1 0 10.8 0"/>',
  help: '<circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M9.5 9.5a2.6 2.6 0 1 1 3.6 2.4c-.7.3-1.1.9-1.1 1.6v.5"/><circle cx="12" cy="17" r="1.2" fill="currentColor"/>',
  more: '<circle cx="5" cy="12" r="1.9" fill="currentColor"/><circle cx="12" cy="12" r="1.9" fill="currentColor"/><circle cx="19" cy="12" r="1.9" fill="currentColor"/>',
  list: '<path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M8 6h12M8 12h12M8 18h12M4 6h.01M4 12h.01M4 18h.01"/>',
  grid: '<path fill="none" stroke="currentColor" stroke-width="2" d="M4 4h7v7H4zm9 0h7v7h-7zM4 13h7v7H4zm9 0h7v7h-7z"/>',
  gear: '<circle cx="12" cy="12" r="3" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" d="M12 2.8 14 5l3-.4.8 2.9 2.6 1.6-1 2.9 1 2.9-2.6 1.6-.8 2.9-3-.4-2 2.2-2-2.2-3 .4-.8-2.9L3.6 15l1-2.9-1-2.9 2.6-1.6L7 4.6l3 .4z"/>',
  back: '<path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M15 5l-7 7 7 7"/>',
  forward: '<path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M9 5l7 7-7 7"/>',
  calendar: '<rect x="3.5" y="5" width="17" height="15" rx="2" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M3.5 10h17M8 3v4M16 3v4"/>'
};

function xsIcon(name, size) {
  size = size || 20;
  return '<svg width="' + size + '" height="' + size + '" viewBox="0 0 24 24" aria-hidden="true" focusable="false">' + (xsIcons[name] || '') + '</svg>';
}

// replaces <span data-icon="name" data-size="n"></span> placeholders
function xsFillIcons(root) {
  $(root).find('[data-icon]').each(function() {
    $(this).replaceWith(xsIcon($(this).attr('data-icon'), +$(this).attr('data-size') || undefined));
  });
}

var xsButtonColors = ['green', 'red', 'blue', 'cyan', 'orange'];

// A button set up in xSchedule. Guest buttons (labels starting "GB_") are tagged instead of showing the prefix.
function xsUserButton(button) {
  var label = String(button.label || '');
  var guest = label.indexOf('GB_') == 0;
  if (guest) label = label.substring(3);
  var color = xsButtonColors.indexOf(button.color) >= 0 ? button.color : 'default';
  return '<button type="button" class="ub ub-' + color + '" data-press-button="' + xsEscape(button.id) + '">' +
    (guest ? '<span class="tag">Guest</span>' : '') + xsEscape(label) + '</button>';
}

function xsPlaylistItem(playlist) {
  var playing = playingStatus.status != undefined && playingStatus.status != 'idle' && playingStatus.playlistid == playlist.id;
  return '<li class="xs-item pl">' +
    '<span class="name"><b>' + xsEscape(playlist.name) + '</b>' +
    (playlist.nextscheduled ? '<small>Next ' + xsEscape(xsWhen(playlist.nextscheduled)) + '</small>' : '') + '</span>' +
    '<span class="tag">' + (playing ? '<span class="xs-pill p-acc">Playing</span>' : '') + '</span>' +
    '<span class="len">' + xsEscape(xsDuration(playlist.length)) + '</span>' +
    '<span class="acts"><button type="button" class="xs-btn xs-btn-sm" data-view-playlist="' + xsEscape(playlist.name) + '">View</button>' +
    '<button type="button" class="xs-btn xs-btn-sm xs-btn-primary" data-play-playlist="' + xsEscape(playlist.id) + '">' + xsIcon('play', 12) + 'Play</button>' +
    '<span class="dropdown"><button type="button" class="xs-btn xs-btn-sm dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false" title="More ways to play" aria-label="More ways to play">' + xsIcon('more', 12) + '</button>' +
    '<ul class="dropdown-menu dropdown-menu-right">' +
    '<li><a data-play-mode="Play specified playlist next" data-playlist-id="' + xsEscape(playlist.id) + '">Play next</a></li>' +
    '<li><a data-play-mode="Play specified playlist now then resume" data-playlist-id="' + xsEscape(playlist.id) + '">Play now, then resume</a></li>' +
    '<li><a data-play-mode="Play specified playlist now then restart interrupted step" data-playlist-id="' + xsEscape(playlist.id) + '">Play now, then restart the interrupted song</a></li>' +
    '</ul></span></span>' +
    '</li>';
}

$(document).on('click', '[data-press-button]', function() {
  runCommand('PressButton', 'id:' + $(this).attr('data-press-button'));
});
$(document).on('click', '[data-play-playlist]', function() {
  runCommand('Play specified playlist', 'id:' + $(this).attr('data-play-playlist'));
});
$(document).on('click', '[data-play-mode]', function() {
  runCommand($(this).attr('data-play-mode'), 'id:' + $(this).attr('data-playlist-id'));
});
$(document).on('click', '[data-view-playlist]', function() {
  updatePage('page', 'playlists', $(this).attr('data-view-playlist'));
});

function storeKey(key, value, notificationLevel) {
  // written by cp16net so blame him.
  if (notificationLevel == undefined) {
    notificationLevel = "";
  }
  $.ajax({
    type: "POST",
    url: '/xScheduleStash?Command=Store&Key=' + key,
    data: value,
    success: function(response) {
      notification("Settings Saved", "success", notificationLevel);
      notification(response.result + response.message, "success", "2");
    },
    error: function(error) {
      notification("Saving failed", "danger", '0');
    }
  });
}

function retrieveKey(key, f) {
  // written by cp16net so blame him.
  $.ajax({
    type: "GET",
    url: '/xScheduleStash?Command=Retrieve&Key=' + key,
    success: f,
    error: f,
  });
}

function notification(message, color, priority) {

  // priority   //
  // "" = always //
  // 1 = debug   //
  if (uiSettings == undefined || priority <= uiSettings.notificationLevel) {
    $.notify({
      // options
      message: message
    }, {
      // settings
      type: color,
      placement: { from: 'bottom', align: 'center' },
      offset: { y: 76 },
      delay: 3000
    });
  }
}

function loadXyzzyData() {
  //matrix
  $.ajax({
    type: "GET",
    url: '/xScheduleQuery?Query=GetMatrices',
    success: function(response) {
      availableMatrices = response;
    }
  });

  $.ajax({
    type: "GET",
    url: '/xyzzy?c=highscore',
    success: function(response) {
      xyzzyHighScore = JSON.parse('{"highscoreplayer":"' + response.highscoreplayer + '","highscore":' + response.highscore + '}');
    }

  });
}

function loadXyzzy2Data() {
  $.ajax({
    type: "GET",
    url: '/xyzzy2?c=highscore',
    success: function(response) {
      xyzzy2HighScore = JSON.parse('{"highscoreplayer":"' + response.highscoreplayer + '","highscore":' + response.highscore + '}');
    }

  });
}

//Upercase First Letter of string
function jsUcfirst(string) {
  return string.charAt(0).toUpperCase() + string.slice(1);
}

//sleep
function sleep(time) {
  return new Promise((resolve) => setTimeout(resolve, time));
}

// a plugin page's script runs again on every visit and registers a new copy of its function, so one with the same
// name replaces the old one
function registerStatusFunction(fnStatus) {
  var same = fnStatus.name ? registeredForStatus.findIndex(function(f) { return f.name === fnStatus.name; }) : -1;
  if (same >= 0) {
    registeredForStatus[same] = fnStatus;
  } else if (!registeredForStatus.includes(fnStatus)) {
    registeredForStatus.push(fnStatus);
  }
}

// Built-in pages register by name: their scripts run again on every visit, so registering the function itself would
// add a new copy each time.
function onStatus(name, fnStatus) {
  statusHandlers[name] = fnStatus;
}

function OnStatus(item, index, arr) {
  item(this);
}

function callRegisteredForStatus(response) {
  registeredForStatus.forEach(OnStatus, response);
  for (var name in statusHandlers) {
    statusHandlers[name](response);
  }
}
