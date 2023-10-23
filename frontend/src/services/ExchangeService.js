import axios from 'axios'
import {
  encodeBookQuery,
  decodeBookReport,
  encodeWalletQuery,
  decodeWalletReport,
} from '../lib/LambadexSerialization'

class ExchangeService {
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
    this.subscribers = []
    //this.ws = new WebSocket('wss://your-backend-url.com/websocket-endpoint')
    //this.ws.onmessage = this._onMessage.bind(this)
  }
  // Method to get available assets
  getAvailableAssets() {
    return this.assets
    //async getAvailableAssets() {
    //const response = await axios.get('/api/assets')
    //return response.data
  }

  startMockOrderStream() {
    this.mockOrderStreamInterval = setInterval(this.updateOrders.bind(this), 1000)
  }

  stopMockOrderStream() {
    clearInterval(this.mockOrderStreamInterval)
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
  async getOrderBook(symbol) {
    // Create the book query object
    const bookQuery = {
      symbol: symbol,
      depth: 10, // Assuming a fixed depth for now
    }
    // Encode the book query object to binary
    const binaryData = encodeBookQuery(bookQuery)
    try {
      // Send a POST request with the binary data in the body
      const response = await axios.post('http://localhost:8080/inspect', binaryData.buffer, {
        headers: {
          'Content-Type': 'application/octet-stream',
        },
        responseType: 'json', // Changed to 'json' as the response is JSON formatted
      })
      // Get the hex-encoded data string from the response
      const hexData = response.data.reports[0].payload
      // Decode the binary response data to a JSON object
      const bookReport = decodeBookReport(hexData)
      return bookReport
    } catch (error) {
      console.error('Error fetching order book:', error)
      throw error // Re-throw the error to be handled by the caller
    }
  }

  // Method to get the wallet report for a particular trader
  async getWallet(trader) {
    // Create the lambadex-wallet-query object
    const walletQuery = {
      trader: trader,
    }
    // Encode the wallet query to binary
    const binaryWalletQuery = encodeWalletQuery(walletQuery)
    try {
      // Send the binary wallet query to the server via a POST request
      const response = await axios.post('http://localhost:8080/inspect', binaryWalletQuery, {
        headers: {
          'Content-Type': 'application/octet-stream', // Set the content type to application/octet-stream for binary data
        },
        responseType: 'arraybuffer', // Set the response type to arraybuffer to receive binary data
      })
      // Decode the binary response to a lambadex-wallet-report object
      return decodeWalletReport(new Uint8Array(response.data))
    } catch (error) {
      console.error('Error fetching wallet report:', error)
      throw error // Re-throw the error to allow further handling
    }
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
