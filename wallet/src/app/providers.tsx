"use client";

import { WagmiConfig, createConfig, configureChains, mainnet } from 'wagmi'

import { jsonRpcProvider } from 'wagmi/providers/jsonRpc'
import { publicProvider } from 'wagmi/providers/public'
import { foundry } from 'wagmi/chains'

import { MetaMaskConnector } from 'wagmi/connectors/metaMask'

import Profile from './profile'
import Deposit from './deposit'
import Allowance from './allowance'

const { chains, publicClient, webSocketPublicClient } = configureChains(
  [foundry],
  [publicProvider()],
  //[
  //  jsonRpcProvider({
  //    rpc: (chain) => ({
  //      http: `http://localhost:8545`,
  //    }),
  //  }),
  //],
)

const config = createConfig({
  autoConnect: true,
  connectors: [
    new MetaMaskConnector({ chains }),
  ],
  publicClient,
  webSocketPublicClient,
})

export default function Provider() {
  return (
    <WagmiConfig config={config}>
      <Profile />
      <Allowance />
      <Deposit />
    </WagmiConfig>
  )
}
