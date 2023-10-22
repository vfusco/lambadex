#!/bin/bash

ORDER_JSON=$1
MEMORY_RANGE_UTIL=$2
INPUT_BOX_ADDRESS=$3
DAPP_ADDRESS=$4
SENDER_KEY=$5

GAS_LIMIT=10000000

payload_json="{\"symbol\":\"$SYMBOL\",\"side\":\"buy\",\"price\":$PRICE,\"quantity\":$QUANTITY}"
payload_bin=$(echo $payload_json | $MEMORY_RANGE_UTIL encode lambadex-new-order-input | dd bs=64 skip=1 status=none)
payload_hex=$(echo -n $payload_hex | xxd -p | tr -d '\n' | tr -d ' ')

cast send $INPUT_BOX_ADDRESS "addInput(address,bytes)" $DAPP_ADDRESS "0x$payload_hex" --mnemonic="$SENDER_KEY" --gas-limit=$GAS_LIMIT > /dev/null
