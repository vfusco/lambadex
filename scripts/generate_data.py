import random
import json

tokens = ["ada", "bnb", "btc", "ctsi", "dai", "doge", "sol", "ton", "usdc", "xrp"]

users = {
    "User1": {token: 10000 for token in tokens},
    "User2": {token: 10000 for token in tokens},
    "User3": {token: 10000 for token in tokens},
    "User4": {token: 10000 for token in tokens}
}

orders = []
for _ in range(600):
    user = random.choice(list(users.keys()))
    symbol = random.sample(tokens, 2)
    side = random.choice(["buy", "sell"])
    price = int(random.uniform(1, 20))

    if side == "buy":
        max_quantity = min(users[user][symbol[1]] // price, random.randint(1, 100))
        if max_quantity <= 0:
            continue
        quantity = random.randint(1, max_quantity)
    else:
        quantity = random.randint(1, 100)

    order = {
        "user": user,
        "symbol": "/".join(symbol),
        "side": side,
        "price": price,
        "quantity": quantity
    }
    orders.append(order)

    if side == "buy":
        users[user][symbol[1]] -= price * quantity
        users[user][symbol[0]] += quantity
    else:
        users[user][symbol[0]] -= quantity
        users[user][symbol[1]] += quantity

for order in orders:
    print(json.dumps(order))
