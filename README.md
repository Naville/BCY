# BCY++

# Building
## Requirements
- ``OpenSSL``
- ``Boost 1.67+``
- ``curl``

Note that on some systems you need the development package of these

## Optional
Edit ``cpr/cpr/session.cpp``, inside ``Session::Impl::Impl()`` add ``curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); `` during initializing the ``CURL*`` Handle
