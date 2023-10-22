#!/bin/bash

RPC_URL="http://localhost:8545"
MASTER_MNEMONIC="test test test test test test test test test test test junk"
INIT_SUPPLY=100000000000000000000

addresses=""
for file in ./contracts/*.sol; do
    if [ -f "$file" ]; then
        base_filename=$(basename -- "$file" .sol)
        output=$(forge create "$file:$base_filename" --rpc-url $RPC_URL --mnemonic="$MASTER_MNEMONIC" --constructor-args $INIT_SUPPLY)
        deployed_to=$(echo "$output" | grep -o 'Deployed to: [^ ]*' | cut -d ' ' -f 3)
        addresses="$addresses $base_filename:$deployed_to"
    fi
done

echo $addresses
