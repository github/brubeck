#!/bin/sh

### BEGIN INIT INFO
# Provides:          brubeckdaemon
# Required-Start:    $local_fs $network $syslog
# Required-Stop:     $local_fs $network $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Example
# Description:       Example start-stop-daemon - Debian
### END INIT INFO

NAME="brubeck-tech"
PIDFILE="/var/run/$NAME.pid"
APPDIR="/usr/local/bin"
APPBIN="/usr/local/bin/brubeck"
APPARGS="--config=/etc/brubeck/tech.json &> /var/log/brubeck/tech.txt"
LOGFILE="/var/log/brubeck/tech.log"

# Include functions
. /lib/lsb/init-functions

start() {
    if [ -e $PIDFILE ]; then
        status_of_proc -p ${PIDFILE} ${APPBIN} "${NAME} process" && status="0" || status="$?"
        if [ $status = "0" ]; then
            echo "Nothing to do."
            exit
        fi
    fi
    echo "Starting ${NAME} process"
    cd ${APPDIR}
    ${APPBIN} ${APPARGS} >> ${LOGFILE} 2>&1 &
    echo $! > ${PIDFILE}
    echo "Done."
}
stop() {
    if [ -e $PIDFILE ]; then
        status_of_proc -p ${PIDFILE} ${APPBIN} "${NAME} process" && status="0" || status="$?"
        if [ $status = "0" ]; then
            echo "Stopping ${NAME}."
            start-stop-daemon --stop --quiet --oknodo --pidfile ${PIDFILE}
            rm -rf ${PIDFILE}
        fi
    else
        echo "${NAME} is not running, nothing to do."
    fi
}


status(){
    if [ -e ${PIDFILE} ]; then
        status_of_proc -p ${PIDFILE} ${APPBIN} "${NAME} process" && status="0" || status="$?"
        if [ $status = "0" ]; then
            exit 0
        fi
    else
        echo "${NAME} process is not running"
    fi
}



case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        sleep 2
        start
        ;;
    status)
        status
        ;;
    *)
        echo $"Usage: $0 {start|stop|restart|reload|condrestart|status}"
esac