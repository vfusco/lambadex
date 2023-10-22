#!/bin/bash

SCRIPTS_DIR=$1
MEMORY_RANGE_UTIL=$2
INPUT_BOX_ADDRESS=$3
DAPP_ADDRESS=$4

DATA_FILE=$SCRIPTS_DIR/trades.data
ORDER_FREQUENCY=1 # seconds

while IFS= read -r line; do
    order_data=$(echo "$line" | jq 'del(.user)')
    $SCRIPTS_DIR/execute_order.sh \
        "$order_data" \
        $MEMORY_RANGE_UTIL \
        $INPUT_BOX_ADDRESS \
        $DAPP_ADDRESS \
        $sender_key
    sleep $ORDER_FREQUENCY
done < "$DATA_FILE"
