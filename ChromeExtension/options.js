function save_options() {
  var ip = document.getElementById('ip').value;
  var port = document.getElementById('port').value;
  chrome.storage.sync.set({
    IP: ip,
    Port: port
  }, function() {
    // Update status to let user know options were saved.
    var status = document.getElementById('status');
    status.textContent = 'Options saved.';
    setTimeout(function() {
      status.textContent = '';
    }, 750);
  });
}

// Restores select box and checkbox state using the preferences
// stored in chrome.storage.
function restore_options() {
  // Use default value color = 'red' and likesColor = true.
  chrome.storage.sync.get({
    IP: "127.0.0.1",
    Port: "8081"
  }, function(items) {
    document.getElementById('ip').value = items.IP;
    document.getElementById('port').value = items.Port;
  });
}
document.addEventListener('DOMContentLoaded', restore_options);
document.getElementById('save').addEventListener('click',
    save_options);
