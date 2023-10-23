#!/usr/bin/env lua5.4

-- Copyright Cartesi and individual authors (see AUTHORS)
-- SPDX-License-Identifier: LGPL-3.0-or-later
--
-- This program is free software: you can redistribute it and/or modify it under
-- the terms of the GNU Lesser General Public License as published by the Free
-- Software Foundation, either version 3 of the License, or (at your option) any
-- later version.
--
-- This program is distributed in the hope that it will be useful, but WITHOUT ANY
-- WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
-- PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License along
-- with this program (see COPYING). If not, see <https://www.gnu.org/licenses/>.
--

local util = require("cartesi.util")
local json = require("dkjson")

local function stderr(fmt, ...) io.stderr:write(string.format(fmt, ...)) end

-- Print help and exit
local function help()
    stderr(
        [=[
Usage:

  %s [action] [what]

[action] can be "encode" or "decode". When encoding, the utility reads
from stdin a JSON object and writes binary data to stdout. Conversely,
when decoding, the utility reads binary data from stdin and writes a
JSON object to stdout.

[what] can be:

    input-metadata
      the JSON repsentation is
        {
          "msg_sender": <eth-address>,
          "epoch_index": <number>,
          "input_index": <number>,
          "block_number": <number>,
          "time_stamp": <number>
        }

    input
      the JSON representation is
        {"payload": <string> }

    erc20-deposit-input
      the JSON repsentation is
        {
          "status": "successful" | "failed",
          "token": <eth-address>,
          "sender": <eth-address>,
          "amount": <number>,
        }

    lambadex-new-order-input
      the JSON repsentation is
        {
          "symbol": <string>,
          "side": "buy" | "sell",
          "quantity": <number>,
          "price": <number>
        }

    lambadex-cancel-order-input
      the JSON repsentation is
        {
          "id": <number>
        }

    lambadex-withdraw-input
      the JSON repsentation is
        {
          "token": <string>,
          "quantity": <number>
        }

    query
      the JSON representation is
        {"payload": <string> }

    lambadex-book-query
      the JSON representation is
        {
          "symbol": <string>,
          "depth": <number>
        }

    lambadex-wallet-query
      the JSON representation is
        {
          "trader": <eth-address>
        }

    voucher
      the JSON representation is
        {"destination": <eth-address>, "payload": <string>}

    erc20-transfer-voucher
      the JSON representation is
        {
          "token": <eth-address>,
          "destination": <eth-address>,
          "amount": <number>
        }

    notice
      the JSON representation is
        {"payload": <string> }

    lambadex-execution-notice
      the JSON representation is
        {
          "trader": <eth-address>,
          "event": "new-order" | "cancel-order" | "execution" |
                   "rejection-insuficient-funds" | "rejection-invalid-symbol",
          "id": <number>,
          "symbol": <string>,
          "side": "buy" | "sell",
          "quantity": <number>,
          "price": <number>
        }

    lambadex-wallet-notice
      the JSON representation is
        {
          "trader": <eth-address>,
          "token": <eth-address>,
          "direction": "deposit" | "withdraw",
          "quantity": <number>
        }

    report
      the JSON representation is
        {"payload": <string> }

    lambadex-book-report
      the JSON representation is
        {
          "symbol": <string>,
          "entries": [ {
            "trader": <eth-address>,
            "id": <number>,
            "side": "buy" | "sell",
            "quantity": <number>,
            "price": <number>
          }, ... ]
        }

    lambadex-wallet-report
      the JSON representation is
        {
          "entries": [ { "token": <eth-address>, "quantity": <number> }, ... ]
        }

    exception
      the JSON representation is
        {"payload": <string> }

    voucher-hashes
      the JSON representation is
        [ <hash>, <hash>, ... <hash> ]
      (only works for decoding)

    notice-hashes
      the JSON representation is
        [ <hash>, <hash>, ... <hash> ]
      (only works for decoding)

    where field <eth-address> contains a 20-byte ETH address in hex
    (e.g., "0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef").
]=],
        arg[0]
    )
