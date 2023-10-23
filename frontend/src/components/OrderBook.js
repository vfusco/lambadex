import React from 'react'
import { useExchange } from '../contexts/ExchangeContext'
import {
  CTable,
  CTableBody,
  CTableDataCell,
  CTableHead,
  CTableHeaderCell,
  CTableRow,
  CRow,
  CCol,
} from '@coreui/react'

function OrderBook() {
  const { orders } = useExchange()

  if (!orders || !orders?.symbol) return <div>Loading...</div>

  const { symbol, entries } = orders
  const [, quote] = symbol.split('/')
  const bids = entries.filter((entry) => entry.side === 'buy')
  const asks = entries.filter((entry) => entry.side === 'sell')

  return (
    <CRow>
      <CCol sm={6}>
        <div className="text-medium-emphasis">Buy Order</div>
        <CTable align="middle" className="mb-0 border" hover responsive>
          <CTableHead color="light">
            <CTableRow>
              <CTableHeaderCell>Id</CTableHeaderCell>
              <CTableHeaderCell>Price ({quote})</CTableHeaderCell>
              <CTableHeaderCell>Quantity</CTableHeaderCell>
              <CTableHeaderCell>Total ({quote})</CTableHeaderCell>
            </CTableRow>
          </CTableHead>
          <CTableBody>
            {bids.map((order, index) => (
              <CTableRow key={index}>
                <CTableDataCell>{order.id.toString()}</CTableDataCell>
                <CTableDataCell>{order.price.toString()}</CTableDataCell>
                <CTableDataCell>{order.quantity.toString()}</CTableDataCell>
                <CTableDataCell>{(order.price * order.quantity).toString()}</CTableDataCell>
              </CTableRow>
            ))}
          </CTableBody>
        </CTable>
      </CCol>
      <CCol sm={6} className="d-none d-md-block">
        <div className="text-medium-emphasis">Sell Order</div>
        <CTable align="middle" className="mb-0 border" hover responsive>
          <CTableHead color="light">
            <CTableRow>
              <CTableHeaderCell>Id</CTableHeaderCell>
              <CTableHeaderCell>Price ({quote})</CTableHeaderCell>
              <CTableHeaderCell>Quantity</CTableHeaderCell>
              <CTableHeaderCell>Total ({quote})</CTableHeaderCell>
            </CTableRow>
          </CTableHead>
          <CTableBody>
            {asks.map((order, index) => (
              <CTableRow key={index}>
                <CTableDataCell>{order.id.toString()}</CTableDataCell>
                <CTableDataCell>{order.price.toString()}</CTableDataCell>
                <CTableDataCell>{order.quantity.toString()}</CTableDataCell>
                <CTableDataCell>{(order.price * order.quantity).toString()}</CTableDataCell>
              </CTableRow>
            ))}
          </CTableBody>
        </CTable>
      </CCol>
    </CRow>
  )
}

export default OrderBook
