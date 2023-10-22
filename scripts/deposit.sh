#!/bin/bash

DAPP_ADDRESS=$1
ERC20_PORTAL_ADDRESS=$2
DEPOSIT_AMOUNT=$3
OUT_FILE=$4
NUM_WALLETS=$5
MEMORY_RANGE_UTIL=$6
shift 6

GAS_LIMIT=10000000


echo "-> Depositing tokens..."
user_count=0
for user in $(jq -r 'keys[]' "$OUT_FILE"); do
    address=$(jq -r ".\"$user\".Address" "$OUT_FILE")
    key=$(jq -r ".\"$user\".Mnemonic" "$OUT_FILE")
    for token_address in "$@"; do
        payload_json="{\"status\":\"successful\",\"sender\":\"$address\",\"token\":\"$token_address\",\"amount\":\"$DEPOSIT_AMOUNT\"}"
        payload_bin=$(echo $payload_json | $MEMORY_RANGE_UTIL encode erc20-deposit-input | dd bs=64 skip=1 status=none)
        payload_hex=$(echo -n $payload_bin | xxd -p | tr -d '\n' | tr -d ' ')
        cast send $token_address "approve(address, uint256)(bool)" $ERC20_PORTAL_ADDRESS $DEPOSIT_AMOUNT --mnemonic="$key" #> /dev/null
        cast send $ERC20_PORTAL_ADDRESS "depositERC20Tokens(address, address, uint256, bytes)" \
            $token_address $DAPP_ADDRESS $DEPOSIT_AMOUNT "0x$payload_hex" --mnemonic="$key" --gas-limit=$GAS_LIMIT #> /dev/null
    done
    ((user_count++))
    echo "$user deposited successfully [$user_count/$NUM_WALLETS]"
done