end

local function unhex(hex)
    local invalid
    local data = string.gsub(hex, "(%x%x)", function(c)
        local n = tonumber(c, 16)
        if not n then
            invalid = c
            n = 0
        end
        return string.char(n)
    end)
    if invalid then
        return nil, string.format("'%q' not valid hex", invalid)
    else
        return data
    end
end

local function hex(hash)
    if type(hash) ~= "string" then return nil, "expected string, got " .. type(hash) end
    return (string.gsub(hash, ".", function(c) return string.format("%02x", string.byte(c)) end))
end

local erc20_transfer_bytecode = "\xa9\x05\x9c\xbb\0\0\0\0\0\0\0\0\0\0\0\0"

local from_eth_address = {
    [string.lower("0x9C21AEb2093C32DDbC53eEF24B873BDCd1aDa1DB")] = "erc20-portal",
    [string.lower("0xc6e7DF5E7b4f2A278906862b61205850344D4e7d")] = "ADA",
    [string.lower("0x59b670e9fA9D0A427751Af201D676719a970857b")] = "BNB",
    [string.lower("0x4ed7c70F96B99c776995fB64377f0d4aB3B0e1C1")] = "BTC",
    [string.lower("0x322813Fd9A801c5507c9de605d63CEA4f2CE6c44")] = "CTSI",
    [string.lower("0xa85233C63b9Ee964Add6F2cffe00Fd84eb32338f")] = "DAI",
    [string.lower("0x4A679253410272dd5232B3Ff7cF5dbB88f295319")] = "DOGE",
    [string.lower("0x7a2088a1bFc9d81c55368AE168C2C02570cB814F")] = "SOL",
    [string.lower("0x09635F643e140090A9A8Dcd712eD6285858ceBef")] = "TON",
    [string.lower("0xc5a5C42992dECbae36851359345FE25997F5C42d")] = "USDT",
    [string.lower("0x67d269191c92Caf3cD7723F116c85e6E9bf55933")] = "XRP",
    [string.lower("0x0000000000000000000000000000000000000001")] = "perna",
    [string.lower("0x0000000000000000000000000000000000000002")] = "diego",
    [string.lower("0x0000000000000000000000000000000000000003")] = "victor",
    [string.lower("0x0000000000000000000000000000000000000004")] = "alex"
}

local function hexhash(hash)
    hash = "0x" .. hex(hash)
    return from_eth_address[hash] or hash
end

if not arg[1] then
    help()
    stderr("expected action\n")
    os.exit()
end

if arg[1] == "-h" or arg[1] == "--help" then
    help()
    os.exit()
end

if arg[1] ~= "encode" and arg[1] ~= "decode" then
    help()
    stderr("unexpected action '%s'\n", arg[1])
    os.exit()
end

local action = arg[1]

local what_table = {
    ["exception"] = true,
    ["input-metadata"] = true,
    ["input"] = true,
    ["erc20-deposit-input"] = true,
    ["lambadex-new-order-input"] = true,
    ["lambadex-cancel-order-input"] = true,
    ["lambadex-withdraw-input"] = true,
    ["query"] = true,
    ["lambadex-book-query"] = true,
    ["lambadex-wallet-query"] = true,
    ["voucher"] = true,
    ["erc20-transfer-voucher"] = true,
    ["voucher-hashes"] = true,
    ["notice"] = true,
    ["lambadex-execution-notice"] = true,
    ["lambadex-wallet-notice"] = true,
    ["notice-hashes"] = true,
    ["report"] = true,
    ["lambadex-book-report"] = true,
    ["lambadex-wallet-report"] = true,
}

if not arg[2] then
    help()
    stderr("%s what?\n", arg[1])
    os.exit()
end

if not what_table[arg[2]] then
    help()
    stderr("unexpected what '%s'\n", arg[2])
    os.exit()
end

local what = arg[2]

if arg[3] then error("unexpected option " .. arg[3]) end


local function be256(value)
    return string.rep("\0", 32 - 8) ..  string.pack(">I8", value)
