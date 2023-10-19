# Toy Exchange

## Build
```
make 
```

## Run initializing state
```
dd if=/dev/zero of=lambda-state  bs=1 count=8192
./exchange --interactive  --init-state --setup-test-fixture  --lambda-state ./lambda-state
```

## Run using existing state
```
./exchange --interactive  --lambda-state ./lambda-state
```

## Run interactive tests
```
./exchange --interactive  --lambda-state ./lambda-state
gold:exchange mpernambuco$ ./exchange --interactive  --init-state --setup-test-fixture  --lambda-state /Users/mpernambuco/ctsi2/hackaton/lambadex/exchange/lambda-state
init state....

> book ctsi/usdc
id  	tradr	qty	bid | ask	qty 	tradr	id
0	perna	40	111 | 112	50	diego	0
0	perna	50	110 | 120	100	diego	0
0	perna	100	100 |     	    	    	    

> wallet perna
ctsi: 1000000
usdc: 980060

> wallet diego
ctsi: 999850
usdc: 1000000

> deposit perna brl 10
> wallet perna
brl: 10
ctsi: 1000000
usdc: 980060


> buy ctsi/brl perna 10 1         
Execution - Trader: perna Order ID: 0 Execution Type: N Side: B Quantity: 1 Price: 10


> book ctsi/brl
id  	tradr	qty	bid | ask	qty 	tradr	id
0	perna	1	10 |     	    	    	    

> sell ctsi/brl diego 10 1
Execution - Trader: diego Order ID: 0 Execution Type: N Side: S Quantity: 1 Price: 10
Execution - Trader: perna Order ID: 0 Execution Type: E Side: B Quantity: 1 Price: 10
Execution - Trader: diego Order ID: 0 Execution Type: E Side: S Quantity: 1 Price: 10
Enter command. Format is: (buy|sell|book) symbol trader price qty

> book ctsi/brl
id  	tradr	qty	bid | ask	qty 	tradr	id

> wallet perna
brl: 0
ctsi: 1000001
usdc: 980060

> wallet diego
brl: 10
ctsi: 999848
usdc: 1000000

```
