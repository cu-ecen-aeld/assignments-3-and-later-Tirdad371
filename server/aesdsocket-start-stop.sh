#!/bin/sh
# System V init script for aesdsocket daemon
# Placed in /etc/init.d/S99aesdsocket on the target

DAEMON=/usr/bin/aesdsocket
DAEMON_NAME=aesdsocket

case "$1" in
    start)
        echo "Starting $DAEMON_NAME"
        start-stop-daemon --start --name $DAEMON_NAME \
            --exec $DAEMON -- -d
        ;;
    stop)
        echo "Stopping $DAEMON_NAME"
        start-stop-daemon --stop --name $DAEMON_NAME \
            --signal TERM
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
        ;;
esac

exit 0
