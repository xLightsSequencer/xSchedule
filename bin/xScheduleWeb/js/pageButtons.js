$(document).ready(function() {
  $.ajax({
    url: '/xScheduleQuery?Query=GetButtons',
    dataType: "json",
    success: function(response) {
      var buttons = response.buttons || [];
      $('#buttonGrid').html(buttons.map(xsUserButton).join('') || '<p class="xs-empty">No buttons are set up yet.</p>');
      var guests = buttons.some(function(b) { return String(b.label).indexOf('GB_') == 0; });
      if (guests) {
        var page = location.protocol + '//' + location.host + location.pathname.replace(/[^\/]*$/, '') + 'guestbuttons.html';
        $('#guestNote').removeAttr('hidden').html('Guest buttons are also on the visitor page: <a href="' + xsEscape(page) + '" target="_blank" rel="noopener">' + xsEscape(page) + '</a>');
      }
    }
  });
});
