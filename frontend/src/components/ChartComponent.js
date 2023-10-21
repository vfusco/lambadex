import React, { useEffect, useRef } from 'react'
import { createChart } from 'lightweight-charts'
import PropTypes from 'prop-types'

function ChartComponent({ data }) {
  const chartContainerRef = useRef(null)

  useEffect(() => {
    if (chartContainerRef.current) {
      const chart = createChart(chartContainerRef.current, { width: 400, height: 300 })
      const lineSeries = chart.addLineSeries()
      lineSeries.setData(data)
    }
  }, [data])

  return <div ref={chartContainerRef} />
}

ChartComponent.propTypes = {
  data: PropTypes.arrayOf(
    PropTypes.shape({
      time: PropTypes.string.isRequired,
      value: PropTypes.number.isRequired,
    }),
  ).isRequired,
}

export default ChartComponent
