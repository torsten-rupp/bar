#!/bin/sh

if test ! -f `dirname $0`/barserver-dummy.awk; then
  echo >&2 "ERROR: `dirname $0`/barserver-dummy.awk not found"
  exit 1
fi

echo "Start BARControl with: localhost --port=10000 --no-tls --geometry 840x695 --debug-fake-tls"
socat TCP4-LISTEN:10000,reuseaddr SYSTEM:"awk -f `dirname $0`/barserver-dummy.awk",pty,echo=0
