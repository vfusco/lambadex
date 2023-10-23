import React, {
  createContext,
  useContext,
  useReducer,
  useEffect,
  useCallback,
  useState,
} from 'react'
import PropTypes from 'prop-types'

const ExchangeContext = createContext()

const initialState = {
  orders: {},
  transactions: {},
  assets: [],
  selectedSymbol: 'CTSI/USDT',
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
    case 'UPDATE_ORDER_BOOK':
      return {
        ...state,
        orders: action.orders,
      }
    case 'SET_SYMBOL':
      return {
        ...state,
        selectedSymbol: action.selectedSymbol,
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

  useEffect(() => {
    if (state.selectedSymbol) {
      const intervalId = setInterval(async () => {
        try {
          const orders = await exchangeService.getOrderBook(state.selectedSymbol)
          dispatch({ type: 'UPDATE_ORDER_BOOK', orders })
        } catch (error) {
          console.error('Error fetching order book:', error)
        }
      }, 1000) // Set interval to 1 second (adjust as needed)

      return () => clearInterval(intervalId)
    }
  }, [exchangeService, state.selectedSymbol, dispatch]) // Added dispatch to dependencies

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

  const selectSymbol = useCallback(
    (symbol) => {
      dispatch({ type: 'SET_SYMBOL', selectedSymbol: symbol }) // Updated to dispatch action
    },
    [dispatch],
  ) // Added dispatch to dependencies

  return (
    <ExchangeContext.Provider value={{ ...state, submitOrder, cancelOrder, selectSymbol }}>
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
