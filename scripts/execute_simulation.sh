#!/bin/bash

SCRIPTS_DIR=$(dirname "$0")
PROJECT_DIR=$SCRIPTS_DIR/$(dirname "$0")/../
MEMORY_RANGE_UTIL=$PROJECT_DIR/misc/
TOKEN_CONTRACTS_DIR=$PROJECT_DIR/contracts/

USERS_AMOUNT=1
MINT_AMOUNT=1000000000000000000
OUT_FILE=wallets.json


# deploy token contracts
echo "-> Deploying token contracts..."
deployed_contracts=$(./$SCRIPTS_DIR/deploy_tokens.sh $PROJECT_DIR)

# extract contracts addresses
echo "-> Getting token contracts addresses..."
SUNODO_ADDRESS_BOOK=$(sunodo address-book)
SUNODO_TOKEN_ADDRESS=$(echo "$SUNODO_ADDRESS_BOOK" | grep "SunodoToken" | awk '{print $2}')
DAPP_ADDRESS=$(echo "$SUNODO_ADDRESS_BOOK" | grep -E "CartesiDApp\s+" | awk '{print $2}')
ERC20_PORTAL_ADDRESS=$(echo "$SUNODO_ADDRESS_BOOK" | grep "ERC20Portal" | awk '{print $2}')

echo $deployed_contracts | sed 's/\([a-zA-Z]\+\):\([a-zA-Z0-9]\+\)/\n\1: \2/g'
echo "SunodoToken: $SUNODO_TOKEN_ADDRESS"
echo "DApp: $DAPP_ADDRESS"
echo "ERC20Portal: $ERC20_PORTAL_ADDRESS"

new_tokens=$(echo $deployed_contracts | grep -o '0x[0-9a-fA-F]\+' | awk '{ printf $0 " " }')
TOKENS="$SUNODO_TOKEN_ADDRESS $new_tokens"

# create test users and mint tokens for them
./$SCRIPTS_DIR/create_users.sh \
    $USERS_AMOUNT \
    $MINT_AMOUNT \
    $OUT_FILE \
    $TOKENS

# deposit all tokens to the dapp
#./$SCRIPTS_DIR/deposit.sh \
#    $DAPP_ADDRESS \
#    $ERC20_PORTAL_ADDRESS \
#    $MINT_AMOUNT \
#    $OUT_FILE \
#    $USERS_AMOUNT \
#    $MEMORY_RANGE_UTIL \
#    $TOKENS
