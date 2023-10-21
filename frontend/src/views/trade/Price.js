import React from 'react'
import ChartComponent from '../../components/ChartComponent'
import { CCard, CCol, CRow } from '@coreui/react'

const data = [
  { time: '2020-05-04', value: 77.85 },
  { time: '2020-05-05', value: 78.92 },
  // ... more data points
]

function Price() {
  return (
    <CRow>
      <CCol xs={12}>
        <CCard className="mb-4">
          <div className="App">
            <ChartComponent data={data} />
          </div>
        </CCard>
      </CCol>
    </CRow>
  )
}

export default Price
