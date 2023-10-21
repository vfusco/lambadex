import React, { useState } from 'react'
import { CCard, CCardBody, CCol, CRow } from '@coreui/react'
import OrderBook from '../../components/OrderBook'
import { ExchangeProvider } from '../../contexts/ExchangeContext'
import { exchangeService } from '../../services/MockExchangeService'
import OrderBookTable from '../../components/OrderBookTable'

const Spot = () => {
  const [selectedAsset, setSelectedAsset] = useState('ETH') // Default to 'ETH'

  const handleAssetChange = (event) => {
    setSelectedAsset(event.target.value)
  }

  const availableAssets = exchangeService.getAvailableAssets()

  return (
    <CRow>
      <CCol xs={12}>
        <CCard className="mb-4">
          <CCardBody>
            <ExchangeProvider exchangeService={exchangeService}>
              <div className="App">
                <select onChange={handleAssetChange} value={selectedAsset}>
                  {availableAssets.map((asset) => (
                    <option key={asset} value={asset}>
                      {asset}
                    </option>
                  ))}
                </select>
                <OrderBook selectedAsset={selectedAsset} />
                <OrderBookTable selectedAsset={selectedAsset} />
              </div>
            </ExchangeProvider>
          </CCardBody>
        </CCard>
      </CCol>
    </CRow>
  )
}

export default Spot
