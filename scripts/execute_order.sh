#!/bin/bash

ORDER_JSON=$1
MEMORY_RANGE_UTIL=$2
INPUT_BOX_ADDRESS=$3
DAPP_ADDRESS=$4
SENDER_KEY=$5

GAS_LIMIT=10000000

export LUA_PATH_5_4="../misc/?.lua;/opt/local/share/luarocks/share/lua/5.4/?.lua;;"
ORDER_JSON=$(echo "$ORDER_JSON" | tr -d '\n' | tr -d ' ')
payload_hex=$(echo "$ORDER_JSON" | $MEMORY_RANGE_UTIL encode lambadex-new-order-input |  xxd -p -c 0 -s 64 )

echo "Executing order: $ORDER_JSON"
cast send $INPUT_BOX_ADDRESS "addInput(address,bytes)" $DAPP_ADDRESS "0x$payload_hex" \
    --private-key="$SENDER_KEY" --gas-limit=$GAS_LIMIT > /dev/null
