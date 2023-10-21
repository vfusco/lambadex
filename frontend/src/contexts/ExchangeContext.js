import React, { createContext, useContext, useReducer, useEffect, useCallback } from 'react'
import PropTypes from 'prop-types'

const ExchangeContext = createContext()

const initialState = {
  orders: {},
  transactions: {},
  assets: [],
}

function reducer(state, action) {
  switch (action.type) {
    case 'UPDATE':
      return {
        ...state,
        orders: action.orders,
        transactions: action.transactions,
      }
    case 'SET_ASSETS':
      return {
        ...state,
        assets: action.assets,
      }
    default:
      throw new Error(`Unknown action type: ${action.type}`)
  }
}

export function ExchangeProvider({ children, exchangeService }) {
  const [state, dispatch] = useReducer(reducer, initialState)

  useEffect(() => {
    const assets = exchangeService.getAvailableAssets()
    dispatch({ type: 'SET_ASSETS', assets })
    const subscriptionId = exchangeService.subscribe((orders, transactions) => {
      dispatch({ type: 'UPDATE', orders, transactions })
    })
    return () => exchangeService.unsubscribe(subscriptionId)
  }, [exchangeService])

  const submitOrder = useCallback(
    async (asset, order) => {
      await exchangeService.submitOrder(asset, order)
    },
    [exchangeService],
  )

  const cancelOrder = useCallback(
    async (asset, orderId) => {
      await exchangeService.cancelOrder(asset, orderId)
    },
    [exchangeService],
  )

  return (
    <ExchangeContext.Provider value={{ ...state, submitOrder, cancelOrder }}>
      {children}
    </ExchangeContext.Provider>
  )
}

ExchangeProvider.propTypes = {
  children: PropTypes.node.isRequired,
  exchangeService: PropTypes.object.isRequired,
}

export function useExchange() {
  return useContext(ExchangeContext)
}
