import React from 'react'
import PropTypes from 'prop-types'
import { useExchange } from '../contexts/ExchangeContext'

function OrderBook({ selectedAsset }) {
  const { orders } = useExchange()

  if (!orders[selectedAsset]) return <div>Loading...</div>

  const { bids, asks } = orders[selectedAsset]

  return (
    <div className="order-book">
      <h2>Order Book for {selectedAsset}</h2>
      <table>
        <thead>
          <tr>
            <th>User</th>
            <th>Amount</th>
            <th>Price</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td colSpan="3">Bids</td>
          </tr>
          {bids.slice(0, 10).map((order, index) => (
            <tr key={index}>
              <td>{order.user}</td>
              <td>{order.amount}</td>
              <td>{order.price}</td>
            </tr>
          ))}
          <tr>
            <td colSpan="3">Asks</td>
          </tr>
          {asks.slice(0, 10).map((order, index) => (
            <tr key={index}>
              <td>{order.user}</td>
              <td>{order.amount}</td>
              <td>{order.price}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}

OrderBook.propTypes = {
  selectedAsset: PropTypes.string.isRequired,
}

export default OrderBook
