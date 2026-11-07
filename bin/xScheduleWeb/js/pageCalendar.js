// Times from the server are minutes from the night's midnight, so 1470 is 12:30 AM the next morning.
var cal = {
  month: null,
  selected: null,
  today: null,
  inactive: false,
  schedules: {}
};

function calIso(d) {
  return d.getFullYear() + '-' + String(d.getMonth() + 1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');
}

function calParse(iso) {
  var p = iso.split('-');
  return new Date(+p[0], +p[1] - 1, +p[2]);
}

function calTime(minutes) {
  var d = new Date(2000, 0, 1, 0, 0);
  d.setMinutes(((minutes % 1440) + 1440) % 1440);
  return d.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' });
}

function calRange(s, e) {
  return calTime(s) + ' – ' + calTime(e);
}

// the browser's first day of the week (0 Sunday, 1 Monday); browsers that can't say get Sunday
function calFirstDay() {
  try {
    var locale = new Intl.Locale(navigator.language);
    var info = locale.getWeekInfo ? locale.getWeekInfo() : locale.weekInfo;
    if (info && info.firstDay) return info.firstDay % 7;
  } catch (e) {}
  return 0;
}

function calColor(id) {
  var s = cal.schedules[id];
  return 'xs-cal-c' + ((s ? s.index : 0) % 8);
}

function calName(id) {
  var s = cal.schedules[id];
  return s ? s.name : '';
}

function calQuery(query, params, done) {
  $.ajax({
    url: '/xScheduleQuery?Query=' + query + '&Parameters=' + encodeURIComponent(params + (cal.inactive ? ',inactive' : '')),
    dataType: 'json',
    success: function(response) {
      if (response.result == 'failed') return;
      (response.schedules || []).forEach(function(s) { cal.schedules[s.id] = s; });
      done(response);
    }
  });
}

function calLoadMonth() {
  var first = new Date(cal.month.getFullYear(), cal.month.getMonth(), 1);
  var fd = calFirstDay();
  var start = new Date(first.getFullYear(), first.getMonth(), 1 - (first.getDay() - fd + 7) % 7);
  var last = new Date(first.getFullYear(), first.getMonth() + 1, 0);
  var end = new Date(last.getFullYear(), last.getMonth(), last.getDate() + (6 - (last.getDay() - fd + 7) % 7));
  $('#calTitle').text(first.toLocaleDateString([], { month: 'long', year: 'numeric' }));
  $('#calGrid').addClass('xs-cal-loading');
  calQuery('GetCalendar', calIso(start) + ',' + calIso(end), function(r) {
    $('#calGrid').removeClass('xs-cal-loading').html(r.days.map(function(day) {
      var d = calParse(day.date);
      var cls = 'xs-cal-cell';
      if (d.getMonth() != cal.month.getMonth()) cls += ' out';
      if (day.holiday) cls += ' hol';
      if (day.date == calIso(cal.selected)) cls += ' sel';
      if (day.date == calIso(cal.today)) cls += ' today';
      var chips = day.windows.slice(0, 3).map(function(w) {
        var c = 'xs-cal-chip ' + calColor(w.id) + (w.skipped ? ' skipped' : '') + (w.enabled ? '' : ' inactive');
        return '<span class="' + c + '" title="' + xsEscape(calName(w.id) + ', ' + calRange(w.start, w.end) + (w.skipped ? ' (skipped)' : '')) + '">' +
          '<b>' + xsEscape(calTime(w.start)) + '</b><span>' + xsEscape(calName(w.id)) + '</span></span>';
      }).join('');
      var more = day.windows.length > 3 ? '<span class="xs-cal-more">+' + (day.windows.length - 3) + ' more</span>' : '';
      var clash = day.clashes.length ? '<span class="xs-cal-clash" title="Two schedules with the same priority overlap">Clash</span>' : '';
      var label = d.toLocaleDateString([], { weekday: 'long', month: 'long', day: 'numeric' }) + (day.holiday ? ', ' + day.holiday : '') + ', ' + day.windows.length + ' scheduled';
      return '<button type="button" class="' + cls + '" data-date="' + day.date + '" aria-label="' + xsEscape(label) + '">' +
        '<span class="hd"><span class="n">' + d.getDate() + '</span>' + (day.holiday ? '<span class="h">' + xsEscape(day.holiday) + '</span>' : '') + clash + '</span>' +
        chips + more + '</button>';
    }).join(''));
  });
}

function calLane(window, lane, axisStart, span) {
  var pct = function(m) { return Math.max(0, Math.min(100, (m - axisStart) / span * 100)); };
  var pos = function(s, e) { return 'left:' + pct(s).toFixed(2) + '%;width:' + (pct(e) - pct(s)).toFixed(2) + '%'; };
  if (window.skipped || !window.enabled) {
    return '<span class="xs-cal-seg dashed" style="' + pos(window.start, window.end) + '">' + (window.skipped ? 'Skipped tonight' : 'Inactive') + '</span>';
  }
  return lane.segments.map(function(g) {
    var text = g.state == 'plays' ? calRange(g.start, g.end) : g.state == 'done' ? 'Playlist finished' : (g.state == 'hidden' ? 'Waiting: ' : 'Clash with ') + calName(cal.dayWindows[g.by].id);
    return '<span class="xs-cal-seg st-' + g.state + ' ' + calColor(window.id) + '" style="' + pos(g.start, g.end) + '" title="' + xsEscape(text) + '">' + xsEscape(text) + '</span>';
  }).join('');
}

function calLoadDay() {
  var d = cal.selected;
  $('#calDayTitle').text(d.toLocaleDateString([], { weekday: 'long', month: 'long', day: 'numeric' }));
  calQuery('GetCalendarDay', calIso(d), function(r) {
    cal.dayWindows = r.windows;
    $('#calDayHoliday').text(r.holiday).prop('hidden', !r.holiday);
    $('#calDaySun').text('Sunset ' + calTime(r.sunset) + ' · Dusk ' + calTime(r.dusk));
    var span = r.axisEnd - r.axisStart;
    var hours = '';
    for (var m = r.axisStart; m <= r.axisEnd; m += 60) {
      var edge = m == r.axisStart ? 'first' : m == r.axisEnd ? 'last' : '';
      hours += '<span class="' + edge + '" style="left:' + ((m - r.axisStart) / span * 100).toFixed(2) + '%">' + xsEscape(calTime(m).replace(':00', '')) + '</span>';
    }
    var now = new Date();
    var nowMin = (calIso(now) == calIso(d)) ? now.getHours() * 60 + now.getMinutes() : -1;
    var nowLine = nowMin >= r.axisStart && nowMin <= r.axisEnd ? '<span class="xs-cal-now" style="left:' + ((nowMin - r.axisStart) / span * 100).toFixed(2) + '%"></span>' : '';
    var lights = r.onTheLights.map(function(g) {
      var w = r.windows[g.window];
      return '<span class="xs-cal-seg st-plays ' + calColor(w.id) + '" style="left:' + ((g.start - r.axisStart) / span * 100).toFixed(2) + '%;width:' + ((g.end - g.start) / span * 100).toFixed(2) + '%" title="' + xsEscape(calName(w.id) + ', ' + calRange(g.start, g.end)) + '">' + xsEscape(calName(w.id)) + '</span>';
    }).join('');
    var rows = '<div class="xs-cal-row"><span class="lbl"></span><span class="xs-cal-hours">' + hours + '</span></div>' +
      '<div class="xs-cal-row"><span class="lbl"><b>On the lights</b></span><span class="xs-cal-track lights">' + lights + nowLine + '</span></div>';
    rows += r.lanes.map(function(lane) {
      var w = r.windows[lane.window];
      var s = cal.schedules[w.id] || {};
      var from = w.night != calIso(d) ? ' (from ' + calParse(w.night).toLocaleDateString([], { weekday: 'short' }) + ')' : '';
      return '<div class="xs-cal-row"><span class="lbl"><span class="xs-cal-dot ' + calColor(w.id) + '"></span><span class="nm">' + xsEscape(s.name + from) + '<small>' + xsEscape(s.playlist + ' · priority ' + s.priority) + '</small></span></span>' +
        '<span class="xs-cal-track">' + calLane(w, lane, r.axisStart, span) + '</span></div>';
    }).join('');
    $('#calStrip').html(r.windows.length ? rows : '<p class="xs-empty">Nothing is scheduled this night.</p>');

    var agenda = r.onTheLights.map(function(g) {
      var w = r.windows[g.window];
      return '<li class="xs-cal-ag"><span class="bar ' + calColor(w.id) + '"></span><span class="nm"><b>' + xsEscape(calName(w.id)) + '</b><small>' + xsEscape(calRange(g.start, g.end)) + '</small></span></li>';
    }).join('');
    var notes = r.windows.map(function(w, i) {
      var lane = r.lanes[i];
      var text = '';
      if (w.skipped) text = 'Skipped tonight (skip date)';
      else if (!w.enabled) text = 'Inactive';
      else {
        var clash = lane.segments.filter(function(g) { return g.state == 'clash'; })[0];
        var hidden = lane.segments.filter(function(g) { return g.state == 'hidden'; })[0];
        if (clash) text = 'Clash: same priority as ' + calName(r.windows[clash.by].id) + ' from ' + calTime(clash.start);
        else if (hidden) text = 'Waits while ' + calName(r.windows[hidden.by].id) + ' plays (higher priority)';
        else if (w.playEnd < w.end) text = 'Playlist finishes at ' + calTime(w.playEnd) + ', before its schedule ends';
      }
      return text ? '<li class="xs-cal-ag"><span class="xs-cal-dot ' + calColor(w.id) + '"></span><span class="nm"><b>' + xsEscape(calName(w.id)) + '</b><small' + (text.indexOf('Clash') == 0 ? ' class="bad"' : '') + '>' + xsEscape(text) + '</small></span></li>' : '';
    }).join('');
    $('#calAgenda').html('<div class="xs-narrow-only"><h3 class="xs-cal-sub">On the lights</h3><ul class="xs-list">' + (agenda || '<li class="xs-empty">Nothing plays this night.</li>') + '</ul></div>' +
      (notes ? '<h3 class="xs-cal-sub">Also scheduled</h3><ul class="xs-list">' + notes + '</ul>' : ''));
  });
}

function calLoadTonight() {
  var today = cal.today;
  $('#calTonightDate').text(today.toLocaleDateString([], { weekday: 'short', month: 'short', day: 'numeric' }));
  calQuery('GetCalendarDay', calIso(today), function(r) {
    var now = new Date();
    var nowMin = now.getHours() * 60 + now.getMinutes();
    $('#calTonight').html(r.onTheLights.map(function(g) {
      var w = r.windows[g.window];
      var playing = nowMin >= g.start && nowMin < g.end ? '<span class="xs-pill p-ok"><span class="dot"></span>Playing</span>' : '';
      return '<li class="xs-cal-ag"><span class="bar ' + calColor(w.id) + '"></span><span class="nm"><b>' + xsEscape(calName(w.id)) + '</b><small>' + xsEscape(calRange(g.start, g.end)) + '</small></span>' + playing + '</li>';
    }).join('') || '<li class="xs-empty">Nothing plays tonight.</li>');
    $('#calSchedules').html(Object.keys(cal.schedules).map(function(k) { return cal.schedules[k]; }).sort(function(a, b) { return a.index - b.index; }).map(function(s) {
      return '<li class="xs-cal-ag' + (s.enabled ? '' : ' off') + '"><span class="xs-cal-dot ' + calColor(s.id) + '"></span><span class="nm"><b>' + xsEscape(s.name) + '</b><small>' + xsEscape(s.playlist + (s.enabled ? '' : ' · inactive')) + '</small></span><span class="xs-cal-pri">P' + s.priority + '</span></li>';
    }).join('') || '<li class="xs-empty">No schedules yet.</li>');
  });
}

function calCheckSeason() {
  var from = $('#calFrom').val(), to = $('#calTo').val();
  if (!from || !to || to < from) return;
  calQuery('GetCalendar', from + ',' + to, function(r) {
    var empty = [], clashes = [], skipped = [];
    var run = null;
    var endRun = function() {
      if (run) empty.push(run.n == 1 ? run.first : run.first + ' – ' + run.last + ' (' + run.n + ' nights)');
      run = null;
    };
    $('#calNights').html(r.days.map(function(day) {
      var plays = day.windows.filter(function(w) { return w.enabled && !w.skipped; }).length;
      var d = calParse(day.date);
      var label = d.toLocaleDateString([], { weekday: 'short', month: 'short', day: 'numeric' });
      if (!plays) {
        if (run) { run.last = label; run.n++; } else run = { first: label, last: label, n: 1 };
      } else endRun();
      day.clashes.forEach(function(c) { clashes.push(label + ', ' + calRange(c.start, c.end) + ': ' + calName(c.a) + ' and ' + calName(c.b) + ' (priority ' + (cal.schedules[c.a] || {}).priority + ')'); });
      day.windows.forEach(function(w) { if (w.skipped) skipped.push(label + ': ' + calName(w.id)); });
      var cls = !plays ? 'none' : day.clashes.length ? 'clash' : day.holiday ? 'hol' : '';
      return '<span class="' + cls + '" title="' + xsEscape(label + (day.holiday ? ', ' + day.holiday : '') + ': ' + (plays ? plays + ' scheduled' : 'no show') + (day.clashes.length ? ', clash' : '')) + '"></span>';
    }).join(''));
    endRun();
    var list = function(title, items, pill) {
      var shown = items.slice(0, 10).map(function(t) { return '<li>' + xsEscape(t) + '</li>'; }).join('');
      if (items.length > 10) shown += '<li class="more">and ' + (items.length - 10) + ' more</li>';
      return '<h3 class="xs-cal-sub"><span class="xs-pill ' + pill + '">' + items.length + '</span> ' + title + '</h3>' +
        (items.length ? '<ul class="xs-cal-find">' + shown + '</ul>' : '');
    };
    $('#calFindings').html(list('Nights with no show', empty, empty.length ? 'p-bad' : 'p-off') + list('Clashes: same priority, overlapping', clashes, clashes.length ? 'p-bad' : 'p-off') + list('Skipped nights', skipped, 'p-off'));
  });
}

// the season: from the first night with a show to the last, looking a year ahead
function calFindSeason() {
  var end = new Date(cal.today.getFullYear() + 1, cal.today.getMonth(), cal.today.getDate());
  $.ajax({
    url: '/xScheduleQuery?Query=GetCalendar&Parameters=' + encodeURIComponent(calIso(cal.today) + ',' + calIso(end)),
    dataType: 'json',
    success: function(r) {
      var nights = (r.days || []).filter(function(day) { return day.windows.some(function(w) { return w.enabled && !w.skipped; }); });
      if (nights.length) {
        $('#calFrom').val(nights[0].date);
        $('#calTo').val(nights[nights.length - 1].date);
      }
      calCheckSeason();
    }
  });
}

function calSelect(d) {
  cal.selected = d;
  if (d.getMonth() != cal.month.getMonth() || d.getFullYear() != cal.month.getFullYear()) {
    cal.month = new Date(d.getFullYear(), d.getMonth(), 1);
  }
  calLoadMonth();
  calLoadDay();
}

$(document).ready(function() {
  var now = new Date();
  cal.today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  cal.month = new Date(now.getFullYear(), now.getMonth(), 1);
  cal.selected = cal.today;
  try { cal.inactive = localStorage.getItem('xsCalInactive') == '1'; } catch (e) {}
  $('#calInactive').prop('checked', cal.inactive);

  var names = [];
  for (var i = 0; i < 7; i++) names.push(new Date(2023, 0, 1 + (calFirstDay() + i) % 7).toLocaleDateString([], { weekday: 'short' }));
  $('#calWeekdays').html(names.map(function(n) { return '<span>' + xsEscape(n) + '</span>'; }).join(''));

  $('#calPrev').on('click', function() { cal.month = new Date(cal.month.getFullYear(), cal.month.getMonth() - 1, 1); calLoadMonth(); });
  $('#calNext').on('click', function() { cal.month = new Date(cal.month.getFullYear(), cal.month.getMonth() + 1, 1); calLoadMonth(); });
  $('#calToday').on('click', function() { calSelect(cal.today); });
  $('#calGrid').on('click', '.xs-cal-cell', function() { calSelect(calParse($(this).attr('data-date'))); });
  $('#calInactive').on('change', function() {
    cal.inactive = this.checked;
    try { localStorage.setItem('xsCalInactive', cal.inactive ? '1' : '0'); } catch (e) {}
    calLoadMonth();
    calLoadDay();
  });
  $('#calCheck').on('click', calCheckSeason);
  var seasonEnd = new Date(now.getFullYear(), now.getMonth() + 2, 0);
  $('#calFrom').val(calIso(cal.today));
  $('#calTo').val(calIso(seasonEnd));

  var dayParam = getQueryVariable('args');
  if (dayParam && /^\d{4}-\d{2}-\d{2}$/.test(dayParam)) calSelect(calParse(dayParam)); else calSelect(cal.today);
  calLoadTonight();
  calFindSeason();

  // the "now" line and Tonight's playing badge move with the clock; a new schedule or edit shows within a minute
  xsPageTimers.push(setInterval(function() {
    var n = new Date();
    if (calIso(n) != calIso(cal.today)) cal.today = new Date(n.getFullYear(), n.getMonth(), n.getDate());
    calLoadMonth();
    calLoadDay();
    calLoadTonight();
  }, 60000));
});
