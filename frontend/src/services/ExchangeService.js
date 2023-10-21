import axios from 'axios'

class ExchangeService {
  constructor() {
    this.subscribers = []
    this.ws = new WebSocket('wss://your-backend-url.com/websocket-endpoint')
    this.ws.onmessage = this._onMessage.bind(this)
  }

  // Method to get available assets
  async getAvailableAssets() {
    const response = await axios.get('/api/assets')
    return response.data
  }

  // Method to submit an order
  async submitOrder(asset, order) {
    const response = await axios.post(`/api/orders/${asset}`, order)
    if (response.data.matched) {
      this._notifySubscribers()
    }
  }

  // Method to cancel an order
  async cancelOrder(asset, orderId) {
    await axios.delete(`/api/orders/${asset}/${orderId}`)
    this._notifySubscribers()
  }

  // Method to get the order book for a particular asset
  async getOrderBook(asset) {
    const response = await axios.get(`/api/orders/${asset}`)
    return response.data
  }

  // Method to get transactions for a particular asset
  async getTransactions(asset) {
    const response = await axios.get(`/api/transactions/${asset}`)
    return response.data
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

  // Method to get time series data for a particular asset
  async getTimeSeriesData(asset) {
    const response = await axios.get(`/api/time_series_data/${asset}`)
    return response.data
  }

  // Private method to notify subscribers of order book updates
  async _notifySubscribers() {
    // Assume a backend endpoint exists to fetch updated orders and transactions for all assets
    const response = await axios.get('/api/updates')
    const { orders, transactions } = response.data
    this.subscribers.forEach((callback) => callback(orders, transactions))
  }

  // Private method to handle incoming WebSocket messages
  _onMessage(event) {
    const { orders, transactions } = JSON.parse(event.data)
    this.subscribers.forEach((callback) => callback(orders, transactions))
  }
}

export const exchangeService = new ExchangeService()
