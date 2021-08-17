# mediasoup broadcaster demo (libmediasoupclient v3)
publish stream to SRS server

## Resources
* mediasoup website and documentation: [mediasoup.org](https://mediasoup.org)
* mediasoup support forum: [mediasoup.discourse.group](https://mediasoup.discourse.group)

## Usage
Once installed (see **Installation** below):
```bash
SERVER_URL=http://d.ossrs.net:1985/rtc/v1/publish/ STREAM_ID=broadcaster build/broadcaster
```

Environment variables:

* `SERVER_URL`: The URL of the mediasoup-demo HTTP API server (default: http://d.ossrs.net:1985/rtc/v1/publish/).
* `STREAM_ID`: Room id (default: broadcaster).

## Dependencies

* [libmediasoupclient][libmediasoupclient] (already included in the repository, not used!)
* [cpr][cpr] (already included in the repository)
* OpenSSL (must be installed in the system including its headers)

## Installation
```bash
git clone https://github.com/armersong/mediasoup-broadcaster-demo.git
```
- change WEBRTC and LIBWEBRTC_BINARY_PATH in build.sh
```bash
./build.sh
make -C build -j4
```

## Play
open: [http://ossrs.net/players/rtc_player.html?vhost=d.ossrs.net&server=d.ossrs.net&port=1985&autostart=true](http://ossrs.net/players/rtc_player.html?vhost=d.ossrs.net&server=d.ossrs.net&port=1985&autostart=true)
url: ```webrtc://d.ossrs.net/live/broadcaster```

#### Linkage Considerations (1)

```
[ 65%] Linking C shared library ../../../../lib/libcurl.dylib ld: cannot link directly with dylib/framework, your binary is not an allowed client of /usr/lib/libcrypto.dylib for architecture x86_64 clang: error: linker command failed with exit code 1 (use -v to see invocation)
make[2]: *** [lib/libcurl.dylib] Error 1 make[1]: *** [cpr/opt/curl/lib/CMakeFiles/libcurl.dir/all] Error 2
make: *** [all] Error 2
```

The following error may happen if the linker is not able to find the openssl crypto library. In order to avoid this error, specify the crypto library path along with the openssl root directory using the `OPENSSL_CRYPTO_LIBRARY` flag. Ie:

```
-DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl@1.1/1.1.1h \
-DOPENSSL_CRYPTO_LIBRARY=/usr/local/Cellar/openssl@1.1/1.1.1h/lib/libcrypto.1.1.dylib
```



## License

Some files contain specific license agreements, written in the beginning of the respective files.

*NOTE 1:* `PATH_TO_OPENSSL_HEADERS` is `/usr/local/opt/openssl/include` if you install OpenSSL using Homebrew in OSX.

[mediasoup-demo]: https://github.com/versatica/mediasoup-demo
[libmediasoupclient]: https://github.com/versatica/libmediasoupclient
[cpr]: https://github.com/whoshuu/cpr
