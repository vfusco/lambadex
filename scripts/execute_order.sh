#!/bin/bash

ORDER_JSON=$1
MEMORY_RANGE_UTIL=$2
INPUT_BOX_ADDRESS=$3
DAPP_ADDRESS=$4
SENDER_KEY=$5

GAS_LIMIT=10000000

ORDER_JSON=$(echo "$ORDER_JSON"  | tr -d '\n' | tr -d ' ')
payload_bin=$(echo "$ORDER_JSON"  | tr -d '\n' | $MEMORY_RANGE_UTIL encode lambadex-new-order-input | dd bs=64 skip=1 status=none)
payload_hex=$(echo -n $payload_hex | xxd -p | tr -d '\n' | tr -d ' ')

echo "Executing order: $ORDER_JSON"
cast send $INPUT_BOX_ADDRESS "addInput(address,bytes)" $DAPP_ADDRESS "0x$payload_hex" \
    --private-key="$SENDER_KEY" --gas-limit=$GAS_LIMIT > /dev/null
