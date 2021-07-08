#!/bin/sh

while true; do
    ./pipemon ssh -v \
	      -o ExitOnForwardFailure=yes \
	      -o Compression=no \
	      -o ServerAliveCountMax=0 \
              -o ServerAliveInterval=0 \
              -o TCPKeepAlive=no \
              -o ForwardX11=no \
              -o ForwardX11Trusted=no \
	      -R 0.0.0.0:12345:localhost:22 \
              -D 0.0.0.0:8080 \
	      192.168.177.1 \
	      cat
    sleep 5
done
