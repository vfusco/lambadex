import React, { useEffect, useState } from 'react'
import { useMetaMask } from 'metamask-react'
import { ethers, parseEther, formatEther } from 'ethers'
import { CCol, CRow } from '@coreui/react'

const Wallet = () => {
  const { status, connect, account, chainId } = useMetaMask()
  const [balance, setBalance] = useState(null)
  const [recipient, setRecipient] = useState('')
  const [amount, setAmount] = useState('')

  const handleSend = async () => {
    const provider = new ethers.BrowserProvider(window.ethereum)
    const signer = provider.getSigner()
    const transaction = await signer.sendTransaction({
      to: recipient,
      value: parseEther(amount),
    })
    console.log('Transaction sent: ', transaction)
  }

  useEffect(() => {
    if (status === 'connected') {
      const provider = new ethers.BrowserProvider(window.ethereum)
      // fix this callback
      const fetchBalance = async (provider) => {
        const balance = await provider.getBalance(account)
        setBalance(formatEther(balance))
      }
      fetchBalance(provider)
    }
  }, [status, account])

  return (
    <CRow>
      <CCol xs={12}>
        <div className="MetaMask">
          {status === 'initializing' && <div>Initializing...</div>}
          {status === 'notConnected' && <button onClick={connect}>Connect to MetaMask</button>}
          {status === 'connected' && (
            <div>
              <h1>Connected</h1>
              <p>Account: {account}</p>
              <p>Chain ID {chainId}</p>
            </div>
          )}
          {status === 'connected' && (
            <div>
              {/* ... rest of your connected JSX */}
              <p>Balance: {balance} ETH</p>
              <input
                type="text"
                placeholder="Recipient Address"
                value={recipient}
                onChange={(e) => setRecipient(e.target.value)}
              />
              <input
                type="text"
                placeholder="Amount"
                value={amount}
                onChange={(e) => setAmount(e.target.value)}
              />
              <button onClick={handleSend}>Send</button>
            </div>
          )}
        </div>
      </CCol>
    </CRow>
  )
}

export default Wallet