end

local function write_be256(value)
    io.stdout:write(string.rep("\0", 32 - 8))
    io.stdout:write(string.pack(">I8", value))
end

local function errorf(...) error(string.format(...)) end

local function read_json()
    local j, _, e = json.decode(io.read("*a"))
    if not j then error(e) end
    return j
end

local to_eth_address = {
    ["erc20-portal"] = string.lower("0x9C21AEb2093C32DDbC53eEF24B873BDCd1aDa1DB"),
    ["ADA"] = string.lower("0xc6e7DF5E7b4f2A278906862b61205850344D4e7d"),
    ["BNB"] = string.lower("0x59b670e9fA9D0A427751Af201D676719a970857b"),
    ["BTC"] = string.lower("0x4ed7c70F96B99c776995fB64377f0d4aB3B0e1C1"),
    ["CTSI"] = string.lower("0x322813Fd9A801c5507c9de605d63CEA4f2CE6c44"),
    ["DAI"] = string.lower("0xa85233C63b9Ee964Add6F2cffe00Fd84eb32338f"),
    ["DOGE"] = string.lower("0x4A679253410272dd5232B3Ff7cF5dbB88f295319"),
    ["SOL"] = string.lower("0x7a2088a1bFc9d81c55368AE168C2C02570cB814F"),
    ["TON"] = string.lower("0x09635F643e140090A9A8Dcd712eD6285858ceBef"),
    ["USDT"] = string.lower("0xc5a5C42992dECbae36851359345FE25997F5C42d"),
    ["XRP"] = string.lower("0x67d269191c92Caf3cD7723F116c85e6E9bf55933"),
    ["perna"] = string.lower("0x0000000000000000000000000000000000000001"),
    ["diego"] = string.lower("0x0000000000000000000000000000000000000002"),
    ["victor"] = string.lower("0x0000000000000000000000000000000000000003"),
    ["alex"] = string.lower("0x0000000000000000000000000000000000000004")
}

local function unhexhash(addr, name)
    if not addr then errorf("missing %s", name) end
    addr = to_eth_address[addr] or addr
    if string.sub(addr, 1, 2) ~= "0x" then errorf("invalid %s %s (missing 0x prefix)", name, addr) end
    if #addr ~= 42 then errorf("%s must contain 40 hex digits (%s has %g digits)", name, addr, #addr - 2) end
    local bin, err = unhex(string.sub(addr, 3))
    if not bin then errorf("invalid %s %s (%s)", name, addr, err) end
    return bin
end

local decode_erc20_deposit_status_enum = {
    ["\1"] = "successful",
    ["\0"] = "failure"
}

local decode_order_side_enum = {
    ["B"] = "buy",
    ["S"] = "sell"
}

local encode_event_what_enum = {
    ["new-order"] = "N",
    ["cancel-order"] = "C",
    ["execution"] = "E",
    ["rejection-insufficient-funds"] = "R",
    ["rejection-invalid-symbol"] = "r",
}

local decode_event_what_enum = {
    ["N"] = "new-order",
    ["C"] = "cancel-order",
    ["E"] = "execution",
    ["R"] = "rejection-insufficient-funds",
    ["r"] = "rejection-invalid-symbol",
}

local encode_erc20_deposit_status_enum = {
    ["successful"] = "\1",
    ["failure"] = "\0"
}

local encode_order_side_enum = {
    ["buy"] = "B",
    ["sell"] = "S"
}

local encode_wallet_notice_direction_enum = {
    ["deposit"] = "D",
    ["witdraw"] = "W"
}

local function check_enum(enum, options, name)
    if not enum then errorf("missing %s", name) end
    if not type(enum) == "string" then errorf("%s not a %s", name) end
    if not options[enum] then errorf("invalid %s %q", name, enum) end
    return options[enum]
end

local function check_number(number, name)
    if not number then errorf("missing %s", name) end
    number = util.parse_number(number)
    if not number then errorf("invalid %s %s", name, tostring(number)) end
    return number
end

