import random
import json

tokens = ["ADA", "BNB", "BTC", "CTSI", "DAI", "DOGE", "SOL", "TON", "USDT", "XRP"]


token_pairs = ["ADA/USDT", "BNB/USDT", "BTC/USDT", "CTSI/USDT", "DAI/USDT", "DOGE/USDT", "SOL/USDT", "TON/USDT", "XRP/USDT", "ADA/BTC", "BNB/BTC", "CTSI/BTC", "XRP/BTC"]

users = {
    "User1": {token: 10000 for token in tokens},
    "User2": {token: 10000 for token in tokens},
    "User3": {token: 10000 for token in tokens},
    "User4": {token: 10000 for token in tokens}
}

orders = []
for _ in range(600):
    user = random.choice(list(users.keys()))
    symbol = random.choice(token_pairs)
    side = random.choice(["buy", "sell"])
    price = int(random.uniform(1, 20))

    s1, s2 = symbol.split("/")

    if side == "buy":
        max_quantity = min(users[user][s1] // price, random.randint(1, 100))
        if max_quantity <= 0:
            continue
        quantity = random.randint(1, max_quantity)
    else:
        quantity = random.randint(1, 100)

    order = {
        "user": user,
        "symbol": symbol,
        "side": side,
        "price": price,
        "quantity": quantity
    }
    orders.append(order)

    if side == "buy":
        users[user][s1] -= price * quantity
        users[user][s2] += quantity
    else:
        users[user][s1] -= quantity
        users[user][s2] += quantity

for order in orders:
    print(json.dumps(order))
