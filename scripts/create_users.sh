#!/bin/bash

NUM_WALLETS=$1
AMOUNT_TO_MINT=$2
OUT_FILE=$3
shift 3


ADMIN_WALLET_MNEMONIC="test test test test test test test test test test test junk"
GAS_PER_TEST_ACCOUNT=5000000000000000000  # 5 ETH


# create wallets
echo "-> Creating test users..."
echo "{" > $OUT_FILE
for ((i=1; i<=$NUM_WALLETS; i++)); do
    result=$(wagyu ethereum hd -j)
    address=$(echo "$result" | jq -r '.[0].address')
    mnemonic=$(echo "$result" | jq -r '.[0].mnemonic')
    priv_key=$(echo "$result" | jq -r '.[0].private_key')
    echo "  \"User$i\": {" >> $OUT_FILE
    echo "    \"Address\": \"$address\"," >> $OUT_FILE
    echo "    \"Mnemonic\": \"$mnemonic\"," >> $OUT_FILE
    echo "    \"PrivateKey\": \"0x$priv_key\"" >> $OUT_FILE

    if [ $i -ne $NUM_WALLETS ]; then
        echo "  }," >> $OUT_FILE
    else
        echo "  }" >> $OUT_FILE
    fi
done
echo "}" >> $OUT_FILE
echo "Users information has been saved to $OUT_FILE"

# mint tokens for test users
echo "-> Minting tokens..."
user_count=0
for user in $(jq -r 'keys[]' "$OUT_FILE"); do
    address=$(jq -r ".\"$user\".Address" "$OUT_FILE")
    # send some gas from the master account to the new account
    cast send $address --value $GAS_PER_TEST_ACCOUNT --mnemonic="$ADMIN_WALLET_MNEMONIC" > /dev/null
    amount=$(cast balance $address)
    echo "$user has $amount GWEI"
    for token_address in "$@"; do
        cast send $token_address "mint(address, uint256)(bool)" $address $AMOUNT_TO_MINT --mnemonic="$ADMIN_WALLET_MNEMONIC" > /dev/null
        amount=$(cast call $token_address "balanceOf(address)(uint256)" $address)
        echo "$user has $amount $token_address"
    done
    ((user_count++))
    echo "Minted for $user [$user_count/$NUM_WALLETS]"
done
