#!/bin/sh

echo "Start BAR dummy server on port 10000..."
echo "Start BARControl with: localhost --port=10000 --no-tls --geometry 840x695
socat TCP4-LISTEN:10000,reuseaddr SYSTEM:'awk -f barserver-dummy.awk',pty,echo=0
