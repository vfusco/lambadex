import React from 'react'

const Dashboard = React.lazy(() => import('./views/dashboard/Dashboard'))
const Spot = React.lazy(() => import('./views/trade/Spot'))
const Price = React.lazy(() => import('./views/trade/Price'))
const Wallet = React.lazy(() => import('./views/wallet/Wallet'))

const routes = [
  { path: '/', exact: true, name: 'Home' },
  { path: '/dashboard', name: 'Dashboard', element: Dashboard },
  { path: '/trade/spot', name: 'Spot', element: Spot, exact: true },
  { path: '/trade/price', name: 'Price', element: Price, exact: true },
  { path: '/wallet', name: 'Wallet', element: Wallet, exact: true },
]

export default routes
