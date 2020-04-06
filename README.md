# BCY++
Library to emulate [BCY](https://bcy.net/)'s iOS client and interact with its service. No more reCAPTCHA and other horseshit.  

# Building
**PyBCY has been long deprecated in favor of this package**
## Requirements
- Unix-like environment.
  - Windows Native Environment won't work due to character encoding issues.
  - WSL won't work due to M$'s [crappy implementation of ``fcntl(F_SETLK)``](https://github.com/microsoft/WSL/issues/2395).
- A C++17 compatible compiler.
- ``Boost 1.67+``
- ``cpprestsdk 8+`` which also depends on [OpenSSL](https://www.openssl.org)
- [cryptopp](https://github.com/weidai11/cryptopp) Compiled from source using [cryptopp-cmake](https://github.com/noloader/cryptopp-cmake) is required

Note that on some systems you need the development package of these
