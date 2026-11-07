// Create WebSocket connection.
url = (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + location.hostname + (location.port ? ':' + location.port : '');
const socket = new ReconnectingWebSocket(url);
// Connection opened
socket.addEventListener('open', function(event) {
  console.log("Socket Opened");
  $('#connectionChip').attr('hidden', true);
});

socket.addEventListener('message', function(event) {
  var response = JSON.parse(event.data);

  if (response.result == 'not logged in') {
    window.location.href = "login.html";
  }
  if (response.result == 'failed' && response.message == 'Login failed.') {
    window.location.href = "login.html";
  }

  if (response.score == undefined && response.highscore == undefined) {
    if (response.status != undefined) {
      playingStatus = response;
      updateNavStatus();
      callRegisteredForStatus(response);
    }

    if (response.result == 'failed') {
      notification('Failed: ' + response.message, 'danger', '0');
    }
  }

  //Run Fuction if refrence is set
  if (response.reference != "" && response.reference != undefined) {
    var fun = response.reference;
    var fn = window[fun];
    if (typeof fn === "function") {
      fn(response);
    } else {
      notification('Unknown Reference Function: "' + response.reference + '"', 'danger', '0');
    }
  }
});

socket.onclose = function(e) {
  $('#connectionChip').removeAttr('hidden');
  notification('Web Socket: Disconnected "' + e + '"', 'danger', '2');
};

function test(response) {
  notification('Test Function: "' + response + '"', 'success', '0');
}
