var homeState = { status: '', playlistId: null, steps: [] };

$(document).ready(function() {
  homeState = { status: '', playlistId: null, steps: [] };
  smartVolume();
  smartBrightness();
  onStatus('home', homeStatus);
  if (playingStatus.status != undefined) {
    updateNavStatus();
    homeStatus(playingStatus);
  }
  homeLoadComingUp();
  homeLoadButtons();
});

function homeStatus(s) {
  if (!$('#playerCard').length || s.status == undefined) return;

  var idle = s.status == 'idle';
  var changed = s.status != homeState.status;
  $('#playerActive').prop('hidden', idle);
  $('#playerIdle').prop('hidden', !idle);

  if (idle) {
    if (changed) {
      homeState.playlistId = null;
      homeLoadNext();
      homeShowPlaylists();
      homeLoadComingUp();
    }
  } else {
    homeShowPlaying(s, changed);
    if (s.playlistid != homeState.playlistId) {
      homeLoadSteps(s);
    } else {
      homeMarkSteps(s);
    }
    if (changed && homeState.status == 'idle') homeLoadComingUp();
  }
  homeState.status = s.status;
}

function homeUntil(end) {
  return xsWhen(end).replace(/^Today /, '');
}

function homeShowPlaying(s, changed) {
  var paused = s.status == 'paused';
  if (changed) {
    $('#playState').attr('class', 'xs-pill ' + (paused ? 'p-wait' : 'p-ok')).find('span:last').text(paused ? 'Paused' : 'Playing');
    $('#buttonPlayPause').html(xsIcon(paused ? 'play' : 'pause', 26)).attr('title', paused ? 'Resume' : 'Pause').attr('aria-label', paused ? 'Resume' : 'Pause');
  }
  $('#playPlaylist').text(s.playlist);
  if (s.scheduleend && s.scheduleend != 'N/A') {
    $('#playSchedule').text((s.schedulename ? s.schedulename + ', ' : '') + 'until ' + homeUntil(s.scheduleend));
  } else {
    $('#playSchedule').text('');
  }
  var queued = parseInt(s.queuelength, 10) || 0;
  $('#playQueue').prop('hidden', queued == 0).text(queued + ' queued');
  $('#playStep').text(s.step);

  var length = parseInt(s.lengthms, 10) || 0;
  var position = parseInt(s.positionms, 10) || 0;
  var percent = length > 0 ? Math.min(100, Math.max(0, position / length * 100)) : 0;
  $('#playFill').css('width', percent.toFixed(1) + '%');
  $('#playPos').text(xsDuration(s.position));
  $('#playLeft').text(length > 0 ? '−' + xsDuration(s.left) : '');

  var stepText = '';
  var index = homeStepIndex(s.stepid);
  if (index >= 0) stepText = 'Step ' + (index + 1) + ' of ' + homeState.steps.length;
  if (s.playlistleft) stepText += (stepText ? ' · ' : '') + xsDuration(s.playlistleft) + ' left in playlist';
  $('#playListLeft').text(stepText);

  $('#playNext').html(s.nextstep ? 'Up next: <b>' + xsEscape(s.nextstep) + '</b>' : '');
}

function homeStepIndex(id) {
  for (var i = 0; i < homeState.steps.length; i++) {
    if (homeState.steps[i].id == id) return i;
  }
  return -1;
}

function homeLoadSteps(s) {
  homeState.playlistId = s.playlistid;
  homeState.steps = [];
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayListSteps&Parameters=' + encodeURIComponent(s.playlist),
    dataType: "json",
    success: function(response) {
      if (homeState.playlistId != s.playlistid) return;
      homeState.steps = response.steps || [];
      var total = 0;
      var items = '';
      for (var i = 0; i < homeState.steps.length; i++) {
        var step = homeState.steps[i];
        total += parseInt(step.lengthms, 10) || 0;
        items += '<li class="xs-item" data-step="' + xsEscape(step.id) + '">' +
          '<span class="n">' + (i + 1) + '</span>' +
          '<span class="name">' + xsEscape(step.name) + '</span>' +
          '<span class="tag"></span>' +
          '<span class="len">' + xsEscape(xsDuration(step.length)) + '</span>' +
          '<span class="acts"><button type="button" class="xs-btn xs-btn-sm xs-hover-btn" data-play-step="' + xsEscape(step.id) + '" title="Play the playlist from this step">' + xsIcon('play', 12) + '<span class="txt">Play from here</span></button></span>' +
          '</li>';
      }
      $('#playlistsTable').html(
        '<div class="xs-card-h"><h2>' + xsEscape(s.playlist) + '</h2><span class="sub">' + homeState.steps.length + ' steps · ' + xsDuration(homeFormatMS(total)) + '</span></div>' +
        '<ol class="xs-list">' + items + '</ol>');
      homeMarkSteps(playingStatus);
      homeShowPlaying(playingStatus, false);
    }
  });
}