local function encode_input_metadata()
    local j = read_json()
    j.msg_sender = to_eth_address[assert(j.msg_sender, "missing msg_sender")] or j.msg_sender
    j.msg_sender = string.rep("\0", 12) .. unhexhash(j.msg_sender, "msg_sender")
    j.block_number = check_number(j.block_number, "block_number")
    j.time_stamp = check_number(j.time_stamp, "time_stamp")
    j.epoch_index = check_number(j.epoch_index, "epoch_index")
    j.input_index = check_number(j.input_index, "input_index")
    io.stdout:write(j.msg_sender)
    write_be256(j.block_number)
    write_be256(j.time_stamp)
    write_be256(j.epoch_index)
    write_be256(j.input_index)
end

local function encode_voucher()
    local j = read_json()
    local payload = assert(j.payload, "missing payload")
    local destination = string.rep("\0", 12) .. unhexhash(j.destination, "destination")
    io.stdout:write(destination)
    write_be256(64)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function read_address32() return string.sub(assert(io.stdin:read(32)), 13) end
local function read_address20() return assert(io.stdin:read(20)) end

local function read_symbol()
    return string.gsub(string.unpack("c10", assert(io.stdin:read(10))), "\0*$", "")
end

local function read_uint64() return string.unpack("<I8", assert(io.stdin:read(8))) end

local function read_byte() return assert(io.stdin:read(1)) end

local function read_bytecode() return assert(io.stdin:read(16)) end

local function read_hash()
    local s = io.stdin:read(32)
    if s and #s == 32 then return s end
end

local function read_be256() return string.unpack(">I8", string.sub(io.stdin:read(32), 25)) end

local function decode_input_metadata()
    local msg_sender = read_address32()
    local block_number = read_be256()
    local time_stamp = read_be256()
    local epoch_index = read_be256()
    local input_index = read_be256()
    msg_sender = hexhash(msg_sender)
    io.stdout:write(
        json.encode({
            msg_sender = msg_sender,
            block_number = block_number,
            time_stamp = time_stamp,
            epoch_index = epoch_index,
            input_index = input_index,
        }, {
            indent = true,
            keyorder = {
                "msg_sender",
                "block_number",
                "time_stamp",
                "epoch_index",
                "input_index",
            },
        }),
        "\n"
    )
end

local function decode_string()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local payload = length == 0 and "" or assert(io.stdin:read(length))
    io.stdout:write(json.encode({ payload = payload }, { indent = true }), "\n")
end

