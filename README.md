# BCY++

# Optional
Edit ``cpr/cpr/session.cpp``, inside ``Session::Impl::Impl()`` add ``curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); `` during initializing the ``CURL*`` Handle

## BCYDownloader
By default the program uses ``cpr``, if the configuration json contains something like this:
```
"aria2":{
  "secret":"4B675A6A",
  "RPCServer":"http://127.0.0.1:6900/jsonrpc"
}
```

then download is handled by aria2's jsonrpc, you need to start arai2c in daemon mode first with something like the following
```
aria2c --dir=/BCY/ --enable-rpc --rpc-listen-port=6900 --rpc-allow-origin-all=true --rpc-secret="4B675A6A" --save-session=/BCY/a2.session --continue=true --auto-file-renaming=false --allow-overwrite=false --input-file=/BCY/a2.session --max-concurrent-downloads=32
```
