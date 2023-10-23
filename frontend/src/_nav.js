import React from 'react'
import CIcon from '@coreui/icons-react'
import { cilChart, cilChartPie, cilWallet } from '@coreui/icons'
import { CNavItem, CNavTitle } from '@coreui/react'

const _nav = [
  {
    component: CNavItem,
    name: 'Dashboard',
    to: '/dashboard',
    icon: <CIcon icon={cilChartPie} customClassName="nav-icon" />,
  },
  {
    component: CNavTitle,
    name: 'Trade',
  },
  {
    component: CNavItem,
    name: 'Spot',
    to: '/trade/Spot',
    icon: <CIcon icon={cilChart} customClassName="nav-icon" />,
  },
  /*  {
    component: CNavItem,
    name: 'Price',
    to: '/trade/Price',
    icon: <CIcon icon={cilChart} customClassName="nav-icon" />,
  },
*/ {
    component: CNavItem,
    name: 'Wallet',
    to: '/wallet',
    icon: <CIcon icon={cilWallet} customClassName="nav-icon" />,
  },
]

export default _nav
