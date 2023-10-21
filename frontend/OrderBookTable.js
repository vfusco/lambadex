import React from 'react'
import PropTypes from 'prop-types'

function GroupedOrdersTable({ orders }) {
  const groupedOrders = {}

  orders.bids.forEach((order) => {
    const price = Math.round(order.price)
    groupedOrders[price] = groupedOrders[price] || { bids: 0, asks: 0 }
    groupedOrders[price].bids += order.amount
  })

  orders.asks.forEach((order) => {
    const price = Math.round(order.price)
    groupedOrders[price] = groupedOrders[price] || { bids: 0, asks: 0 }
    groupedOrders[price].asks += order.amount
  })

  const sortedPrices = Object.keys(groupedOrders).sort((a, b) => b - a)

  return (
    <table className="grouped-orders-table">
      <thead>
        <tr>
          <th>Number of Bids</th>
          <th>Bid Price</th>
          <th>Ask Price</th>
          <th>Number of Asks</th>
        </tr>
      </thead>
      <tbody>
        {sortedPrices.map((price) => (
          <tr key={price}>
            <td>{groupedOrders[price].bids}</td>
            <td>{groupedOrders[price].bids > 0 ? price : ''}</td>
            <td>{groupedOrders[price].asks > 0 ? price : ''}</td>
            <td>{groupedOrders[price].asks}</td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

GroupedOrdersTable.propTypes = {
  orders: PropTypes.shape({
    bids: PropTypes.arrayOf(
      PropTypes.shape({
        price: PropTypes.number.isRequired,
        amount: PropTypes.number.isRequired,
      }),
    ).isRequired,
    asks: PropTypes.arrayOf(
      PropTypes.shape({
        price: PropTypes.number.isRequired,
        amount: PropTypes.number.isRequired,
      }),
    ).isRequired,
  }).isRequired,
}

export default GroupedOrdersTable
