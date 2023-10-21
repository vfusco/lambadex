import React from 'react'

const Dashboard = React.lazy(() => import('./views/dashboard/Dashboard'))
const Spot = React.lazy(() => import('./views/trade/Spot'))
const Metamask = React.lazy(() => import('./views/metamask/Metamask'))

const routes = [
  { path: '/', exact: true, name: 'Home' },
  { path: '/dashboard', name: 'Dashboard', element: Dashboard },
  { path: '/trade/spot', name: 'Spot', element: Spot, exact: true },
  { path: '/metamask', name: 'Metamask', element: Metamask, exact: true },
]

export default routes
