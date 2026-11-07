// Plugin pages written for the old layout have #sideBar1 and #sideBar2; they still get the navigation and controller lists.
function populateSideBar() {
  if ($('#sideBar1').length) {
    var sidebar1 = `
      <div class="list-group">
      <div class="list-group-item active main-color-bg"><span class="glyphicon glyphicon-cog" aria-hidden="true"></span> Navigation </div>
      <a onclick="updatePage('page','home')" class="list-group-item" style="cursor: pointer;"><span class="glyphicon glyphicon-home" aria-hidden="true"></span>  Home </a>
      <a onclick="updatePage('page','playlists')" class="list-group-item" style="cursor: pointer;"><span class="glyphicon glyphicon-list-alt" aria-hidden="true"></span>  Playlists </a>
      <a onclick="updatePage('page','settings')" class="list-group-item" style="cursor: pointer;"><span class="glyphicon glyphicon-cog" aria-hidden="true"></span>  Settings </a>
      </div>
      `;
    $('#sideBar1').html(sidebar1);
  }

  if ($('#sideBar2').length) {
    var sidebar2 = `
    <div class="list-group">
      <div class="list-group-item active main-color-bg" data-toggle="collapse" data-target="#controllerStatusCollapse" style="cursor: pointer;">
        <span class="glyphicon glyphicon-sort" aria-hidden="true"></span> Controller Status
        <span class="glyphicon glyphicon-chevron-down pull-right" aria-hidden="true"></span>
      </div>
      <div id="controllerStatusCollapse" class="collapse in">
        <div id="controllerStatusTable"></div>
      </div>
    </div>
   `;
    $('#sideBar2').html(sidebar2);

    $('#controllerStatusCollapse').on('show.bs.collapse', function() {
      $(this).prev().find('.glyphicon.pull-right').removeClass('glyphicon-chevron-down').addClass('glyphicon-chevron-up');
    });
    $('#controllerStatusCollapse').on('hide.bs.collapse', function() {
      $(this).prev().find('.glyphicon.pull-right').removeClass('glyphicon-chevron-up').addClass('glyphicon-chevron-down');
    });
  }
  lastControllerStatus = '';
  updateControllerStatus();
}

var lastControllerStatus = '';

function controllerState(result) {
  if (result == 'Ok') return { dot: 'ok', pill: 'p-ok', text: 'OK' };
  if (result == 'Failed') return { dot: 'bad', pill: 'p-bad', text: 'No reply' };
  if (result == 'Unavailable') return { dot: 'wait', pill: 'p-wait', text: 'Unavailable' };
  return { dot: '', pill: 'p-off', text: 'Unknown' };
}

// Redraws the controller lists only when the ping results change.
function updateControllerStatus() {
  var pings = playingStatus.pingstatus;
  if (!Array.isArray(pings)) return;

  var key = JSON.stringify(pings);
  if (key == lastControllerStatus) return;
  lastControllerStatus = key;

  var ok = 0;
  var rows = '';
  for (var i = 0; i < pings.length; i++) {
    var s = controllerState(pings[i].result);
    if (pings[i].result == 'Ok') ok++;
    rows += '<div class="xs-ctrl"><span class="dot ' + s.dot + '"></span><span class="nm">' + xsEscape(pings[i].controller) +
      '</span><span class="xs-pill ' + s.pill + '">' + s.text + '</span></div>';
  }
  $('#controllerList').html(rows || '<p class="xs-empty">No controllers are being checked.</p>');
  $('#controllerCount').text(pings.length ? ok + ' of ' + pings.length + ' OK' : '');
  $('#controllerStatusTable').html(rows);

  var chip = $('#controllerChip');
  if (pings.length == 0) {
    chip.attr('hidden', true);
  } else {
    chip.removeAttr('hidden').toggleClass('is-bad', ok < pings.length)
      .attr('title', ok + ' of ' + pings.length + ' controllers answering');
    chip.find('.label').text(ok == pings.length ? 'Controllers OK' : (pings.length - ok) + ' of ' + pings.length + ' controllers down');
  }
}
