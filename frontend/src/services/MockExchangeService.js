class MockExchangeService {
  constructor() {
    this.assets = [
      'ADA/USDT',
      'BNB/USDT',
      'BTC/USDT',
      'CTSI/USDT',
      'DAI/USDT',
      'DOGE/USDT',
      'SOL/USDT',
      'TON/USDT',
      'XRP/USDT',
      'ADA/BTC',
      'BNB/BTC',
      'CTSI/BTC',
      'XRP/BTC',
    ]
    this.orders = {} // An object to hold orders for each asset
    this.transactions = {} // An object to hold transactions for each asset
    this.subscribers = [] // Subscribers to order book updates
    this.assets.forEach((asset) => {
      this.orders[asset] = { bids: [], asks: [] }
      this.transactions[asset] = []
    })
    //this.startMockOrderStream()
  }

  // Method to get available assets
  getAvailableAssets() {
    return this.assets
  }

  // Method to submit an order
  submitOrder(asset, order) {
    const newOrder = { ...order, timestamp: new Date().toISOString() }
    if (order.type === 'buy') {
      this.orders[asset].bids.push(newOrder)
    } else {
      this.orders[asset].asks.push(newOrder)
    }
    this._matchOrders()
    this._notifySubscribers()
  }

  // Method to cancel an order
  cancelOrder(asset, orderId) {
    // Assuming each order has a unique id field
    this.orders[asset].bids = this.orders[asset].bids.filter((order) => order.id !== orderId)
    this.orders[asset].asks = this.orders[asset].asks.filter((order) => order.id !== orderId)
    this._notifySubscribers()
  }

  // Method to get the order book for a particular asset
  getOrderBook(asset) {
    this.generateMockOrders()
    return this.orders[asset]
  }

  // Method to get transactions for a particular asset
  getTransactions(asset) {
    return this.transactions[asset]
  }

  // Method to subscribe to order book updates
  subscribe(callback) {
    const subscriptionId = this.subscribers.length
    this.subscribers.push(callback)
    return subscriptionId
  }

  // Method to unsubscribe from order book updates
  unsubscribe(subscriptionId) {
    delete this.subscribers[subscriptionId]
  }

  // Private method to notify subscribers of order book updates
  _notifySubscribers() {
    //console.log('_notifySubscribers called')
    this.subscribers.forEach((callback) => callback(this.orders, this.transactions))
  }

  // Method to generate mock orders
  generateMockOrders() {
    this.assets.forEach((asset) => {
      const randomUserNumber = Math.floor(Math.random() * 100) + 1
      const randomUser = `User${randomUserNumber}`
      const orderType = Math.random() > 0.5 ? 'buy' : 'sell'
      const price = parseFloat((Math.random() * 1000).toFixed(2))
      const amount = Math.floor(Math.random() * 100) + 1
      const timestamp = new Date().toISOString()
      const newOrder = { type: orderType, price, amount, timestamp, user: randomUser }
      this.submitOrder(asset, newOrder)
    })
    this._matchOrders()
  }

  startMockOrderStream() {
    this.mockOrderStreamInterval = setInterval(this.generateMockOrders.bind(this), 1000)
  }

  stopMockOrderStream() {
    clearInterval(this.mockOrderStreamInterval)
  }

  // Method to match orders
  _matchOrders() {
    this.assets.forEach((asset) => {
      const bids = this.orders[asset].bids.sort((a, b) => b.price - a.price)
      const asks = this.orders[asset].asks.sort((a, b) => a.price - b.price)
      while (bids.length > 0 && asks.length > 0 && bids[0].price >= asks[0].price) {
        const matchedPrice = (bids[0].price + asks[0].price) / 2
        const matchedAmount = Math.min(bids[0].amount, asks[0].amount)
        this.transactions[asset].push({
          price: matchedPrice,
          amount: matchedAmount,
          timestamp: new Date().toISOString(),
        })
        bids[0].amount -= matchedAmount
        asks[0].amount -= matchedAmount
        if (bids[0].amount === 0) bids.shift()
        if (asks[0].amount === 0) asks.shift()
      }
      this.orders[asset].bids = bids
      this.orders[asset].asks = asks
    })
    this._notifySubscribers()
  }

  // Method to get time series data for a particular asset
  getTimeSeriesData(asset) {
    return this.transactions[asset].map((transaction) => ({
      time: transaction.timestamp,
      price: transaction.price,
    }))
  }
}

export const exchangeService = new MockExchangeService()
