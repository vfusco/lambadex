"use client";

import * as React from 'react'
import {
  usePrepareContractWrite,
  useContractWrite,
  useWaitForTransaction,
} from 'wagmi'


export default function Allowance() {
  const {
    config,
    error: prepareError,
    isError: isPrepareError,
  } = usePrepareContractWrite({
    address: '0xae7f61eCf06C65405560166b259C54031428A9C4',  // SunodoToken address
    abi: [
      {
        name: 'approve',
        type: 'function',
        stateMutability: 'nonpayable',
        inputs: [
          { name: '_spender', type: 'address' },
          { name: '_value', type: 'uint256' },
        ],
        outputs: [
          { type: 'bool' },
        ],
      },
    ],
    functionName: "approve",
    args: [
      '0x9C21AEb2093C32DDbC53eEF24B873BDCd1aDa1DB',  // ERC20Portal address
      100000  // allowance
    ],
  });
  const { data, error, isError, write } = useContractWrite(config)
  const { isLoading, isSuccess } = useWaitForTransaction({
    hash: data?.hash,
  })

  return (
    <div>
      <button disabled={!write || isLoading} onClick={() => write()}>
        {isLoading ? 'Setting allowance...' : 'Set allowance'}
      </button>
      {isSuccess && (<div>Success</div>)}
      {(isPrepareError || isError) && (
        <div>Error: {(prepareError || error)?.message}</div>
      )}
    </div>
  )
}
