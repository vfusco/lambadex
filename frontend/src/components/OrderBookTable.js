import React from 'react'
import PropTypes from 'prop-types'
import { useExchange } from '../contexts/ExchangeContext'

function OrderBookTable({ selectedAsset }) {
  const { orders } = useExchange()

  if (!orders[selectedAsset]?.bids || !orders[selectedAsset]?.asks) {
    return <div>Loading...</div>
  }

  if (!orders[selectedAsset]?.bids || !orders[selectedAsset]?.asks) {
    return { groupedOrders: { bids: {}, asks: {} }, sortedBidPrices: [], sortedAskPrices: [] }
  }
  const asset_orders = orders[selectedAsset]
  const groups = { bids: {}, asks: {} }

  asset_orders.bids.forEach((order) => {
    const price = order.price.toFixed(2)
    groups.bids[price] = (groups.bids[price] || 0) + order.amount
  })

  asset_orders.asks.forEach((order) => {
    const price = order.price.toFixed(2)
    groups.asks[price] = (groups.asks[price] || 0) + order.amount
  })

  const sortedBidPrices = Object.keys(groups.bids)
    .sort((a, b) => parseFloat(b) - parseFloat(a))
    .slice(0, 10)
  const sortedAskPrices = Object.keys(groups.asks)
    .sort((a, b) => parseFloat(a) - parseFloat(b))
    .slice(0, 10)

  const groupedOrders = {
    bids: Object.fromEntries(sortedBidPrices.map((price) => [price, groups.bids[price]])),
    asks: Object.fromEntries(sortedAskPrices.map((price) => [price, groups.asks[price]])),
  }

  return (
    <table className="order-book-table">
      <thead>
        <tr>
          <th>Bids</th>
          <th>Price</th>
          <th>Price</th>
          <th>Asks</th>
        </tr>
      </thead>
      <tbody>
        {Array.from({ length: 10 }).map((_, index) => (
          <tr key={index}>
            <td>{groupedOrders.bids[sortedBidPrices[index]]}</td>
            <td>{sortedBidPrices[index]}</td>
            <td>{sortedAskPrices[index]}</td>
            <td>{groupedOrders.asks[sortedAskPrices[index]]}</td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

OrderBookTable.propTypes = {
  selectedAsset: PropTypes.string.isRequired,
}

export default OrderBookTable
