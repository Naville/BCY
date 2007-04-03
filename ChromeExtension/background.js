function appendMessage(text) {
    chrome.tabs.executeScript({
        code: "console.log('" + text + "');"
    });
}
chrome.browserAction.onClicked.addListener(function(tab) {
            PageURL = tab.url
            if (PageURL.indexOf("bcy.net") !== -1) {
                chrome.storage.sync.get({
                  IP: "127.0.0.1",
                  Port: "8081"
                    }, function(items) {
                        Args = {
                            "URL": PageURL,
                        };
                        PostCode = `var xmlhttp = new XMLHttpRequest();xmlhttp.open("POST", "http://`+items.IP+':'+items.Port+`");xmlhttp.setRequestHeader("Content-Type", "text/plain;charset=UTF-8");xmlhttp.send('`+JSON.stringify(Args)+`');`;
                        chrome.tabs.executeScript({
                                code: PostCode
                            });
                        });
                }
            });
