function toLittleEndianUint8Array(value) {
  const buffer = new ArrayBuffer(8) // Use 8 bytes for 64-bit integer
  const view = new DataView(buffer)
  view.setUint32(0, value, true) // Set true for little endian
  return new Uint8Array(buffer)
}

function encodeBookQuery(jsonObj) {
  const encoder = new TextEncoder()
  const symbolBytes = encoder.encode(jsonObj.symbol)
  if (symbolBytes.length > 10) {
    throw new Error('Symbol string is too long to fit within 10 bytes')
  }
  const paddedSymbolBytes = new Uint8Array(10)
  paddedSymbolBytes.set(symbolBytes)
  const depthBytes = toLittleEndianUint8Array(jsonObj.depth)
  const binaryData = new Uint8Array(19)
  binaryData.set([0x42], 0)
  binaryData.set(paddedSymbolBytes, 1)
  binaryData.set(depthBytes, paddedSymbolBytes.length + 1)
  return binaryData
}

// Decoding function for lambadex-book-report
function decodeBookReport(hexData) {
  // Convert hex string to binary
  const binaryData = new Uint8Array(hexData.match(/.{1,2}/g).map((byte) => parseInt(byte, 16)))
  const decoder = new TextDecoder()
  const symbol = decoder.decode(binaryData.subarray(2, 12)).replace(/\0/g, '') // Removing null characters
  const entryCount = new DataView(binaryData.buffer).getBigUint64(12, true)
  const entriesData = binaryData.subarray(20) // Remaining binary data for entries
  const entries = []
  const maxEntries = entryCount < 200n ? entryCount : 200n
  let offset = 0
  for (let i = 0n; i < maxEntries; i++) {
    const entryData = entriesData.subarray(offset, offset + 45)
    const trader =
      '0x' +
      Array.from(entryData.subarray(0, 20))
        .map((byte) => byte.toString(16).padStart(2, '0'))
        .join('')
    const id = new DataView(entryData.buffer).getBigUint64(20 + offset + 20, true) // Assuming id is 64-bit
    const side = entryData[28] === 66 ? 'buy' : 'sell'
    const quantity = new DataView(entryData.buffer).getBigUint64(20 + offset + 29, true) // Assuming quantity is 64-bit
    const price = new DataView(entryData.buffer).getBigUint64(20 + offset + 37, true) // Assuming price is 64-bit
    entries.push({
      trader,
      id,
      side,
      quantity,
      price,
    })
    offset += 45
  }

  return {
    symbol,
    entries,
  }
}

function encodeWalletQuery(walletQuery) {
  // Extracting the trader Ethereum address from the walletQuery object
  const { trader } = walletQuery
  // Removing the '0x' prefix from the Ethereum address
  const traderStripped = trader.replace(/^0x/, '')
  // Converting the stripped Ethereum address to a binary buffer
  const traderBuffer = Buffer.from(traderStripped, 'hex')
  // Prepending a fixed octet (0x57) to the traderBuffer
  const fixedOctet = Buffer.from([0x57])
  const encodedQuery = Buffer.concat([fixedOctet, traderBuffer])
  // Converting the binary buffer to a Uint8Array and returning it
  return new Uint8Array(encodedQuery)
}

function decodeWalletReport(encodedReport) {
  // Assuming encodedReport is a Buffer or Uint8Array
  let offset = 0
  const entries = []
  while (offset < encodedReport.length) {
    // Each entry has a token (20 bytes) and a quantity (8 bytes)
    const tokenBuffer = encodedReport.slice(offset, offset + 20)
    offset += 20
    const quantityBuffer = encodedReport.slice(offset, offset + 8)
    offset += 8
    // Converting tokenBuffer to Ethereum address format
    const token = '0x' + tokenBuffer.toString('hex')
    // Converting quantityBuffer to a little-endian unsigned 64-bit integer
    const quantity = quantityBuffer.readBigUInt64LE()
    entries.push({ token, quantity: Number(quantity) })
  }
  return { entries }
}

export { encodeBookQuery, decodeBookReport, encodeWalletQuery, decodeWalletReport }
