$(document).ready(function() {
  if (getQueryVariable("args") == false) {
    playlistsLoadPlaylists();
  } else {
    playlistsLoadPlaylistsSteps(getQueryVariable("args"));
  }
});

function playlistsLoadPlaylists() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayLists',
    dataType: "json",
    success: function(response) {
      var playlists = response.playlists || [];
      $('#currentPlaylist').text("Playlists");
      $('#playlistsSub').text(playlists.length + ' playlists');
      $('#playlist').html(playlists.map(xsPlaylistItem).join('') || '<li class="xs-empty">There are no playlists. Create them in xSchedule on the PC.</li>');
    }
  });
}

function playlistsLoadPlaylistsSteps(playlist) {
  $('#playlistsBack').removeAttr('hidden');
  $('#currentPlaylist').text(playlist);
  $.ajax({
    url: '/xScheduleQuery?Query=GetPlayLists',
    dataType: "json",
    success: function(lists) {
      var id = '';
      (lists.playlists || []).forEach(function(p) {
        if (p.name == playlist) id = p.id;
      });
      $.ajax({
        url: '/xScheduleQuery?Query=GetPlayListSteps&Parameters=' + encodeURIComponent(playlist),
        dataType: "json",
        success: function(response) {
          if (response.result == 'failed') {
            $('#playlist').html('<li class="xs-empty">' + xsEscape(response.message) + '</li>');
            return;
          }
          var steps = response.steps || [];
          var playingHere = playingStatus.status != undefined && playingStatus.status != 'idle' && playingStatus.playlistid == id;
          $('#playlistsSub').text(steps.length + ' steps');
          $('#playlist').html(steps.map(function(step, i) {
            var cur = playingHere && playingStatus.stepid == step.id;
            return '<li class="xs-item' + (cur ? ' cur' : '') + '">' +
              '<span class="n">' + (i + 1) + '</span>' +
              '<span class="name">' + xsEscape(step.name) + '</span>' +
              '<span class="tag">' + (cur ? '<span class="xs-pill p-acc">Playing</span>' : '') + '</span>' +
              '<span class="len">' + xsEscape(xsDuration(step.length)) + '</span>' +
              '<span class="acts"><button type="button" class="xs-btn xs-btn-sm xs-hover-btn" data-start-step="' + xsEscape(step.id) + '" title="Play the playlist starting at this step">' + xsIcon('play', 12) + '<span class="txt">Play from here</span></button></span>' +
              '</li>';
          }).join(''));
          // the steps may load before the first status arrives
          onStatus('playlists', function(st) {
            var here = st.status != 'idle' && st.playlistid == id;
            $('#playlist .xs-item').each(function() {
              var cur = here && $(this).find('[data-start-step]').attr('data-start-step') == st.stepid;
              if (cur == $(this).hasClass('cur')) return;
              $(this).toggleClass('cur', cur).find('.tag').html(cur ? '<span class="xs-pill p-acc">Playing</span>' : '');
            });
          });
          $('#playlist').on('click', '[data-start-step]', function() {
            runCommand('Play playlist starting at step', 'id:' + id + ',id:' + $(this).attr('data-start-step'));
            updatePage('page', 'home');
          });
        }
      });
    }
  });
}
