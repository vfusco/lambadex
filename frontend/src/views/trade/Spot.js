import React from 'react'
import { CCard, CCardHeader, CCardBody, CRow, CCol, CFormSelect } from '@coreui/react'
import OrderBook from '../../components/OrderBook'
import { ExchangeProvider, useExchange } from '../../contexts/ExchangeContext'
import { exchangeService } from '../../services/ExchangeService'
import OrderBookTable from '../../components/OrderBookTable'

const Spot = () => {
  return (
    <ExchangeProvider exchangeService={exchangeService}>
      <SpotInner />
    </ExchangeProvider>
  )
}

const SpotInner = () => {
  const { assets, selectedSymbol, selectSymbol } = useExchange()

  const handleAssetChange = (event) => {
    selectSymbol(event.target.value)
  }

  return (
    <>
      <CCard className="mb-4">
        <CCardHeader>
          <CRow>
            <CCol sm={9}>
              <h4 id="OrderBook" className="card-title mb-0">
                Order Book
              </h4>
            </CCol>
            <CCol sm={3} className="d-none d-md-block">
              <CFormSelect
                aria-label="Default select example"
                options={assets}
                value={selectedSymbol}
                onChange={handleAssetChange}
              />
            </CCol>
          </CRow>
        </CCardHeader>
        <CCardBody>
          <OrderBook />
        </CCardBody>
      </CCard>
    </>
  )
}

export default Spot