local function encode_string()
    local j = read_json()
    assert(j.payload, "missing payload")
    write_be256(32)
    write_be256(#j.payload)
    io.stdout:write(j.payload)
end

local function encode_lambadex_execution_notice()
    local j = read_json()
    local payload = table.concat{
        'E',
        unhexhash(j.trader, "trader"),
        check_enum(j.event, encode_event_what_enum, "event"),
        string.pack("<I8", check_number(j.id, "id")),
        string.pack("c10", assert(j.symbol, "missing symbol")),
        check_enum(j.side, encode_order_side_enum, "side"),
        string.pack("<I8", check_number(j.quantity, "quantity")),
        string.pack("<I8", check_number(j.price, "price")),
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_lambadex_wallet_notice()
    local j = read_json()
    local payload = table.concat{
        check_enum(j.direction, encode_wallet_notice_direction_enum, "direction"),
        unhexhash(j.trader, "trader"),
        unhexhash(j.token, "token"),
        string.pack("<I8", check_number(j.quantity, "quantity")),
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_lambadex_book_query()
    local j = read_json()
    local payload = table.concat{
        'B',
        string.pack("c10", assert(j.symbol, "missing symbol")),
        string.pack("<I8", check_number(j.depth, "depth")),
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function decode_lambadex_book_query()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'B', "not a book query")
    local symbol = read_symbol()
    local depth = read_uint64()
    io.stdout:write(
        json.encode({
            symbol = symbol,
            depth = depth,
        }, {
            indent = true,
            keyorder = {
                "symbol",
                "depth",
            },
        }),
        "\n"
    )
end

local function decode_lambadex_book_report()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'B', "not a book report")
    local symbol = read_symbol()
    local entry_count = read_uint64()
    local entries = {}
    for i = 1, entry_count do
        local trader = read_address20()
        local id = read_uint64()
        local side = check_enum(read_byte(), decode_order_side_enum, "side")
        local quantity = read_uint64()
        local price = read_uint64()
        entries[#entries+1] = {
            trader = hexhash(trader),
            id = id,
            side = side,
            quantity = quantity,
            price = price
        }
    end
    io.stdout:write(
        json.encode({
            symbol = symbol,
            entries = entries,
        }, {
            indent = true,
            keyorder = {
                "symbol",
                "entries",
            },
        }),
        "\n"
    )
end

local MAX_BOOK_ENTRY = 64
local function encode_lambadex_book_report()
    local j = read_json()
    assert(type(j.entries) == "table", "missing entries")
    local entry_count = #j.entries
    assert(entry_count <= MAX_BOOK_ENTRY, "too many entries")
    local payload_tab = {
        'B',
        string.pack("c10", assert(j.symbol, "missing symbol")),
        string.pack("<I8", entry_count)
    }
    for _, v in ipairs(j.entries) do
        payload_tab[#payload_tab+1] = unhexhash(v.trader, "trader")
        payload_tab[#payload_tab+1] = string.pack("<I8", check_number(v.id, "id"))
        payload_tab[#payload_tab+1] = check_enum(v.side, encode_order_side_enum, "side")
        payload_tab[#payload_tab+1] = string.pack("<I8", check_number(v.quantity, "quantity"))
        payload_tab[#payload_tab+1] = string.pack("<I8", check_number(v.price, "price"))
    end
    payload_tab[#payload_tab+1] = string.rep("\0", (MAX_BOOK_ENTRY-entry_count)*(1+8+8))
    local payload = table.concat(payload_tab)
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function decode_lambadex_wallet_report()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'W', "not a wallet report")
    local entry_count = read_uint64()
    local entries = {}
    for i = 1, entry_count do
        local token = read_address20()
        local quantity = read_uint64()
        entries[#entries+1] = {
            token = hexhash(token),
            quantity = quantity,
        }
    end
    io.stdout:write(
        json.encode({
            entries = entries,
        }, {
            indent = true,
            keyorder = {
                "entries",
            },
        }),
        "\n"
    )
end

local MAX_WALLET_ENTRY = 16
local function encode_lambadex_wallet_report()
    local j = read_json()
    assert(type(j.entries) == "table", "missing entries")
    local entry_count = #j.entries
    assert(entry_count <= MAX_WALLET_ENTRY, "too many entries")
    local payload_tab = {
        'W',
        string.pack("<I8", entry_count)
    }
    for _, v in ipairs(j.entries) do
        payload_tab[#payload_tab+1] = unhexhash(v.token, "token")
        payload_tab[#payload_tab+1] = string.pack("<I8", check_number(v.quantity, "quantity"))
    end
    payload_tab[#payload_tab+1] = string.rep("\0", (MAX_WALLET_ENTRY-entry_count)*(20+8))
    local payload = table.concat(payload_tab)
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_lambadex_wallet_query()
    local j = read_json()
    local payload = table.concat{
        'W',
        unhexhash(j.trader, "trader")
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function decode_lambadex_wallet_query()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'W', "not a wallet query")
    local trader = read_address20()
    io.stdout:write(
        json.encode({
            trader = hexhash(trader),
        }, {
            indent = true,
            keyorder = {
                "trader"
            },
        }),
        "\n"
    )
end

local function decode_lambadex_execution_notice()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'E', "not an execution notice")
    local trader = read_address20()
    local event = read_byte()
    local id = read_uint64()
    local symbol = read_symbol()
    local side = read_byte()
    local quantity = read_uint64()
    local price = read_uint64()
    io.stdout:write(
        json.encode({
            trader = hexhash(trader),
            event = check_enum(event, decode_event_what_enum, "event"),
            id = id,
            symbol = symbol,
            side = check_enum(side, decode_order_side_enum, "side"),
            quantity = quantity,
            price = price
        }, {
            indent = true,
            keyorder = {
                "trader",
                "event",
                "id",
                "symbol",
                "side",
                "quantity",
                "price"
            },
        }),
        "\n"
    )
end

local function decode_lambadex_wallet_notice()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'W' or what == "D", "not an wallet notice")
    local trader = read_address20()
    local token = read_address20()
    local quantity = read_uint64()
    io.stdout:write(
        json.encode({
            trader = hexhash(trader),
            token = hexhash(token),
            direction = (what == 'W') and "withdraw" or "deposit",
            quantity = quantity,
        }, {
            indent = true,
            keyorder = {
                "trader",
                "token",
                "direction",
                "quantity",
            },
        }),
        "\n"
    )
end

local function decode_voucher()
    local destination = hexhash(read_address32())
    local offset = read_be256()
    assert(offset == 64, "expected offset 64, got " .. offset) -- skip offset
    local length = read_be256()
    local payload = length == 0 and "" or assert(io.stdin:read(length))
    io.stdout:write(
        json.encode({
            destination = destination,
            payload = payload,
        }, {
            indent = true,
            keyorder = {
                "destination",
                "payload",
            },
        }),
        "\n"
    )
end

local function decode_hashes()
    local t = {}
    while 1 do
        local hash = read_hash()
        if not hash then break end
        t[#t + 1] = hexhash(hash)
    end
    io.stdout:write(json.encode(t, { indent = true }), "\n")
end

local function encode_erc20_deposit_input()
    local j = read_json()
    local payload = table.concat{
        check_enum(j.status, encode_erc20_deposit_status_enum, "status"),
        unhexhash(j.token, "token"),
        unhexhash(j.sender, "sender"),
        be256(check_number(j.amount, "amount"))
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_lambadex_new_order_input()
    local j = read_json()
    local payload = table.concat{
        'N',
        string.pack("c10", assert(j.symbol, "missing symbol")),
        check_enum(j.side, encode_order_side_enum, "side"),
        string.pack("<I8", check_number(j.quantity, "quantity")),
        string.pack("<I8", check_number(j.price, "price")),
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_lambadex_withdraw_input()
    local j = read_json()
    local payload = table.concat{
        'W',
        unhexhash(j.token, "token"),
        string.pack("<I8", check_number(j.quantity, "quantity")),
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_lambadex_cancel_order_input()
    local j = read_json()
    local payload = table.concat{
        'C',
        string.pack("<I8", check_number(j.id, "id")),
    }
    write_be256(32)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function encode_erc20_transfer_voucher()
    local j = read_json()
    local token = string.rep("\0", 12) .. unhexhash(j.token, "token")
    io.stdout:write(token)
    local payload = table.concat{
        erc20_transfer_bytecode,
        unhexhash(j.destination, "destination"),
        be256(check_number(j.amount, "amount"))
    }
    write_be256(64)
    write_be256(#payload)
    io.stdout:write(payload)
end

local function decode_erc20_deposit_input()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local status = read_byte()
    local token = read_address20()
    local sender = read_address20()
    local amount = read_be256()
    io.stdout:write(
        json.encode({
            status = check_enum(status, decode_erc20_deposit_status_enum, "status"),
            token = hexhash(token),
            sender = hexhash(sender),
            amount = amount,
        }, {
            indent = true,
            keyorder = {
                "status",
                "token",
                "sender",
                "amount",
            },
        }),
        "\n"
    )
end

local function decode_lambadex_new_order_input()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'N', "not a new order input")
    local symbol = read_symbol()
    local side = read_byte()
    local quantity = read_uint64()
    local price = read_uint64()
    io.stdout:write(
        json.encode({
            symbol = symbol,
            side = check_enum(side, decode_order_side_enum, "side"),
            quantity = quantity,
            price = price,
        }, {
            indent = true,
            keyorder = {
                "symbol",
                "side",
                "quantity",
                "price",
            },
        }),
        "\n"
    )
end

local function decode_lambadex_withdraw_input()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'W', "not a withdraw input")
    local token = read_address20()
    local quantity = read_uint64()
    io.stdout:write(
        json.encode({
            token = hexhash(token),
            quantity = quantity,
        }, {
            indent = true,
            keyorder = {
                "token",
                "quantity"
            },
        }),
        "\n"
    )
end

local function decode_lambadex_cancel_order_input()
    assert(read_be256() == 32) -- skip offset
    local length = read_be256()
    local what = read_byte()
    assert(what == 'C', "not a cancel order input")
    local id = read_uint64()
    io.stdout:write(
        json.encode({
            id = id,
        }, {
            indent = true,
            keyorder = {
                "id"
            },
        }),
        "\n"
    )
end

local function decode_erc20_transfer_voucher()
    local token = read_address32()
    local offset = read_be256()
    assert(offset == 64, "expected offset 64, got " .. offset) -- skip offset
    local length = read_be256()
    local bytecode = read_bytecode()
    assert(bytecode == erc20_transfer_bytecode, "invalid bytecode")
    local destination = read_address20()
    local amount = read_be256()
    io.stdout:write(
        json.encode({
            token = hexhash(token),
            destination = hexhash(destination),
            amount = amount,
        }, {
            indent = true,
            keyorder = {
                "token",
                "destination",
                "amount",
            },
        }),
        "\n"
    )
end

local action_what_table = {
    encode_input_metadata = encode_input_metadata,
    encode_input = encode_string,
    encode_erc20_deposit_input = encode_erc20_deposit_input,
    encode_lambadex_new_order_input = encode_lambadex_new_order_input,
    encode_lambadex_withdraw_input = encode_lambadex_withdraw_input,
    encode_lambadex_cancel_order_input = encode_lambadex_cancel_order_input,
    encode_erc20_transfer_voucher = encode_erc20_transfer_voucher,
    encode_query = encode_string,
    encode_lambadex_wallet_query = encode_lambadex_wallet_query,
    encode_lambadex_book_query = encode_lambadex_book_query,
    encode_voucher = encode_voucher,
    encode_notice = encode_string,
    encode_lambadex_execution_notice = encode_lambadex_execution_notice,
    encode_lambadex_wallet_notice = encode_lambadex_wallet_notice,
    encode_exception = encode_string,
    encode_report = encode_string,
    encode_lambadex_book_report = encode_lambadex_book_report,
    encode_lambadex_wallet_report = encode_lambadex_wallet_report,
    decode_input_metadata = decode_input_metadata,
    decode_input = decode_string,
    decode_erc20_deposit_input = decode_erc20_deposit_input,
    decode_lambadex_new_order_input = decode_lambadex_new_order_input,
    decode_lambadex_withdraw_input = decode_lambadex_withdraw_input,
    decode_lambadex_cancel_order_input = decode_lambadex_cancel_order_input,
    decode_erc20_transfer_voucher = decode_erc20_transfer_voucher,
    decode_query = decode_string,
    decode_lambadex_book_query = decode_lambadex_book_query,
    decode_lambadex_wallet_query = decode_lambadex_wallet_query,
    decode_voucher = decode_voucher,
    decode_notice = decode_string,
    decode_lambadex_execution_notice = decode_lambadex_execution_notice,
    decode_lambadex_wallet_notice = decode_lambadex_wallet_notice,
    decode_exception = decode_string,
    decode_report = decode_string,
    decode_lambadex_book_report = decode_lambadex_book_report,
    decode_lambadex_wallet_report = decode_lambadex_wallet_report,
    decode_voucher_hashes = decode_hashes,
    decode_notice_hashes = decode_hashes,
}

local action_what_todo = action .. "_" .. string.gsub(what, "%-", "_")

local action_what = action_what_table[action_what_todo]

if not action_what then
    help()
    stderr("unexpected action what %s %s\n", action, what)
    os.exit()
end

action_what(arg)
