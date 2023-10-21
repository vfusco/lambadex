#!/bin/bash

SYMBOL=$1
PRICE=$2
QUANTITY=$3
MEMORY_RANGE_UTIL=$4
INPUT_BOX_ADDRESS=$5
DAPP_ADDRESS=$6
SENDER_KEY=$7

GAS_LIMIT=10000000

payload_json="{\"symbol\":\"$SYMBOL\",\"side\":\"buy\",\"price\":$PRICE,\"quantity\":$QUANTITY}"
payload_bin=$(echo $payload_json | $MEMORY_RANGE_UTIL encode lambadex-new-order-input | dd bs=64 skip=1 status=none)
payload_hex=$(echo -n $payload_hex | xxd -p)

cast send $INPUT_BOX_ADDRESS "addInput(address,bytes)" $DAPP_ADDRESS "0x$payload_hex" --private-key="$SENDER_KEY" --gas-limit=$GAS_LIMIT > /dev/null
