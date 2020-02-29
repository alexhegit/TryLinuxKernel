#!/bin/sh

TPATH="/sys/kernel/debug/tracing"
FILTER=$TPATH/set_ftrace_filter
TOUTPUT="/tmp/qeexo-krp.log"
TFUNC=$2

trace_start() {
    echo 0 > ${TPATH}/tracing_on
    echo 0 > ${TPATH}/trace
    echo > ${TPATH}/set_ftrace_filter
    #echo 'SyS_nanosleep hrtimer_interrupt sys_getpid' > ${TPATH}/set_ftrace_filter
    echo $TFUNC > ${TPATH}/set_ftrace_filter
    echo function_graph > ${TPATH}/current_tracer
    echo nofuncgraph-overhead > ${TPATH}/trace_options
    echo nofuncgraph-cpu > ${TPATH}/trace_options
    echo noprint-parent > ${TPATH}/trace_options
    echo 1 > ${TPATH}/tracing_on
    sleep 1
}

trace_stop() {
    echo 0 > ${TPATH}/tracing_on
    cat ${TPATH}/trace > ${TOUTPUT}
}

trace_show() {
    echo "<symbo name>:<avage time per call in ns>:<invocations>"
    echo "======================================================"
    cat $FILTER | while read line
    do
        echo -n $line:
        #grep $line $TOUTPUT | grep " us " | grep " | "
        grep $line $TOUTPUT | grep " us " | grep " | " | awk 'BEGIN {sum=0;} {sum=sum+$1;} END {printf("%d:%d\n", sum/NR*1000, NR)}'
    done
}

case "$1" in
    "start")
        trace_start
        exit 0
        ;;
    "stop")
        trace_stop
        exit 0
        ;;
    "show")
        trace_show
        exit 0
        ;;
    *)
        echo "Usage:"
        echo "Start trace functions:"
        echo "$0 start 'func1 [func2] [func3] ...'"
        echo "Stop trace:"
        echo "$0 stop"
        echo "Show trace result:"
        echo "$0 show"
esac