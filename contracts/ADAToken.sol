// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

import "@openzeppelin/contracts/token/ERC20/ERC20.sol";

contract ADAToken is ERC20 {
    constructor(uint256 initialSupply) ERC20("Cardano token", "ADA") {
        _mint(msg.sender, initialSupply * (10 ** uint256(decimals())));
    }

    function mint(address to, uint256 amount) public {
        _mint(to, amount);
    }
}
