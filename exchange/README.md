# Toy Exchange

## Build
```
make 
```

## Run and test
```
./exchange --setup-test-fixture --interactive

id  tradr	qty	bid | ask	qty 	tradr	id
4	perna	40	111 | 112	50	diego	5
3	perna	50	110 | 120	100	diego	2
1	perna	100	100 |     	    	    	    

Enter command. Format is: (buy|sell|book) symbol trader price qty
> buy ACME victor 112 30

Execution - Trader: victor Order ID: 6 Execution Type: N Side: B Quantity: 30 Price: 112
Execution - Trader: victor Order ID: 6 Execution Type: E Side: B Quantity: 30 Price: 112
Execution - Trader: diego Order ID: 5 Execution Type: E Side: S Quantity: 30 Price: 112

Enter command. Format is: (buy|sell|book) symbol trader price qty
> book ACME
id  	tradr	qty	bid | ask	qty 	tradr	id
4	perna	40	111 | 112	20	diego	5
3	perna	50	110 | 120	100	diego	2
1	perna	100	100 |     	    	    	    

Enter command. Format is: (buy|sell|book) symbol trader price qty
> 

```
