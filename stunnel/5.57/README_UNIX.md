# Unix Build Instructions

## Build wolfSSL
+ Configure wolfSSL with `./configure --enable-stunnel`. Add `--enable-debug` if you want to enable the debug version of wolfSSL.
+ Compile with `make`.
+ Install wolfSSL into /usr/local with `sudo make install`.

## Build stunnel
+ Download stunnel 5.57 with `curl -O https://www.usenix.org.uk/mirrors/stunnel/archive/5.x/stunnel-5.57.tar.gz`.
+ Unarchive stunnel-5.57.tar.gz with `tar xvf stunnel-5.57.tar.gz`. cd into stunnel-5.57.
+ Patch the source code with `patch -p1 < stunnel-5.57.patch`, adjusting the path to the patch file accordingly.
+ Regenerate the configure script with `autoreconf`.
+ Configure stunnel with `./configure --enable-wolfssl`. Add `--enable-wolfssldebug` if you want to enable wolfSSL debug logging during stunnel operation. This requires that you configured wolfSSL with `--enable-debug`.
+ Compile with `make`.
+ Install stunnel into /usr/local with `sudo make install`.

## Try It Out
To verify that stunnel built with wolfSSL is working, we'll use an example TCP server and client from the wolfSSL examples repository.

+ In a directory containing the stunnel-client.conf and stunnel-server.conf files, clone the wolfSSL examples repository with `git clone git@github.com:wolfSSL/wolfssl-examples.git`.
+ cd into wolfssl-examples/tls/. Edit server-tcp.c to change the line `#define DEFAULT_PORT 11111` to `#define DEFAULT_PORT 11112`.
+ Compile the TCP client and server with `make`. The server executable is server-tcp and the client is client-tcp.
+ cd back into the main directory containing the .conf files. Edit stunnel-server.conf. Set the port on the "connect" line to 11112. Set the port on the "accept line to 11113. Replace <path to server certificate, in PEM format> with the path to wolfssl/certs/server-cert.pem. Replace <path to server private key, in PEM format> with the path to wolfssl/certs/server-key.pem. Replace <path to log file> with the path to a file where logs will be kept. Something like `/tmp/stunnel-server.log`.
+ Edit stunnel-client.conf. Set the port on the "accept" line to 11111 as this is the port to which will connect. Set the port on the "connect" line matches the "accept" port from stunnel-server.conf (i.e.: 11113). Replace <path to CA certificate, in PEM format> with the path to wolfssl/certs/ca-cert.pem. Replace <path to log file> with the path to a file where logs will be kept. Something like `/tmp/stunnel-client.log`.
+ Fire up the stunnel server with `stunnel stunnel-server.conf`.
+ Launch the stunnel client with `stunnel stunnel-client.conf`. We now have a secure tunnel provided by stunnel and backed by wolfSSL.
+ Launch the TCP server, which will communicate with the TCP client via the tunnel, with `./wolfssl-examples/tls/server-tcp`.
+ In another terminal window, launch the TCP client with `./wolfssl-examples/tls/client-tcp 127.0.0.1`. Enter a message to send to the server. You should see the server respond with "I hear ya fa shizzle!", and you should see the message you sent appear in the server window. Additionally, you should see a flurry of activity in the stunnel server and client logs when the message is sent. If you were to observe this traffic in Wireshark, you would see that the message is encrypted using TLS, indicating that the tunnel is working as intended.

## Clean Up
+ To get the server to terminate execution, use the client to send "shutdown".
+ To get the stunnel processes to terminate execution, send the kill signal to the processes. Execute the following command: `ps -A | grep stunnel`. You should see two lines beginning with large numbers. Execute `kill <number>` using the numbers from the previous command.

# stunnel with Experimental Post-Quantum Algorithms

## Build liboqs and OpenQuantumSafe's OpenSSL Fork.
See wolfSSL's INSTALL file for instructions on how to build and install liboqs and OpenQuantumSafe's OpenSSL fork. Also perform the steps for generating the FALCON certificate chain.

## Build wolfSSL and stunnel
Same as instructions at the beginning of this file, but add `--with-liboqs` to the configure command for wolfSSL.

##Try It Out
First, follow all the instructions in "Try It Out" and "Clean Up" above as a sanity check.

In a text editor, open `stunnel-server.conf` and change the `cert` line to point to the location of the FALCON entity certificate that you generated (`falcon_level5_entity_cert.pem`). Change the `key` line to point to the location of the FALCON entity private key that you generated (`falcon_level5_entity_key.pem`). Add the following line after the `key` line:

```
ciphersuites = TLS_AES_256_GCM_SHA384
```

Save the file.

In a text editor, open `stunnel-client.conf` and change the `CAfile` line to point to the location of the FALCON root certificate that you generated (`falcon_level5_root_cert.pem`). Add the following line after the `CAfile` line:

```
ciphersuites = TLS_AES_256_GCM_SHA384
```

Set the `WOLFSSL_PQ_GROUP` environment variable to a valid post-quantum group name:

```
export WOLFSSL_PQ_GROUP <pqc_group>
```

Where `<pqc_group>` is one of:

`NTRU_HPS_LEVEL1`
`NTRU_HPS_LEVEL3`
`NTRU_HPS_LEVEL5`
`NTRU_HRSS_LEVEL3`
`SABER_LEVEL1`
`SABER_LEVEL3`
`SABER_LEVEL5`
`KYBER_LEVEL1`
`KYBER_LEVEL3`
`KYBER_LEVEL5`
`KYBER_90S_LEVEL1`
`KYBER_90S_LEVEL3`
`KYBER_90S_LEVEL5`
`P256_NTRU_HPS_LEVEL1`
`P384_NTRU_HPS_LEVEL3`
`P521_NTRU_HPS_LEVEL5`
`P384_NTRU_HRSS_LEVEL3`
`P256_SABER_LEVEL1`
`P384_SABER_LEVEL3`
`P521_SABER_LEVEL5`
`P256_KYBER_LEVEL1`
`P384_KYBER_LEVEL3`
`P521_KYBER_LEVEL5`
`P256_KYBER_90S_LEVEL1`
`P384_KYBER_90S_LEVEL3`
`P521_KYBER_90S_LEVEL5`

Rerun the instructions for executing stunnel twice as well as the TCP client and server. Congratulations, your simple TCP connection is protected by a TLS 1.3 tunnel using post-quantum algorithms.
