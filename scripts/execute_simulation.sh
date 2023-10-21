#!/bin/bash

USERS_AMOUNT=5
MINT_AMOUNT=1000000000000000000
OUT_FILE=wallets.json
MEMORY_RANGE_UTIL=$(dirname "$0")/../misc/lambadex-memory-range.lua


# extract contracts addresses
echo "-> Getting token contracts addresses..."
SUNODO_ADDRESS_BOOK=$(sunodo address-book)
SUNODO_TOKEN_ADDRESS=$(echo "$SUNODO_ADDRESS_BOOK" | grep "SunodoToken" | awk '{print $2}')
DAPP_ADDRESS=$(echo "$SUNODO_ADDRESS_BOOK" | grep -E "CartesiDApp\s+" | awk '{print $2}')
ERC20_PORTAL_ADDRESS=$(echo "$SUNODO_ADDRESS_BOOK" | grep "ERC20Portal" | awk '{print $2}')
echo "SunodoToken: $SUNODO_TOKEN_ADDRESS"
echo "DApp: $DAPP_ADDRESS"
echo "ERC20Portal: $ERC20_PORTAL_ADDRESS"

TOKENS="$SUNODO_TOKEN_ADDRESS"

# create test users and mint tokens for them
./$(dirname "$0")/create_users.sh \
    $USERS_AMOUNT \
    $MINT_AMOUNT \
    $OUT_FILE \
    $TOKENS

# deposit all tokens to the dapp
./$(dirname "$0")/deposit.sh \
    $DAPP_ADDRESS \
    $ERC20_PORTAL_ADDRESS \
    $MINT_AMOUNT \
    $OUT_FILE \
    $USERS_AMOUNT \
    $MEMORY_RANGE_UTIL \
    $TOKENS
