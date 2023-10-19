"use client";

import * as React from 'react'
import {
  usePrepareContractWrite,
  useContractWrite,
  useWaitForTransaction,
} from 'wagmi'


export default function Deposit() {
  const {
    config,
    error: prepareError,
    isError: isPrepareError,
  } = usePrepareContractWrite({
    address: '0x9C21AEb2093C32DDbC53eEF24B873BDCd1aDa1DB',  // ERC20Portal address
    abi: [
      {
        name: 'depositERC20Tokens',
        type: 'function',
        stateMutability: 'nonpayable',
        inputs: [
          { internalType: 'address', name: '_token', type: 'address' },
          { internalType: 'address', name: '_dapp', type: 'address' },
          { internalType: 'uint256', name: '_amount', type: 'uint256' },
          { internalType: 'bytes', name: '_execLayerData', type: 'bytes' },
        ],
        outputs: [],
      },
    ],
    functionName: 'depositERC20Tokens',
    args: [
      '0xae7f61eCf06C65405560166b259C54031428A9C4',  // token address, SunodoToken
      '0x70ac08179605AF2D9e75782b8DEcDD3c22aA4D0C',  // dapp address
      '10',  // amount
      '0xabc'  // payload
    ],
  })
  const { data, error, isError, write } = useContractWrite(config)
  const { isLoading, isSuccess } = useWaitForTransaction({
    hash: data?.hash,
  })

  return (
    <div>
      <button disabled={!write || isLoading} onClick={() => write()}>
        {isLoading ? 'Depositing...' : 'Deposit'}
      </button>
      {isSuccess && (<div>Success</div>)}
      {(isPrepareError || isError) && (
        <div>Error: {(prepareError || error)?.message}</div>
      )}
    </div>
  )
}
