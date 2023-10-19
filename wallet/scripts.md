## Useful params

- Default sunodo wallet address: `0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266`
- Default sunodo wallet mnemonic: `test test test test test test test test test test test junk`

## Send tokens

### ETH

```
cast send 0x7D2c4B415d9F917d6effC7761EAB3849B5EcbF39 --value 100000000000000000 --mnemonic-path=./.mnemonic
```

- `0x7D2c4B415d9F917d6effC7761EAB3849B5EcbF39`: destination
- `100000000000000000`: amount to send
- `.mnemonic` - text file that contains a wallet mnemonic

### ERC20

```
cast send 0xae7f61eCf06C65405560166b259C54031428A9C4 "transfer(address, uint256)(bool)" 0x7D2c4B415d9F917d6effC7761EAB3849B5EcbF39 10000000000000000000000 --mnemonic-path=./.mnemonic
```

- `0xae7f61eCf06C65405560166b259C54031428A9C4`: token contract address (SunodoToken in this case)
- `0x7D2c4B415d9F917d6effC7761EAB3849B5EcbF39`: destination
- `100000000000000000`: amount to send
- `.mnemonic` - text file that contains a wallet mnemonic

## Get balance

```
cast call 0xae7f61eCf06C65405560166b259C54031428A9C4 "balanceOf(address)(uint256)" 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266
```

- `0xae7f61eCf06C65405560166b259C54031428A9C4`: token contract address (SunodoToken in this case)
- `0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266`: wallet to check (default sunodo wallet in this case)

## Get wallet address

```
cast wallet a --mnemonic-path ./.mnemonic
```

- `.mnemonic` - text file that contains a wallet mnemonic