function homeFormatMS(ms) {
  var seconds = Math.round(ms / 1000);
  var h = Math.floor(seconds / 3600);
  var m = Math.floor(seconds / 60) % 60;
  var sec = ('0' + (seconds % 60)).slice(-2);
  return h > 0 ? h + ':' + ('0' + m).slice(-2) + ':' + sec : m + ':' + sec;
}

function homeMarkSteps(s) {
  var list = $('#playlistsTable .xs-item[data-step]');
  if (!list.length) return;
  if (list.filter('.cur').attr('data-step') == s.stepid && list.data('next') == s.nextstepid && list.data('state') == s.status) return;
  list.data('next', s.nextstepid).data('state', s.status);
  list.removeClass('cur').find('.tag').html('');
  $('#playlistsTable .xs-item[data-step="' + s.stepid + '"]').addClass('cur')
    .find('.tag').html('<span class="xs-pill p-acc">' + (s.status == 'paused' ? 'Paused' : 'Playing') + '</span>');
  if (s.nextstepid != s.stepid) {
    $('#playlistsTable .xs-item[data-step="' + s.nextstepid + '"] .tag').html('<span class="xs-pill p-off">Next</span>');
  }
}

$('#playlistsTable').on('click', '[data-play-step]', function() {
  runCommand('Play playlist starting at step', 'id:' + playingStatus.playlistid + ',id:' + $(this).attr('data-play-step'));
});

function homeShowPlaylists() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayLists',
    dataType: "json",
    success: function(response) {
      if (playingStatus.status != 'idle' || !response.playlists) return;
      $('#playlistsTable').html(
        '<div class="xs-card-h"><h2>Playlists</h2><span class="sub">' + response.playlists.length + ' playlists</span></div>' +
        '<ol class="xs-list">' + response.playlists.map(xsPlaylistItem).join('') + '</ol>');
    }
  });
}

function homeLoadNext() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetNextScheduledPlayList',
    dataType: "json",
    success: function(response) {
      var restart = response.start == "NOW!";
      $('#idleRestart').prop('hidden', !restart);
      $('#idlePlay').prop('hidden', !response.playlistid || response.start == "Never");
      if (response.start == "Never" || !response.playlistname) {
        $('#idleNext').text('No shows are scheduled.');
        return;
      }
      $('#idlePlay').data('playlist', response.playlistid).find('span:last').text('Play ' + response.playlistname);
      if (restart) {
        $('#idleNext').text('');
        $('#idleRestartText').html('The <b>' + xsEscape(response.schedulename) + '</b> schedule should be playing ' + xsEscape(response.playlistname) + ' now, until ' + xsEscape(response.end) + '.');
        $('#idleRestartButton').data('playlist', response.playlistid);
      } else {
        $('#idleNext').html('Next: <b>' + xsEscape(response.schedulename) + '</b> plays ' + xsEscape(response.playlistname) + ' ' + xsEscape(xsWhen(response.start)) + '.');
      }
    }
  });
}

$('#idlePlay').on('click', function() {
  runCommand('Play specified playlist', 'id:' + $(this).data('playlist'));
});
$('#idleRestartButton').on('click', function() {
  runCommand('Restart playlist schedules', 'id:' + $(this).data('playlist'));
});

function homeLoadComingUp() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayLists',
    dataType: "json",
    success: function(response) {
      var upcoming = (response.playlists || []).filter(function(p) { return p.nextscheduled; });
      upcoming.sort(function(a, b) { return a.nextscheduled < b.nextscheduled ? -1 : a.nextscheduled > b.nextscheduled ? 1 : 0; });
      var rows = upcoming.slice(0, 4).map(function(p) {
        return '<div class="xs-sched"><span>' + xsEscape(p.name) + '</span><span class="when">' + xsEscape(xsWhen(p.nextscheduled)) + '</span></div>';
      }).join('');
      $('#comingUp').html(rows || '<p class="xs-empty">Nothing is scheduled.</p>');
    }
  });
}

function homeLoadButtons() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetButtons',
    dataType: "json",
    success: function(response) {
      var buttons = response.buttons || [];
      $('#quickButtonsCard').prop('hidden', buttons.length == 0);
      $('#quickButtons').html(buttons.slice(0, 6).map(xsUserButton).join(''));
    }
  });
}
